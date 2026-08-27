# 变量型数据中心单元测试完善 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将变量模块单元测试从 3 个用例扩展为全面覆盖 8.2 / CONTEXT.md 的分文件 GTest 套件，并通过 `#ifdef DC_TEST` hook 测通恢复与备份路径。

**Architecture:** 在 `dc_variable.c` 末尾添加仅测试编译的 hook；测试按 layout / rw / recovery / backup 四文件拆分，共用 `variable_test_helpers.hpp`；每个 `TEST_F` 前写「测试内容 + 测试步骤」块注释。现有 `variable_test.cpp` 用例迁入对应文件后删除。

**Tech Stack:** C99 库、C++17 测试、Google Test 1.14、CMake、`dc/test/port` 模拟 storage。

**规格:** `docs/superpowers/specs/2026-08-27-variable-unit-tests-design.md`

## Global Constraints

- 仅在 `dc_meter_fw` 与 `dc_tests` 构建时定义 `DC_TEST`；产品固件构建不得定义
- Hook 头文件 `dc/test/dc_test_variable.h` 不得放入 `dc/include/`
- **不修改** `var_backup_power_down` 函数名与实现；测试通过 `extern` 调用
- 每个 `TEST_F` 前必须有块注释：`// 测试内容：` + `// 测试步骤：`（编号步骤）
- Fixture `SetUp()` 必须调用 `DcTestStorageReset()` + `DcTestVarReset()`
- `VAR_EE_BACKUP_BANKS=1` 不单独开 target；PWR_ON_1 用例用 `#if (VAR_EE_BACKUP_BANKS >= 2)` 包裹
- test port 条目数：A=3、B=3、C=19、D=1，合计 `tVariableApiTableCount == 26`
- 成功返回值为写入/读取字节数；错误码：`DC_RET_ALIAS_ERR=-1`、`DC_RET_UNSUPPORTED=-2`、`DC_RET_PARAM_ERR=-3`

## 文件结构

| 路径 | 职责 |
|------|------|
| `dc/test/dc_test_variable.h` | Hook API 声明 |
| `dc/test/variable_test_helpers.hpp` | Seed EE、ReadVar/WriteVar、EE 断言、共用 fixture |
| `dc/test/variable_layout_test.cpp` | 8.2.1 布局与映射表（6 用例） |
| `dc/test/variable_rw_test.cpp` | A/B/C/D 读写与错误码（10 用例） |
| `dc/test/variable_recovery_test.cpp` | 上电/运行中恢复（10 用例） |
| `dc/test/variable_backup_test.cpp` | 备份调度（5 用例） |
| `dc/src/dc_variable.c` | 末尾 `#ifdef DC_TEST` hook 实现 |
| `dc/test/CMakeLists.txt` | 源文件列表、`DC_TEST`、include 路径 |
| `dc/test/variable_test.cpp` | **删除** |

---

### Task 1: DC_TEST Hook 与测试脚手架

**Files:**
- Create: `dc/test/dc_test_variable.h`
- Create: `dc/test/variable_test_helpers.hpp`
- Modify: `dc/src/dc_variable.c`（末尾追加 hook）
- Modify: `dc/test/CMakeLists.txt`

**Interfaces:**
- Consumes: `s_var_ram`、`s_var_inited`、`s_b_dirty`、`s_a_pwr_on_sec`、`s_b_pwr_on_sec`、`s_pwr_dwn_sec`（`dc_variable.c` 内 static）
- Produces: `DcTestVarReset`、`DcTestVarCorruptMagic`、`DcTestVarCorruptCrc`、`DcTestVarInvalidateAll`

- [ ] **Step 1: 创建 `dc/test/dc_test_variable.h`**

```c
#ifndef DC_TEST_VARIABLE_H
#define DC_TEST_VARIABLE_H

#include <stdint.h>

typedef enum {
    DC_TEST_VAR_ZONE_A = 0,
    DC_TEST_VAR_ZONE_B = 1,
} dc_test_var_zone_t;

void DcTestVarReset(void);
void DcTestVarCorruptMagic(dc_test_var_zone_t zone);
void DcTestVarCorruptCrc(dc_test_var_zone_t zone);
void DcTestVarInvalidateAll(dc_test_var_zone_t zone);

#endif
```

- [ ] **Step 2: 在 `dc/src/dc_variable.c` 末尾追加**

```c
#ifdef DC_TEST

void DcTestVarReset(void)
{
    memset(&s_var_ram, 0, sizeof s_var_ram);
    s_var_inited = 0u;
    s_b_dirty = 0u;
    s_a_pwr_on_sec = 0u;
    s_b_pwr_on_sec = 0u;
    s_pwr_dwn_sec = 0u;
}

void DcTestVarCorruptMagic(dc_test_var_zone_t zone)
{
    if (zone == DC_TEST_VAR_ZONE_A) {
        s_var_ram.head_a = 0u;
        s_var_ram.tail_a = 0u;
    } else {
        s_var_ram.head_b = 0u;
        s_var_ram.tail_b = 0u;
    }
}

void DcTestVarCorruptCrc(dc_test_var_zone_t zone)
{
    if (zone == DC_TEST_VAR_ZONE_A) {
        s_var_ram.body_a.crc = 0u;
    } else {
        s_var_ram.body_b.crc = 0u;
    }
}

void DcTestVarInvalidateAll(dc_test_var_zone_t zone)
{
    DcTestVarCorruptMagic(zone);
    DcTestVarCorruptCrc(zone);
}

#endif /* DC_TEST */
```

在文件顶部 `#include` 区追加（仅当编译测试库时需要）：

```c
#ifdef DC_TEST
#include "dc_test_variable.h"
#endif
```

**注意：** `dc_test_variable.h` 在 `dc/test/`，需在 `dc/test/CMakeLists.txt` 对 `dc_meter_fw` 追加 `target_include_directories(dc_meter_fw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})`，否则 `dc_variable.c` 找不到头文件。

- [ ] **Step 3: 创建 `dc/test/variable_test_helpers.hpp`**

```cpp
#pragma once

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "datacenter.h"
#include "dc_crc16.h"
#include "dc_test_storage.h"
#include "dc_test_variable.h"
#include "dc_variable.h"
}

/// @brief 向 A 区 EE 槽写入 CRC 合法块，首数据字节为 first_byte。
inline void SeedAClassEeSlot(E_VARIABLE_EE_SLOT slot, uint8_t first_byte)
{
    uint8_t block[64];
    uint16_t crc;
    std::memset(block, 0, sizeof block);
    block[0] = first_byte;
    crc = dc_crc16_ccitt(block, VAR_A_CRC_ADDR);
    block[VAR_A_CRC_ADDR] = static_cast<uint8_t>(crc & 0xFFu);
    block[VAR_A_CRC_ADDR + 1u] = static_cast<uint8_t>(crc >> 8);
    ASSERT_EQ(VariableEeWriteSlot(slot, block, VAR_A_END_ADDR),
              static_cast<int16_t>(VAR_A_END_ADDR));
}

/// @brief 向 B 区 EE 槽写入 CRC 合法块。
inline void SeedBClassEeSlot(E_VARIABLE_EE_SLOT slot, uint8_t first_byte)
{
    uint8_t block[64];
    uint16_t crc;
    std::memset(block, 0, sizeof block);
    block[0] = first_byte;
    crc = dc_crc16_ccitt(block, VAR_B_CRC_ADDR);
    block[VAR_B_CRC_ADDR] = static_cast<uint8_t>(crc & 0xFFu);
    block[VAR_B_CRC_ADDR + 1u] = static_cast<uint8_t>(crc >> 8);
    ASSERT_EQ(VariableEeWriteSlot(slot, block, VAR_B_END_ADDR),
              static_cast<int16_t>(VAR_B_END_ADDR));
}

inline void SeedAClassEe(uint8_t first_byte)
{
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, first_byte);
}

inline int16_t ReadVar(uint16_t type_id, uint8_t index, uint8_t *buf, uint16_t len)
{
    return dc_read_alias(VarAliasBuild(type_id, index), buf, len, 0u);
}

inline int16_t WriteVar(uint16_t type_id, uint8_t index, const uint8_t *buf, uint16_t len)
{
    return dc_write_alias(VarAliasBuild(type_id, index), buf, len, 0u);
}

inline void ExpectEeSlotFirstByte(E_VARIABLE_EE_SLOT slot, uint8_t expected)
{
    uint8_t block[64];
    ASSERT_EQ(VariableEeReadSlot(slot, block, VAR_A_END_ADDR), static_cast<int16_t>(VAR_A_END_ADDR));
    EXPECT_EQ(block[0], expected);
}

class VariableTestBase : public ::testing::Test {
protected:
    void SetUp() override
    {
        DcTestStorageReset();
        DcTestVarReset();
    }
};
```

- [ ] **Step 4: 更新 `dc/test/CMakeLists.txt`**

```cmake
target_include_directories(dc_meter_fw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(dc_meter_fw PRIVATE DC_TEST)
target_compile_definitions(dc_tests PRIVATE DC_TEST)
target_include_directories(dc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

（此时仍保留 `variable_test.cpp`，下一步迁移后删除。）

- [ ] **Step 5: 构建验证 hook 链接**

Run（在 `dc/cmake-build-debug` 或你的 build 目录）:

```bash
cmake --build . --target dc_meter_fw dc_tests
```

Expected: 链接成功；若报找不到 `dc_test_variable.h`，检查 Step 4 include 路径。

- [ ] **Step 6: Commit**

```bash
git add dc/test/dc_test_variable.h dc/test/variable_test_helpers.hpp dc/src/dc_variable.c dc/test/CMakeLists.txt
git commit -m "test: add DC_TEST variable hooks and shared test helpers"
```

---

### Task 2: 布局测试 `variable_layout_test.cpp`

**Files:**
- Create: `dc/test/variable_layout_test.cpp`
- Modify: `dc/test/CMakeLists.txt`（加入新 cpp，暂保留 `variable_test.cpp`）

**Interfaces:**
- Consumes: `VariableTestBase`、`SeedAClassEe`、`extern tVariableApiTable[]`、`dc_variable_layout.h` 结构体 typedef

- [ ] **Step 1: 创建 `dc/test/variable_layout_test.cpp`（6 个 TEST_F）**

```cpp
#include "variable_test_helpers.hpp"

extern "C" {
#include "dc_variable_layout.h"
}

// 测试内容：EE 槽位地址与 A/B/D 连续映射（8.2.1 + 现有 EeLayout）
// 测试步骤：
//   1. Seed A 区 PWR_ON_0
//   2. 断言 VAR_*_EEPROM_BASE、VariableEeSlotAddr 与 VAR_EE_BACKUP_BANKS 分支
TEST_F(VariableTestBase, EeSlotAddresses) { /* 从 variable_test.cpp EeLayout 迁入 */ }

// 测试内容：layout struct 大小与 VAR_*_END_ADDR 一致
// 测试步骤：
//   1. 比较 sizeof(var_layout_a/b/c/d_t) 与 VAR_A/B/C/D_END_ADDR
TEST_F(VariableTestBase, StructSizesMatchEndAddr)
{
    EXPECT_EQ(sizeof(var_layout_a_t), static_cast<size_t>(VAR_A_END_ADDR));
    EXPECT_EQ(sizeof(var_layout_b_t), static_cast<size_t>(VAR_B_END_ADDR));
    EXPECT_EQ(sizeof(var_layout_c_t), static_cast<size_t>(VAR_C_END_ADDR));
    EXPECT_EQ(sizeof(var_layout_d_t), static_cast<size_t>(VAR_D_END_ADDR));
}

// 测试内容：映射表行数与 cfg 清单条目数一致
// 测试步骤：
//   1. 断言 tVariableApiTableCount == 26
TEST_F(VariableTestBase, ApiTableRowCount)
{
    EXPECT_EQ(tVariableApiTableCount, 26u);
}

// 测试内容：每行偏移+长度不越界
// 测试步骤：
//   1. 遍历 tVariableApiTable
//   2. 按 ucType 选 VAR_A/B/C/D_END_ADDR 上界断言
TEST_F(VariableTestBase, ApiTableOffsetsInBounds) { /* 循环 */ }

// 测试内容：表中 A→B→C→D 分段且 ucType 单调
// 测试步骤：
//   1. 遍历表，记录 ucType 变化点
//   2. 断言顺序为 TYPEA, TYPEB, TYPEC, TYPED 各一段
TEST_F(VariableTestBase, ApiTableTypePartition) { /* 循环 */ }

// 测试内容：EE 区 A→B→D 首尾相接
// 测试步骤：
//   1. 断言 VAR_B_EEPROM_BASE == VAR_A_EEPROM_BASE + VAR_A_EE_TOTAL
//   2. 断言 VAR_D_EEPROM_BASE == VAR_B_EEPROM_BASE + VAR_B_EE_TOTAL
//   3. 断言 VAR_EE_TOTAL == VAR_A_EE_TOTAL + VAR_B_EE_TOTAL + VAR_D_EE_SIZE
TEST_F(VariableTestBase, EeMapContiguous) { /* 见 spec */ }
```

`EeSlotAddresses` 完整断言从现有 `variable_test.cpp:35-53` 复制。

`ApiTableOffsetsInBounds` 参考实现：

```cpp
TEST_F(VariableTestBase, ApiTableOffsetsInBounds)
{
    for (uint16_t i = 0u; i < tVariableApiTableCount; ++i) {
        const ST_DC_VARIABLE_TABLE *row = &tVariableApiTable[i];
        uint16_t limit = 0u;
        switch (row->ucType) {
        case VARIABLE_TYPEA: limit = VAR_A_END_ADDR; break;
        case VARIABLE_TYPEB: limit = VAR_B_END_ADDR; break;
        case VARIABLE_TYPEC: limit = VAR_C_END_ADDR; break;
        case VARIABLE_TYPED: limit = VAR_D_END_ADDR; break;
        default: FAIL() << "unknown ucType";
        }
        EXPECT_LE(static_cast<uint32_t>(row->eVariableAddr) + row->ucLength,
                  static_cast<uint32_t>(limit))
            << "row " << i;
    }
}
```

- [ ] **Step 2: CMake 加入 `variable_layout_test.cpp`**

- [ ] **Step 3: 运行布局用例**

```bash
cmake --build . --target dc_tests
./dc_tests --gtest_filter=VariableTestBase.EeSlotAddresses:VariableTestBase.StructSizesMatchEndAddr:VariableTestBase.ApiTableRowCount:VariableTestBase.ApiTableOffsetsInBounds:VariableTestBase.ApiTableTypePartition:VariableTestBase.EeMapContiguous
```

Expected: 6 PASS

- [ ] **Step 4: Commit**

```bash
git add dc/test/variable_layout_test.cpp dc/test/CMakeLists.txt
git commit -m "test: add variable layout unit tests (8.2.1)"
```

---

### Task 3: 读写与错误码 `variable_rw_test.cpp`

**Files:**
- Create: `dc/test/variable_rw_test.cpp`
- Modify: `dc/test/CMakeLists.txt`

**Interfaces:**
- Consumes: `ReadVar`、`WriteVar`、`VariableTestBase`

- [ ] **Step 1: 创建文件，先写 10 个 TEST_F（含注释）**

核心用例代码片段：

```cpp
// TypeA_ReadWrite — DATE_TIME 7 字节
TEST_F(VariableTestBase, TypeA_ReadWrite)
{
    uint8_t buf[8];
    SeedAClassEe(0x99u);
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x99u);
    buf[0] = 0x26u;
    ASSERT_EQ(WriteVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x26u);
}

// TypeC_NoEeSideEffect — 写 C 类后 EE 快照不变
TEST_F(VariableTestBase, TypeC_NoEeSideEffect)
{
    uint8_t buf[4] = {0x22u, 0x08u};
    uint8_t ee_before[8];
    std::memcpy(ee_before, DcTestStoragePtr() + VAR_A_EEPROM_BASE, sizeof ee_before);
    ASSERT_EQ(WriteVar(VARIABLE_RMS_VOLTAGE, 0, buf, 1u), 2);
    EXPECT_EQ(std::memcmp(ee_before, DcTestStoragePtr() + VAR_A_EEPROM_BASE, sizeof ee_before), 0);
}

// InvalidAlias
TEST_F(VariableTestBase, InvalidAlias)
{
    uint8_t buf[4];
    EXPECT_EQ(ReadVar(0xFFFFu, 0, buf, 1u), DC_RET_ALIAS_ERR);
}

// IndexOutOfRange — RMS_VOLTAGE index=2, usLen=2 越界
TEST_F(VariableTestBase, IndexOutOfRange)
{
    uint8_t buf[8];
    EXPECT_EQ(ReadVar(VARIABLE_RMS_VOLTAGE, 2, buf, 2u), DC_RET_PARAM_ERR);
}

// ZeroLength
TEST_F(VariableTestBase, ZeroLength)
{
    uint8_t buf[4];
    EXPECT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 0u), 0);
}

// NullBufferWithLength
TEST_F(VariableTestBase, NullBufferWithLength)
{
    EXPECT_EQ(dc_read_alias(VarAliasBuild(VARIABLE_DATE_TIME, 0), nullptr, 1u, 0u),
              DC_RET_PARAM_ERR);
}

// InvalidEeSlot
TEST_F(VariableTestBase, InvalidEeSlot)
{
    uint8_t buf[4];
    EXPECT_EQ(VariableEeReadSlot(VAR_EE_SLOT_COUNT, buf, 4u), DC_RET_PARAM_ERR);
    EXPECT_EQ(VariableEeWriteSlot(VAR_EE_SLOT_COUNT, buf, 4u), DC_RET_PARAM_ERR);
}
```

其余用例（`TypeB_ReadWrite`、`TypeC_ReadWriteMultiIndex`、`TypeD_DirectEe`）从现有 `variable_test.cpp:70-112` 拆分迁入，各加块注释。

- [ ] **Step 2: CMake 加入 `variable_rw_test.cpp`**

- [ ] **Step 3: 运行**

```bash
./dc_tests --gtest_filter=VariableTestBase.Type*:VariableTestBase.Invalid*:VariableTestBase.Index*:VariableTestBase.Zero*:VariableTestBase.Null*
```

Expected: 10 PASS

- [ ] **Step 4: Commit**

```bash
git add dc/test/variable_rw_test.cpp dc/test/CMakeLists.txt
git commit -m "test: add variable read/write and error code tests"
```

---

### Task 4: 恢复测试 `variable_recovery_test.cpp`

**Files:**
- Create: `dc/test/variable_recovery_test.cpp`

**Interfaces:**
- Consumes: `DcTestVarCorruptMagic`、`DcTestVarCorruptCrc`、`DcTestVarInvalidateAll`、`SeedAClassEeSlot`、`SeedBClassEeSlot`

- [ ] **Step 1: 上电恢复用例（4 个）**

```cpp
// PwrUpPrefersPwrDwn — 从 variable_test.cpp:55-68 迁入
TEST_F(VariableTestBase, PwrUpPrefersPwrDwn)
{
    uint8_t buf[8];
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0x11u);
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_DWN, 0x99u);
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x99u);
}

// PwrUpFallbackPwrOn0 — 掉电槽 CRC 故意写坏
TEST_F(VariableTestBase, PwrUpFallbackPwrOn0)
{
    uint8_t buf[8];
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0xAAu);
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_DWN, 0xBBu);
    /*  corrupt PWR_DWN CRC */
    DcTestStoragePtr()[VariableEeSlotAddr(VAR_EE_SLOT_A_PWR_DWN) + VAR_A_CRC_ADDR] = 0u;
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0xAAu);
}

#if (VAR_EE_BACKUP_BANKS >= 2)
// PwrUpFallbackPwrOn1
TEST_F(VariableTestBase, PwrUpFallbackPwrOn1) { /* PWR_DWN+PWR_ON_0 CRC 坏，PWR_ON_1=0xCC */ }
#endif
```

- [ ] **Step 2: A 区运行中恢复（4 个）**

```cpp
// RuntimeMagicBadCrcOk
TEST_F(VariableTestBase, RuntimeMagicBadCrcOk)
{
    uint8_t buf[8] = {0x55u};
    SeedAClassEe(0x55u);
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7); /* init + 读 */
    DcTestVarCorruptMagic(DC_TEST_VAR_ZONE_A);
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x55u);
}

// RuntimeCrcBadRestoreFromPwrOn0
TEST_F(VariableTestBase, RuntimeCrcBadRestoreFromPwrOn0)
{
    uint8_t buf[8];
    SeedAClassEeSlot(VAR_EE_SLOT_A_PWR_ON_0, 0x77u);
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    buf[0] = 0x88u;
    ASSERT_EQ(WriteVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    DcTestVarCorruptCrc(DC_TEST_VAR_ZONE_A);
    std::memset(buf, 0, sizeof buf);
    ASSERT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), 7);
    EXPECT_EQ(buf[0], 0x77u); /* 从 EE PWR_ON_0 恢复 */
}

// RuntimeRestoreFails
TEST_F(VariableTestBase, RuntimeRestoreFails)
{
    uint8_t buf[8];
    DcTestVarInvalidateAll(DC_TEST_VAR_ZONE_A);
    EXPECT_EQ(ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u), DC_RET_PARAM_ERR);
}
```

- [ ] **Step 3: B 区镜像 3 用例**

`TypeB_RuntimeMagicBadCrcOk`、`TypeB_RuntimeCrcBadRestore`、`TypeB_RuntimeRestoreFails` — 将 A 区流程改为 `VARIABLE_USED_MONTH`、`SeedBClassEeSlot`、`DC_TEST_VAR_ZONE_B`、`VAR_B_*` 常量。

- [ ] **Step 4: CMake 加入 cpp，运行 recovery 过滤器**

Expected: 全部 PASS（`PwrUpFallbackPwrOn1` 仅在 BANKS>=2 时存在）

- [ ] **Step 5: Commit**

```bash
git commit -m "test: add variable power-up and runtime recovery tests"
```

---

### Task 5: 备份测试 `variable_backup_test.cpp`

**Files:**
- Create: `dc/test/variable_backup_test.cpp`

- [ ] **Step 1: 五个 TEST_F**

```cpp
// TypeA_PeriodicPwrOnBackup
TEST_F(VariableTestBase, TypeA_PeriodicPwrOnBackup)
{
    uint8_t buf[8] = {0x31u};
    SeedAClassEe(0u);
    ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u);
    buf[0] = 0x31u;
    WriteVar(VARIABLE_DATE_TIME, 0, buf, 1u);
    var_backup_tick(VAR_A_BACKUP_INTERVAL_SEC);
    uint8_t ee[32];
    VariableEeReadSlot(VAR_EE_SLOT_A_PWR_ON_0, ee, VAR_A_END_ADDR);
    EXPECT_EQ(ee[0], 0x31u);
#if (VAR_EE_BACKUP_BANKS >= 2)
    VariableEeReadSlot(VAR_EE_SLOT_A_PWR_ON_1, ee, VAR_A_END_ADDR);
    EXPECT_EQ(ee[0], 0x31u);
#endif
}

// TypeB_DirtyGatedBackup
TEST_F(VariableTestBase, TypeB_DirtyGatedBackup)
{
    uint8_t buf[8] = {0x42u};
    SeedAClassEe(0u);
    ReadVar(VARIABLE_DATE_TIME, 0, buf, 1u); /* init */
    var_backup_tick(VAR_B_BACKUP_INTERVAL_SEC); /* 无 B 写，不应备份 B */
    /* 读 B EE 槽应为 0xFF（Reset 后未写入） */
    WriteVar(VARIABLE_USED_MONTH, 0, buf, 1u);
    var_backup_tick(1u); /* 未达间隔 */
    /* 再 tick 满间隔 */
    var_backup_tick(VAR_B_BACKUP_INTERVAL_SEC);
    uint8_t ee[32];
    VariableEeReadSlot(VAR_EE_SLOT_B_PWR_ON_0, ee, VAR_B_END_ADDR);
    EXPECT_EQ(ee[0], 0x42u);
}

// PwrDwnIntervalBackup — var_backup_tick(VAR_PWR_DWN_INTERVAL_SEC)
// ImmediatePowerDown — var_backup_power_down() 后检查 PWR_DWN 槽
// BackupSkippedWhenInvalid — InvalidateAll(A) 后 tick，PWR_ON_0 首字节仍为 seed 值
```

`BackupSkippedWhenInvalid` 参考：

```cpp
TEST_F(VariableTestBase, BackupSkippedWhenInvalid)
{
    uint8_t ee[32];
    SeedAClassEe(0x5Au);
    ReadVar(VARIABLE_DATE_TIME, 0, ee, 1u);
    DcTestVarInvalidateAll(DC_TEST_VAR_ZONE_A);
    var_backup_tick(VAR_A_BACKUP_INTERVAL_SEC);
    VariableEeReadSlot(VAR_EE_SLOT_A_PWR_ON_0, ee, VAR_A_END_ADDR);
    EXPECT_EQ(ee[0], 0x5Au); /* 未覆盖 */
}
```

- [ ] **Step 2: CMake 加入 cpp，运行 backup 过滤器**

- [ ] **Step 3: Commit**

```bash
git commit -m "test: add variable backup scheduling tests"
```

---

### Task 6: 删除旧文件并全量验收

**Files:**
- Delete: `dc/test/variable_test.cpp`
- Modify: `dc/test/CMakeLists.txt`（最终源列表）
- Modify: `docs/superpowers/specs/2026-08-27-variable-unit-tests-design.md`（§3.2 `DcTestVarInvalidateAll` 签名补充 `zone` 参数，与实现一致）

- [ ] **Step 1: 更新 `dc/test/CMakeLists.txt` 最终列表**

```cmake
add_executable(dc_tests
    variable_layout_test.cpp
    variable_rw_test.cpp
    variable_recovery_test.cpp
    variable_backup_test.cpp
    param_test.cpp
)
```

- [ ] **Step 2: 删除 `dc/test/variable_test.cpp`**

- [ ] **Step 3: 全量测试**

```bash
cmake --build . --target dc_tests
ctest --test-dir . -R dc_tests --output-on-failure
```

Expected: 31 变量用例 + param 用例全部 PASS（变量约 6+10+10+5=31）

- [ ] **Step 4: 确认注释规范**

抽查每个 `TEST_F` 前有「测试内容」「测试步骤」块注释。

- [ ] **Step 5: Commit**

```bash
git add -A dc/test docs/superpowers/specs/2026-08-27-variable-unit-tests-design.md
git commit -m "test: complete variable unit test suite; remove legacy variable_test.cpp"
```

---

## Spec 覆盖核对

| Spec §5 用例 | Task |
|--------------|------|
| layout ×6 | Task 2 |
| rw ×10 | Task 3 |
| recovery ×8（B 拆 3） | Task 4 |
| backup ×5 | Task 5 |
| DC_TEST hook | Task 1 |
| CMake / 删除旧文件 | Task 1、6 |
| 用例注释规范 | 全部 Task |

## 执行选项

计划已保存至 `docs/superpowers/plans/2026-08-27-variable-unit-tests.md`。

**1. Subagent-Driven（推荐）** — 每个 Task 派生子 agent，Task 间人工/代理 review  
**2. Inline Execution** — 本会话按 Task 顺序直接实现，checkpoint 处暂停 review

你想用哪种方式开始实现？
