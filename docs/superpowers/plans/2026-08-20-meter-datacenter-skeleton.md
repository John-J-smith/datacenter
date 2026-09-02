# 电表数据中心骨架 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `dc/` 下落地电表专用数据中心骨架：别名读/写分发（空大类入口）+ 由 X-macro 生成的变量参数表；不碰现有 `dc_core` / `dc_meter`。

**Architecture:** `dc_read_alias` / `dc_write_alias` 只做线性查表；各大类独立 `.c` 返回 `DC_RET_UNSUPPORTED`。变量小类、长度、偏移、映射表全部由 `dc/include/dc_variable_table.inc` 展开（偏移用 packed 布局 + `offsetof`）。CMake 目标 `datacenter` 与旧库并存。

**Tech Stack:** C99、CMake、Host `assert` 测试（不链接 `dc_core`）。

**规格:** `docs/superpowers/specs/2026-08-20-meter-datacenter-skeleton-design.md`

## Global Constraints

- 新代码只出现在 `dc/` 与 `tests/host/test_fw_*.c`；不修改 `meter/`、`src/`、`include/dc/` 业务逻辑
- 函数名跟模板；标量用 `stdint.h`；返回 `int16_t`；成功本意为字节数，第一期空入口返回负码
- 别名 `[大类 8 | 小类 16 | 分项 8]`
- `usLen` = 成员个数；`dc_write_alias` 的 `dataPtr` 为 `const uint8_t *`
- 写表无电量；电量写 → `DC_RET_ALIAS_ERR`（-1）
- 空入口不写缓冲，返回 `DC_RET_UNSUPPORTED`（-2）
- `dataPtr == NULL && usLen != 0` → `DC_RET_PARAM_ERR`（-3），在查表之前判定
- 变量表唯一维护点：`dc/include/dc_variable_table.inc`（放 include 以便公开头展开；相对规格里写在 src 的路径，这是为了头文件可 `#include`）
- 不实例化 SRAM、不做 FMT、tick、掉电

## 文件结构

| 路径 | 职责 |
|------|------|
| `dc/include/dc_types.h` | 占位宽度常量 |
| `dc/include/dc_alias.h` | 大类枚举、别名宏 |
| `dc/include/datacenter.h` | 错误码、Read/dc_write_alias |
| `dc/include/dc_variable_table.inc` | 四个 `VAR_LIST_*` X-macro |
| `dc/include/dc_variable.h` | 小类枚举、映射表类型与 `extern` |
| `dc/src/dc_entry.h` | 各大类入口声明（内部） |
| `dc/src/dc_alias.c` | 分发表与 Read/dc_write_alias |
| `dc/src/dc_energy.c` | `dc_read_energy` |
| `dc/src/dc_demand.c` | 需量读/写 |
| `dc/src/dc_param.c` | 参变量读/写 |
| `dc/src/dc_variable.c` | 变量读/写空实现 + 映射表与 CRC 地址常量 |
| `dc/src/dc_list.c` | 列表读/写 |
| `dc/src/dc_record.c` | 记录读/写 |
| `tests/host/test_fw_alias.c` | 分发测试 |
| `tests/host/test_fw_variable_table.c` | 参数表测试 |
| `CMakeLists.txt` | `datacenter` |
| `tests/host/CMakeLists.txt` | 两个新 test |

---

### Task 1: 公开头、CMake 目标、别名宏冒烟

**Files:**
- Create: `dc/include/dc_types.h`
- Create: `dc/include/dc_alias.h`
- Create: `dc/include/datacenter.h`
- Create: `dc/src/dc_alias.c`（空翻译单元占位，下一任务替换）
- Modify: `CMakeLists.txt`
- Create: `tests/host/test_fw_macro.c`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**
- Consumes: 无
- Produces: `ALIAS_CLASS_*`、`GetAliasClass` / `ParaAliasToType` / `GetAliasIndex` / `*AliasBuild`、`DC_RET_*`、库目标 `datacenter`

- [ ] **Step 1: Write the failing test**

Create `tests/host/test_fw_macro.c`:

```c
#include "datacenter.h"
#include "dc_alias.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint32_t a;

    assert(ALIAS_CLASS_ENERGY == 0);
    assert(ALIAS_CLASS_DEMAND == 1);
    assert(ALIAS_CLASS_PARAMETER == 2);
    assert(ALIAS_CLASS_VARIABLE == 3);
    assert(ALIAS_CLASS_LISTPARAM == 4);
    assert(ALIAS_CLASS_RECORD == 5);

    a = VarAliasBuild(0x2000u, 3u);
    assert(GetAliasClass(a) == ALIAS_CLASS_VARIABLE);
    assert(ParaAliasToType(a) == 0x2000u);
    assert(GetAliasIndex(a) == 3u);

    a = EnergyAliasBuild(0x0001u, 0x05u);
    assert(GetAliasClass(a) == ALIAS_CLASS_ENERGY);
    assert(ParaAliasToType(a) == 0x0001u);
    assert(GetAliasIndex(a) == 0x05u);

    assert(DC_RET_ALIAS_ERR == -1);
    assert(DC_RET_UNSUPPORTED == -2);
    assert(DC_RET_PARAM_ERR == -3);

    printf("fw_macro ok\n");
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
cmake -S . -B build-fw
cmake --build build-fw --target test_fw_macro
```

Expected: FAIL — `datacenter.h` / target `test_fw_macro` 不存在。

- [ ] **Step 3: Write headers, stub .c, CMake**

`dc/include/dc_types.h`:

```c
#ifndef DC_TYPES_H
#define DC_TYPES_H

#define DC_VAR_CALENDAR_BYTES         7u
#define DC_VAR_KEY_BYTES              8u
#define DC_VAR_ENERGY_PULSE_BYTES     16u
#define DC_VAR_INTERVAL_PULSE_BYTES   8u

#endif
```

`dc/include/dc_alias.h`:

```c
#ifndef DC_ALIAS_H
#define DC_ALIAS_H

#include <stdint.h>

typedef enum {
    ALIAS_CLASS_ENERGY = 0,
    ALIAS_CLASS_DEMAND = 1,
    ALIAS_CLASS_PARAMETER = 2,
    ALIAS_CLASS_VARIABLE = 3,
    ALIAS_CLASS_LISTPARAM = 4,
    ALIAS_CLASS_RECORD = 5
} E_ALIAS_CLASS;

#define GetAliasClass(a)     ((uint8_t)(((uint32_t)(a) >> 24) & 0xFFu))
#define ParaAliasToType(a)   ((uint16_t)(((uint32_t)(a) >> 8) & 0xFFFFu))
#define GetAliasIndex(a)     ((uint8_t)((uint32_t)(a) & 0xFFu))

#define EnergyAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_ENERGY) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define DemandAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_DEMAND) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define ParaAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_PARAMETER) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define VarAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_VARIABLE) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define ListParaAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_LISTPARAM) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))
#define RecordAliasBuild(t, idx) \
    ((((uint32_t)ALIAS_CLASS_RECORD) << 24) + (((uint32_t)(t)) << 8) + (uint32_t)(idx))

#endif
```

`dc/include/datacenter.h`:

```c
#ifndef DATACENTER_H
#define DATACENTER_H

#include "dc_alias.h"
#include "dc_types.h"
#include <stdint.h>

#define DC_RET_ALIAS_ERR     ((int16_t)-1)
#define DC_RET_UNSUPPORTED   ((int16_t)-2)
#define DC_RET_PARAM_ERR     ((int16_t)-3)

int16_t dc_read_alias(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_alias(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

#endif
```

`dc/src/dc_alias.c`（占位，使库能链上；符号下一任务实现）：

```c
#include "datacenter.h"

int16_t dc_read_alias(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)alias;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}

int16_t dc_write_alias(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)alias;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}
```

In root `CMakeLists.txt`, after `target_compile_definitions(dc_meter ...)` and before `enable_testing()`, add:

```cmake
add_library(datacenter
    dc/src/dc_alias.c
)
target_include_directories(datacenter
    PUBLIC dc/include
    PRIVATE dc/src
)
```

In `tests/host/CMakeLists.txt`, append:

```cmake
add_executable(test_fw_macro test_fw_macro.c)
target_link_libraries(test_fw_macro PRIVATE datacenter)
add_test(NAME fw_macro COMMAND test_fw_macro)
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake -S . -B build-fw
cmake --build build-fw --target test_fw_macro
ctest --test-dir build-fw -R fw_macro --output-on-failure
```

Expected: `fw_macro` PASS，打印 `fw_macro ok`。

- [ ] **Step 5: Commit**

```bash
git add dc/include/dc_types.h dc/include/dc_alias.h dc/include/datacenter.h dc/src/dc_alias.c CMakeLists.txt tests/host/CMakeLists.txt tests/host/test_fw_macro.c
git commit -m "feat: add datacenter headers and alias macros"
```

---

### Task 2: 分发查表与空大类入口

**Files:**
- Create: `dc/src/dc_entry.h`
- Create: `dc/src/dc_energy.c`
- Create: `dc/src/dc_demand.c`
- Create: `dc/src/dc_param.c`
- Create: `dc/src/dc_variable.c`（仅空 Read/dc_write_variable；表在 Task 3）
- Create: `dc/src/dc_list.c`
- Create: `dc/src/dc_record.c`
- Modify: `dc/src/dc_alias.c`
- Modify: `CMakeLists.txt`（把上述 `.c` 加入 `datacenter`）
- Create: `tests/host/test_fw_alias.c`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 `dc_read_alias` / `dc_write_alias` 原型与 `*AliasBuild`
- Produces: 分发表；内部 `dc_read_energy`、`dc_read_demand`、`dc_write_demand`、`dc_read_param`、`dc_write_param`、`dc_read_variable`、`dc_write_variable`、`dc_read_list`、`dc_write_list`、`dc_read_record`、`dc_write_record`

- [ ] **Step 1: Write the failing test**

Create `tests/host/test_fw_alias.c`:

```c
#include "datacenter.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint8_t buf[8];
    int16_t ret;
    uint32_t unknown;
    uint32_t energy;
    uint32_t variable;
    uint32_t demand;

    unknown = ((uint32_t)0x99u << 24);
    energy = EnergyAliasBuild(0x0001u, 0u);
    variable = VarAliasBuild(0x2000u, 0u);
    demand = DemandAliasBuild(0x0001u, 0u);

    ret = dc_read_alias(unknown, buf, 1u, 0u);
    assert(ret == DC_RET_ALIAS_ERR);

    ret = dc_write_alias(unknown, buf, 1u, 0u);
    assert(ret == DC_RET_ALIAS_ERR);

    ret = dc_write_alias(energy, buf, 1u, 0u);
    assert(ret == DC_RET_ALIAS_ERR);

    ret = dc_read_alias(energy, buf, 1u, 0u);
    assert(ret == DC_RET_UNSUPPORTED);

    ret = dc_read_alias(variable, buf, 1u, 0u);
    assert(ret == DC_RET_UNSUPPORTED);

    ret = dc_write_alias(variable, buf, 1u, 0u);
    assert(ret == DC_RET_UNSUPPORTED);

    ret = dc_read_alias(demand, buf, 1u, 0u);
    assert(ret == DC_RET_UNSUPPORTED);

    ret = dc_write_alias(demand, buf, 1u, 0u);
    assert(ret == DC_RET_UNSUPPORTED);

    ret = dc_read_alias(variable, NULL, 1u, 0u);
    assert(ret == DC_RET_PARAM_ERR);

    ret = dc_write_alias(variable, NULL, 1u, 0u);
    assert(ret == DC_RET_PARAM_ERR);

    ret = dc_read_alias(unknown, NULL, 0u, 0u);
    assert(ret == DC_RET_ALIAS_ERR);

    printf("fw_alias ok\n");
    return 0;
}
```

Append to `tests/host/CMakeLists.txt`:

```cmake
add_executable(test_fw_alias test_fw_alias.c)
target_link_libraries(test_fw_alias PRIVATE datacenter)
add_test(NAME fw_alias COMMAND test_fw_alias)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake -S . -B build-fw
cmake --build build-fw --target test_fw_alias
ctest --test-dir build-fw -R fw_alias --output-on-failure
```

Expected: FAIL — `dc_read_alias` 对未知大类仍返回 `DC_RET_UNSUPPORTED`（Task 1 桩），assert 在 `DC_RET_ALIAS_ERR` 处失败。

- [ ] **Step 3: Implement dispatch and empty entries**

`dc/src/dc_entry.h`:

```c
#ifndef DC_ENTRY_H
#define DC_ENTRY_H

#include <stdint.h>

int16_t dc_read_energy(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t dc_read_demand(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_demand(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t dc_read_param(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_param(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t dc_read_variable(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_variable(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t dc_read_list(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_list(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

int16_t dc_read_record(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_record(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);

#endif
```

Each empty `.c` follows the same pattern. `dc/src/dc_energy.c`:

```c
#include "dc_entry.h"
#include "datacenter.h"

int16_t dc_read_energy(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)alias;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}
```

`dc/src/dc_demand.c`:

```c
#include "dc_entry.h"
#include "datacenter.h"

int16_t dc_read_demand(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)alias;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}

int16_t dc_write_demand(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)alias;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}
```

Copy the same two-function body for `dc_param.c` (`dc_read_param` / `dc_write_param`), `dc_variable.c` (`dc_read_variable` / `dc_write_variable`), `dc_list.c` (`dc_read_list` / `dc_write_list`), `dc_record.c` (`dc_read_record` / `dc_write_record`). Do not write to `dataPtr`.

Replace `dc/src/dc_alias.c` entirely:

```c
#include "datacenter.h"
#include "dc_entry.h"

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, uint8_t *, uint16_t, uint8_t);
} STR_ALIAS_RSTORAGE_TABLE;

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, const uint8_t *, uint16_t, uint8_t);
} STR_ALIAS_WSTORAGE_TABLE;

static const STR_ALIAS_RSTORAGE_TABLE readAliasDataTable[] = {
    { (uint8_t)ALIAS_CLASS_ENERGY,    dc_read_energy },
    { (uint8_t)ALIAS_CLASS_DEMAND,    dc_read_demand },
    { (uint8_t)ALIAS_CLASS_PARAMETER, dc_read_param },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  dc_read_variable },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, dc_read_list },
    { (uint8_t)ALIAS_CLASS_RECORD,    dc_read_record },
};

static const STR_ALIAS_WSTORAGE_TABLE writeAliasDataTable[] = {
    { (uint8_t)ALIAS_CLASS_DEMAND,    dc_write_demand },
    { (uint8_t)ALIAS_CLASS_PARAMETER, dc_write_param },
    { (uint8_t)ALIAS_CLASS_VARIABLE,  dc_write_variable },
    { (uint8_t)ALIAS_CLASS_LISTPARAM, dc_write_list },
    { (uint8_t)ALIAS_CLASS_RECORD,    dc_write_record },
};

int16_t dc_read_alias(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    uint8_t class_id;
    uint8_t i;

    if ((dataPtr == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    class_id = GetAliasClass(alias);
    for (i = 0u; i < (uint8_t)(sizeof(readAliasDataTable) / sizeof(readAliasDataTable[0])); i++) {
        if (class_id == readAliasDataTable[i].ucClassId) {
            return readAliasDataTable[i].entry(alias, dataPtr, usLen, type);
        }
    }
    return DC_RET_ALIAS_ERR;
}

int16_t dc_write_alias(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    uint8_t class_id;
    uint8_t i;

    if ((dataPtr == 0) && (usLen != 0u)) {
        return DC_RET_PARAM_ERR;
    }

    class_id = GetAliasClass(alias);
    for (i = 0u; i < (uint8_t)(sizeof(writeAliasDataTable) / sizeof(writeAliasDataTable[0])); i++) {
        if (class_id == writeAliasDataTable[i].ucClassId) {
            return writeAliasDataTable[i].entry(alias, dataPtr, usLen, type);
        }
    }
    return DC_RET_ALIAS_ERR;
}
```

Replace `add_library(datacenter ...)` sources with:

```cmake
add_library(datacenter
    dc/src/dc_alias.c
    dc/src/dc_energy.c
    dc/src/dc_demand.c
    dc/src/dc_param.c
    dc/src/dc_variable.c
    dc/src/dc_list.c
    dc/src/dc_record.c
)
```

Keep the same `target_include_directories`.

- [ ] **Step 4: Run tests to verify they pass**

```bash
cmake -S . -B build-fw
cmake --build build-fw --target test_fw_alias --target test_fw_macro
ctest --test-dir build-fw -R fw_ --output-on-failure
```

Expected: `fw_macro` 与 `fw_alias` 均 PASS。

- [ ] **Step 5: Commit**

```bash
git add dc/src CMakeLists.txt tests/host/test_fw_alias.c tests/host/CMakeLists.txt
git commit -m "feat: alias class dispatch with empty entries"
```

---

### Task 3: 变量 X-macro 参数表

**Files:**
- Create: `dc/include/dc_variable_table.inc`
- Create: `dc/include/dc_variable.h`
- Modify: `dc/src/dc_variable.c`（保留空读写，追加表与布局常量）
- Modify: `dc/include/datacenter.h`（`#include "dc_variable.h"` 以便一个头拿到表）
- Create: `tests/host/test_fw_variable_table.c`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**
- Consumes: `DC_VAR_*_BYTES`、`VAR_LIST_A/B/C/D`
- Produces: `VARIABLE_*` 枚举、`VARIABLE_TYPEA`…`D`、`STR_VARIABLE_API_TABLE`、`tVariableApiTable`、`tVariableApiTableCount`、`VAR_A_CRC_ADDR` / `VAR_A_END_ADDR` / `VAR_B_CRC_ADDR` / `VAR_B_END_ADDR` / `VAR_C_END_ADDR` / `VAR_D_END_ADDR`

- [ ] **Step 1: Write the failing test**

Create `tests/host/test_fw_variable_table.c`:

```c
#include "dc_variable.h"

#include <assert.h>
#include <stdio.h>

static const STR_VARIABLE_API_TABLE *find_id(uint16_t id)
{
    uint16_t i;

    for (i = 0u; i < tVariableApiTableCount; i++) {
        if (tVariableApiTable[i].eVariableType == id) {
            return &tVariableApiTable[i];
        }
    }
    return 0;
}

int main(void)
{
    const STR_VARIABLE_API_TABLE *row;
    const STR_VARIABLE_API_TABLE *prev;
    uint16_t i;
    uint8_t type;

    row = find_id(VARIABLE_RMS_VOLTAGE);
    assert(row != 0);
    assert(row->eVariableType == 0x2000u);
    assert(row->ucIndexNum == 3u);
    assert(row->ucBytes == 2u);
    assert(row->ucLenth == 6u);
    assert(row->ucType == (uint8_t)VARIABLE_TYPEC);
    assert(row->eVariableAddr == 0u);

    row = find_id(VARIABLE_DATE_TIME);
    assert(row != 0);
    assert(row->ucType == (uint8_t)VARIABLE_TYPEA);
    assert(row->eVariableAddr == 0u);
    assert(row->ucLenth == DC_VAR_CALENDAR_BYTES);

    row = find_id(VARIABLE_RTC_SECMIN);
    assert(row != 0);
    assert(row->ucIndexNum == 2u);
    assert(row->ucBytes == 4u);
    assert(row->ucLenth == 8u);

    row = find_id(VARIABLE_MTWORK_EVTKEY);
    assert(row != 0);
    assert(row->ucType == (uint8_t)VARIABLE_TYPED);
    assert(row->eVariableAddr == 0u);
    assert(row->ucLenth == DC_VAR_KEY_BYTES);

    prev = 0;
    for (i = 0u; i < tVariableApiTableCount; i++) {
        row = &tVariableApiTable[i];
        if ((prev != 0) && (prev->ucType == row->ucType)) {
            assert(row->eVariableAddr == (uint16_t)(prev->eVariableAddr + prev->ucLenth));
        }
        if ((prev == 0) || (prev->ucType != row->ucType)) {
            assert(row->eVariableAddr == 0u);
        }
        prev = row;
    }

    prev = 0;
    for (i = 0u; i < tVariableApiTableCount; i++) {
        if (tVariableApiTable[i].ucType == (uint8_t)VARIABLE_TYPEA) {
            prev = &tVariableApiTable[i];
        }
    }
    assert(prev != 0);
    assert(VAR_A_CRC_ADDR == (uint16_t)(prev->eVariableAddr + prev->ucLenth));
    assert(VAR_A_END_ADDR == (uint16_t)(VAR_A_CRC_ADDR + 2u));

    prev = 0;
    for (i = 0u; i < tVariableApiTableCount; i++) {
        if (tVariableApiTable[i].ucType == (uint8_t)VARIABLE_TYPEB) {
            prev = &tVariableApiTable[i];
        }
    }
    assert(prev != 0);
    assert(VAR_B_CRC_ADDR == (uint16_t)(prev->eVariableAddr + prev->ucLenth));
    assert(VAR_B_END_ADDR == (uint16_t)(VAR_B_CRC_ADDR + 2u));

    prev = 0;
    for (i = 0u; i < tVariableApiTableCount; i++) {
        if (tVariableApiTable[i].ucType == (uint8_t)VARIABLE_TYPEC) {
            prev = &tVariableApiTable[i];
        }
    }
    assert(prev != 0);
    assert(VAR_C_END_ADDR == (uint16_t)(prev->eVariableAddr + prev->ucLenth));

    assert(VAR_D_END_ADDR == DC_VAR_KEY_BYTES);

    type = 0u;
    (void)type;
    printf("fw_variable_table ok\n");
    return 0;
}
```

Append:

```cmake
add_executable(test_fw_variable_table test_fw_variable_table.c)
target_link_libraries(test_fw_variable_table PRIVATE datacenter)
add_test(NAME fw_variable_table COMMAND test_fw_variable_table)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake -S . -B build-fw
cmake --build build-fw --target test_fw_variable_table
```

Expected: FAIL — `dc_variable.h` / `tVariableApiTable` 不存在。

- [ ] **Step 3: Write X-macro, header, table implementation**

`dc/include/dc_variable_table.inc`:

```c
#define VAR_LIST_A(X) \
    X(DATE_TIME,     0x4000u, 1u, DC_VAR_CALENDAR_BYTES) \
    X(RUN_TIME,      0xE000u, 4u, 4u) \
    X(WORK_TIME_BAT, 0x2013u, 1u, 4u)

#define VAR_LIST_B(X) \
    X(USED_MONTH,    0x2031u, 1u, 4u) \
    X(ENERGY_DEC,    0xE007u, 1u, DC_VAR_ENERGY_PULSE_BYTES) \
    X(INTVENY_DEC,   0xE008u, 1u, DC_VAR_INTERVAL_PULSE_BYTES)

#define VAR_LIST_C(X) \
    X(RMS_VOLTAGE,      0x2000u, 3u, 2u) \
    X(RMS_CURRENT,      0x2001u, 5u, 4u) \
    X(VOLT_ANGLE,       0x2002u, 3u, 2u) \
    X(PHASE_ANGLE,      0x2003u, 3u, 2u) \
    X(ACTIVE_POWER,     0x2004u, 4u, 4u) \
    X(REACTIVE_POWER,   0x2005u, 4u, 4u) \
    X(APPARENT_POWER,   0x2006u, 4u, 4u) \
    X(ACTPOW_PERMIN,    0x2007u, 4u, 4u) \
    X(REACTPOW_PERMIN,  0x2008u, 4u, 4u) \
    X(POWER_FACT,       0x200Au, 4u, 2u) \
    X(POWER_FREQ,       0x200Fu, 1u, 2u) \
    X(METER_TMP,        0x2010u, 1u, 2u) \
    X(VOLT_BATTIM,      0x2011u, 1u, 2u) \
    X(VOLT_BATDIS,      0x2012u, 1u, 2u) \
    X(STAWDS_METER,     0x2014u, 7u, 2u) \
    X(STAWDS_FLRPT,     0x2015u, 1u, 4u) \
    X(DEMAND_ACTIVE,    0x2017u, 1u, 4u) \
    X(DEMAND_REACTIVE,  0x2018u, 1u, 4u) \
    X(WORK_FEENO,       0xE003u, 1u, 1u) \
    X(RTC_SECMIN,       0xE205u, 2u, 4u)

#define VAR_LIST_D(X) \
    X(MTWORK_EVTKEY, 0xE100u, 1u, DC_VAR_KEY_BYTES)
```

`dc/include/dc_variable.h`:

```c
#ifndef DC_VARIABLE_H
#define DC_VARIABLE_H

#include "dc_types.h"
#include "dc_variable_table.inc"
#include <stdint.h>

typedef enum {
    VARIABLE_TYPEA = 0,
    VARIABLE_TYPEB = 1,
    VARIABLE_TYPEC = 2,
    VARIABLE_TYPED = 3
} E_VARIABLE_STOR_TYPE;

#define VAR_ENUM_ROW(tok, id, n, b) VARIABLE_##tok = (id),

typedef enum {
    VAR_LIST_A(VAR_ENUM_ROW)
    VAR_LIST_B(VAR_ENUM_ROW)
    VAR_LIST_C(VAR_ENUM_ROW)
    VAR_LIST_D(VAR_ENUM_ROW)
    VARIABLE_ID_SENTINEL = 0
} E_VARIABLE_ID;

#undef VAR_ENUM_ROW

typedef struct {
    uint16_t eVariableType;
    uint16_t eVariableAddr;
    uint16_t ucLenth;
    uint8_t  ucIndexNum;
    uint8_t  ucBytes;
    uint8_t  ucType;
} STR_VARIABLE_API_TABLE;

extern const STR_VARIABLE_API_TABLE tVariableApiTable[];
extern const uint16_t tVariableApiTableCount;

extern const uint16_t VAR_A_CRC_ADDR;
extern const uint16_t VAR_A_END_ADDR;
extern const uint16_t VAR_B_CRC_ADDR;
extern const uint16_t VAR_B_END_ADDR;
extern const uint16_t VAR_C_END_ADDR;
extern const uint16_t VAR_D_END_ADDR;

#endif
```

`VARIABLE_ID_SENTINEL` exists only so the enum is not empty if lists were blank; keep it.

Add to `datacenter.h` after other includes:

```c
#include "dc_variable.h"
```

Replace `dc/src/dc_variable.c` with empty entries **plus** table. Use `#pragma pack` if the compiler is MSVC; on GCC/Clang use `__attribute__((packed))`. Prefer portable:

```c
#include "dc_entry.h"
#include "datacenter.h"
#include "dc_variable.h"

#include <stddef.h>

#pragma pack(push, 1)
#define VAR_FIELD(tok, id, n, b) uint8_t tok[(size_t)(n) * (size_t)(b)];

typedef struct {
    VAR_LIST_A(VAR_FIELD)
} var_layout_a_t;

typedef struct {
    VAR_LIST_B(VAR_FIELD)
} var_layout_b_t;

typedef struct {
    VAR_LIST_C(VAR_FIELD)
} var_layout_c_t;

typedef struct {
    VAR_LIST_D(VAR_FIELD)
} var_layout_d_t;

#undef VAR_FIELD
#pragma pack(pop)

#define VAR_ROW_A(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_a_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPEA },
#define VAR_ROW_B(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_b_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPEB },
#define VAR_ROW_C(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_c_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPEC },
#define VAR_ROW_D(tok, id, n, b) \
    { (uint16_t)(id), (uint16_t)offsetof(var_layout_d_t, tok), \
      (uint16_t)((n) * (b)), (uint8_t)(n), (uint8_t)(b), (uint8_t)VARIABLE_TYPED },

const STR_VARIABLE_API_TABLE tVariableApiTable[] = {
    VAR_LIST_A(VAR_ROW_A)
    VAR_LIST_B(VAR_ROW_B)
    VAR_LIST_C(VAR_ROW_C)
    VAR_LIST_D(VAR_ROW_D)
};

const uint16_t tVariableApiTableCount =
    (uint16_t)(sizeof(tVariableApiTable) / sizeof(tVariableApiTable[0]));

const uint16_t VAR_A_CRC_ADDR = (uint16_t)sizeof(var_layout_a_t);
const uint16_t VAR_A_END_ADDR = (uint16_t)(sizeof(var_layout_a_t) + 2u);
const uint16_t VAR_B_CRC_ADDR = (uint16_t)sizeof(var_layout_b_t);
const uint16_t VAR_B_END_ADDR = (uint16_t)(sizeof(var_layout_b_t) + 2u);
const uint16_t VAR_C_END_ADDR = (uint16_t)sizeof(var_layout_c_t);
const uint16_t VAR_D_END_ADDR = (uint16_t)sizeof(var_layout_d_t);

int16_t dc_read_variable(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)alias;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}

int16_t dc_write_variable(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type)
{
    (void)alias;
    (void)dataPtr;
    (void)usLen;
    (void)type;
    return DC_RET_UNSUPPORTED;
}
```

If `#pragma pack` is rejected on the compiler used in CI, switch the four structs to:

```c
typedef struct {
    VAR_LIST_A(VAR_FIELD)
} __attribute__((packed)) var_layout_a_t;
```

and drop the pragma. MinGW/GCC accepts `__attribute__((packed))`.

- [ ] **Step 4: Run all fw tests**

```bash
cmake -S . -B build-fw
cmake --build build-fw --target test_fw_macro --target test_fw_alias --target test_fw_variable_table
ctest --test-dir build-fw -R fw_ --output-on-failure
```

Expected: 三个 `fw_*` 测试 PASS。`dc_read_alias(VarAliasBuild(VARIABLE_RMS_VOLTAGE, 0), …)` 仍为 `DC_RET_UNSUPPORTED`。

- [ ] **Step 5: Commit**

```bash
git add dc/include/dc_variable_table.inc dc/include/dc_variable.h dc/include/datacenter.h dc/src/dc_variable.c tests/host/test_fw_variable_table.c tests/host/CMakeLists.txt
git commit -m "feat: generate variable parameter table from X-macros"
```

---

## Self-review

**Spec coverage**
- 目录与对外原型 → Task 1–2
- 分发、写表无电量、空入口、错误码、NULL 检查 → Task 2
- X-macro、四块、offsetof、CRC 槽、D 独立、7.4.2 表项、占位宽度 → Task 3
- Host 测试不链 `dc_core` → 三个 `test_fw_*` 只链 `datacenter`
- 不做 SRAM 实例 / 真读写 → 无对应实现任务

**Path note:** 规格写 `dc/src/dc_variable_table.inc`；本计划改为 `dc/include/dc_variable_table.inc`，否则公开头无法单点维护。

**Type consistency:** `int16_t` 返回值、`STR_VARIABLE_API_TABLE` 字段名与测试一致；`eVariableType` 存 16 位小类号。
