# 数据中心模块单元测试规范

本文档约定 `dc/test` 下 Google Test 用例的写法、目录结构与验收方式。设计背景见 `docs/superpowers/specs/2026-08-27-variable-unit-tests-design.md`；行为对齐详细设计 **§8.2** 与 `CONTEXT.md` 中的变量/参数策略。

---

## 1. 目标与边界

| 项 | 要求 |
|----|------|
| 框架 | Google Test，构建目标 `dc_tests` |
| 覆盖 | 布局（8.2.1）、类型读写（8.2.2）、恢复链、备份调度、错误码 |
| 隔离 | 用例任意顺序执行、重复运行结果一致 |
| 产品固件 | **不得**定义 `DC_TEST`，无 test hook 开销 |
| 测试构建 | `datacenter` 与 `dc_tests` **均**定义 `DC_TEST` |

---

## 2. 目录结构

```
dc/test/
  README.md                    # 本规范
  CMakeLists.txt
  dc_test_variable.h           # 变量 test hook 声明（不进 dc/include/）
  variable_test_helpers.hpp  # 变量 helper + VariableTestBase
  param_test_helpers.hpp     # 参数 helper + ParamTestBase
  variable_layout_test.cpp   # 8.2.1 布局与映射表
  variable_rw_test.cpp       # 8.2.2 读写与错误码
  variable_recovery_test.cpp # 上电 / 运行中恢复
  variable_backup_test.cpp   # 定时 / 掉电备份
  param_layout_test.cpp      # 参数 layout / block 表
  param_rw_test.cpp          # 参数读写（经别名）
  port/                      # 测试用 cfg、layout、storage 模拟
    dc_*_cfg.h
    dc_*_layout.h
    dc_alias_layout.h
    dc_storage_sim.c
    dc_test_storage.h
```

**按关注点拆文件**，不要把 layout、rw、recovery、backup 混在同一源文件中。新增模块测试时沿用同一模式：`xxx_layout_test.cpp` + `xxx_rw_test.cpp` + `xxx_test_helpers.hpp`。

---

## 3. 用例注释（必须）

每个 `TEST_F` 前写一行测试目的；步骤用编号注释内联在代码中：

```cpp
// 测试内容：A 类定时备份到 PWR_ON 槽（CONTEXT EE 备份）
TEST_F(VariableTestBase, TypeA_PeriodicPwrOnBackup)
{
    // 1. 写 DATE_TIME
    ...
    // 2. var_backup_tick(VAR_A_BACKUP_INTERVAL_SEC)
    ...
    // 3. 读 PWR_ON_0/1 首字节断言
    ...
}
```

- **`// 测试内容：`** — 验证什么，可引用 `8.2.1`、`8.2.2`、`CONTEXT` 等
- **`// 1.` `// 2.` …** — Arrange / Act / Assert，写在对应代码上方
- **Helper** — 用 `/// @brief` 一行说明；不在每个用例里重复 helper 文档

---

## 4. Fixture 与状态隔离

### 4.1 变量类

继承 `VariableTestBase`，`SetUp` 固定为：

```cpp
DcTestStorageReset();  // EE 模拟区复位
DcTestVarReset();      // SRAM、计时器、dirty 清零
```

需要完整上电链时，在用例内调用 `InitVariableModule()`（seed A 区 EE + 首次读触发 `var_ensure_init`）。

### 4.2 参数类

继承 `ParamTestBase`（当前无额外 SetUp；若后续依赖 storage 再扩展）。

### 4.3 Layout 测试

仅断言生成常量与 struct / 映射表一致性，一般不依赖 EE 模拟，可使用独立 `TEST_F(ParamLayoutTest, …)` 或不继承变量 Fixture。

---

## 5. API 入口：经 alias 层

| 模块 | 推荐入口 |
|------|----------|
| 变量 | `ReadVar` / `WriteVar`（`variable_test_helpers.hpp`）→ `VarAliasBuild` + `dc_read_alias` / `dc_write_alias` |
| 参数 | `dc_read_alias` / `dc_write_alias` + `ParaAliasBuild` 或 `DC_ALIAS_PARAM_*`（`port/dc_alias_layout.h`） |

错误码、空指针等边界用例在 **alias 层**直接调用 `dc_read_alias` / `dc_write_alias`。

### 返回值约定

- **成功**：返回值为 `int16_t` 实际传输**字节数**（`usLen * ucBytes` 或全量 `VAR_INDEX_ALL` / `PARAM_INDEX_ALL` 展开后的字节数）
- **失败**：`DC_RET_ALIAS_ERR`、`DC_RET_PARAM_ERR`、`DC_RET_UNSUPPORTED` 等

### 断言习惯

- 前置条件、必须成功的步骤：`ASSERT_EQ` / `ASSERT_NE`
- 结果检查：`EXPECT_EQ` / `EXPECT_NE`
- 表驱动遍历：配合 `SCOPED_TRACE`（见 `TraceVarEntry` / `TraceParamEntry`）

---

## 6. 公共 Helper

### 6.1 变量（`variable_test_helpers.hpp`）

| Helper | 用途 |
|--------|------|
| `SeedAClassEeSlot` / `SeedBClassEeSlot` | 向 EE 槽写入 CRC 合法块 |
| `SeedAClassEe` | 默认写 A 区 PWR_ON_0 |
| `MakeVarIoBuffer` | 按映射表最大元素字节分配 buffer |
| `FillVarWritePattern` | 可识别的写图案，便于 memcmp |
| `ExpectEeSlotFirstByte` | EE 槽首字节断言 |
| `ExpectZoneBodyCrcOk` | A/B 区 body CRC 合法 |
| `ReadVar` / `WriteVar` | 变量读写薄封装 |

### 6.2 参数（`param_test_helpers.hpp`）

| Helper | 用途 |
|--------|------|
| `MakeParamIoBuffer` / `FillParamWritePattern` | buffer 与写图案 |
| `ParamIndexCount` / `ParamElemBytes` | 经 `dc_param_attr.h` 读映射表元数据 |

**原则**：可复用逻辑放进 helper；用例本体只保留 Arrange / Act / Assert。

---

## 7. `DC_TEST` Hook（仅变量恢复 / 备份）

| 项 | 位置 |
|----|------|
| 声明 | `dc/test/dc_test_variable.h` |
| 实现 | `dc/src/dc_variable.c` 末尾 `#ifdef DC_TEST` |

| 函数 | 行为 |
|------|------|
| `DcTestVarReset` | 清空 SRAM、init 标志、备份计时器 |
| `DcTestVarCorruptMagic` | 仅破坏 head/tail magic |
| `DcTestVarCorruptCrc` | 仅破坏 body CRC |
| `DcTestVarInvalidateAll` | magic + CRC 全坏 |
| `DcTestVarBodyCrcOk` | 查询 body CRC 是否合法 |
| `DcTestVarResetBackupTimers` | 重置备份计时 |

生产函数（如 `var_backup_power_down`）**不改名、不为测试改签名**；测试侧按需 `extern` 声明后直接调用。

### 恢复路径触发方式

- **上电恢复**：`DcTestVarReset` → seed EE → 首次 `dc_read_alias`
- **运行中恢复**：正常读写建立 SRAM → `DcTestVarCorrupt*` → 再次读 → 触发 `var_*_prepare_access`

---

## 8. 条件编译

双备份相关断言包在：

```cpp
#if (VAR_EE_BACKUP_BANKS >= 2)
    // PWR_ON_1、FallbackPwrOn1 等
#endif
```

不为 `VAR_EE_BACKUP_BANKS=1` 单独开 CMake target。

---

## 9. 表驱动与特殊 index

### 9.1 全表读写

遍历 `tVariableApiTable` / `tParamApiTable`，对每个变量的每个 `index` 写后再读回，`memcmp` 比对。

### 9.2 `index = 0xFF`（读/写全部元素）

| 模块 | 宏 | 语义 |
|------|-----|------|
| 变量 | `VAR_INDEX_ALL` | `usLen` 被库展开为 `ucIndexNum` |
| 参数 | `PARAM_INDEX_ALL` | 同上 |

调用方 buffer 长度须 ≥ `ucIndexNum * ucBytes`（或参数侧 `ucParamLen`）。

---

## 10. Port 与存储模拟

- 测试 cfg / layout：`dc/test/port/`（CMake 变量 `DC_PORT_DIR`）
- EE 模拟：`port/dc_storage_sim.c` 实现 `DcCfgStorageRead` / `DcCfgStorageWrite`
- **`dc_storage_cfg.h` 须 pack 安全**：仅地址宏与函数声明，**不在头文件中** `#include` 产品 HAL；驱动头文件只出现在 port 的 `.c` 中

Layout 生成见 `dc/tools/CMakeLists.txt` 与 `dc/tools/gen_layouts.cmd`。

---

## 11. CMake 与运行

`dc/test/CMakeLists.txt` 注册源文件示例：

```cmake
add_executable(dc_tests
    variable_layout_test.cpp
    variable_rw_test.cpp
    variable_recovery_test.cpp
    variable_backup_test.cpp
    param_rw_test.cpp
    param_layout_test.cpp
)
target_compile_definitions(datacenter PRIVATE DC_TEST)
target_compile_definitions(dc_tests PRIVATE DC_TEST)
target_link_libraries(dc_tests PRIVATE datacenter dc_test_port GTest::gtest_main)
```

构建与运行：

```bash
cmake -S dc -B dc/build
cmake --build dc/build --target dc_tests
./dc/build/test/dc_tests          # 或 ctest --test-dir dc/build
```

新增 `*_test.cpp` 后必须加入 `add_executable(dc_tests …)`。

---

## 12. 禁止事项

- 在产品代码中为测试增加非 `#ifdef DC_TEST` 的分支
- 用 magic number 代替 `DC_RET_*` 或 layout 生成常量
- 编写只断言恒真、不覆盖真实行为的用例
- 在 layout 测试中依赖未文档化的实现细节
- 把仅用于 pack 的头文件链拉进含 HAL 的 include（导致 host 工具编译失败）
- 为通过测试而修改生产 API 名称或语义

---

## 13. 新增用例检查清单

- [ ] 文件归属正确（layout / rw / recovery / backup / param）
- [ ] `// 测试内容：` 与编号步骤齐全
- [ ] 使用对应 `*TestBase` 与 helper，无重复样板代码
- [ ] 经 `dc_read_alias` / `dc_write_alias`（或已封装的 ReadVar / WriteVar）
- [ ] 恢复 / 备份用例按需使用 `DC_TEST` hook
- [ ] `VAR_EE_BACKUP_BANKS` 条件编译正确
- [ ] 已加入 `CMakeLists.txt` 且本地 `dc_tests` 全绿

---

## 14. 参考

- `docs/superpowers/specs/2026-08-27-variable-unit-tests-design.md` — 变量测试设计规格
- `02-详细设计文档模板(数据中心).md` §8.2 — 白盒测试要点
- `CONTEXT.md` — 变量 A/B/C/D 与恢复 / 备份策略
