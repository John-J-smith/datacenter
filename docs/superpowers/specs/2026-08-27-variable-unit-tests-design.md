# 变量型数据中心单元测试完善 — 设计规格

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-08-27 | 全面覆盖方案 2：分文件测试 + DC_TEST hook |

## 1. 目标

在现有 `variable_test.cpp`（3 个用例）基础上，对齐详细设计 **8.2 变量类白盒测试要点** 与 `CONTEXT.md` 中 A/B/C/D 四类变量、恢复/备份策略，完善 Google Test 单元测试套件。

**成功标准：**

- `dc_tests` 中全部变量相关用例通过（layout / rw / recovery / backup）
- 覆盖 8.2.1 布局、8.2.2 类型行为、CONTEXT.md 恢复链与备份调度、参数/错误码校验
- 用例间隔离（任意顺序、重复运行一致）
- 产品固件（不定义 `DC_TEST`）零 hook 开销
- 每个 `TEST_F` 前有块注释：测试内容 + 编号测试步骤

## 2. 方案选型

| 决策 | 选择 |
|------|------|
| 覆盖深度 | **A — 全面覆盖**（8.2 + CONTEXT.md） |
| 恢复/备份可测性 | **B — 最小 test hook**（`#ifdef DC_TEST`） |
| 文件组织 | **方案 2 — 按关注点拆文件** |
| `var_backup_power_down` | **不修改**函数名与实现；测试直接 `extern` 调用 |
| `VAR_EE_BACKUP_BANKS=1` | 不单独开 CMake target；PWR_ON_1 相关用例 `#if (VAR_EE_BACKUP_BANKS >= 2)` |

## 3. Test Hook API

### 3.1 编译边界

- 在 `dc/test/CMakeLists.txt` 中为 `datacenter` 与 `dc_tests` 定义 `DC_TEST`
- Hook 实现位于 `dc/src/dc_variable.c` 末尾 `#ifdef DC_TEST` 块
- 头文件 `dc/test/dc_test_variable.h`（仅测试 include，不进 `dc/include/`）

### 3.2 API

```c
typedef enum {
    DC_TEST_VAR_ZONE_A = 0,
    DC_TEST_VAR_ZONE_B = 1,
} dc_test_var_zone_t;

void DcTestVarReset(void);
void DcTestVarCorruptMagic(dc_test_var_zone_t zone);
void DcTestVarCorruptCrc(dc_test_var_zone_t zone);
void DcTestVarInvalidateAll(dc_test_var_zone_t zone);
void DcTestVarResetBackupTimers(void);

/* 已有实现，测试侧 extern 声明，不重命名 */
void var_backup_power_down(void);
```

### 3.3 行为

| 函数 | 行为 |
|------|------|
| `DcTestVarReset` | `memset(&s_var_ram, 0, sizeof s_var_ram)`；`s_var_inited = 0`；`s_b_dirty = 0`；备份计时器清零 |
| `DcTestVarCorruptMagic` | 对应 zone 的 `head_*` / `tail_*` 置 0；body 与 CRC 不动 |
| `DcTestVarCorruptCrc` | 对应 zone 的 `body_*.crc` 置 0；magic 不动 |
| `DcTestVarInvalidateAll` | magic + crc 全坏 |

### 3.4 Fixture SetUp

所有变量测试 fixture：

```cpp
void SetUp() override {
    DcTestStorageReset();
    DcTestVarReset();
}
```

## 4. 测试文件结构

```
dc/test/
  dc_test_variable.h           # Hook + var_backup_power_down extern
  variable_test_helpers.hpp    # SeedEe、ReadVar/WriteVar、EE 断言
  variable_layout_test.cpp     # 8.2.1
  variable_rw_test.cpp         # 8.2.2 + 错误码
  variable_recovery_test.cpp   # 上电/运行中恢复
  variable_backup_test.cpp     # tick / 脏标记 / 掉电
```

- **删除** `variable_test.cpp`，现有 3 个用例逻辑迁入对应文件
- `param_test.cpp` 不变

### 4.1 Helpers（`variable_test_helpers.hpp`）

- `SeedAClassEeSlot(slot, first_byte)` / `SeedBClassEeSlot(slot, first_byte)` — 构造 CRC 合法 EE 块
- `SeedAClassEe(first_byte)` — 默认写 PWR_ON_0
- `ReadVar(type_id, index, buf, len)` / `WriteVar(...)` — `VarAliasBuild` + `dc_read_alias` / `dc_write_alias` 薄封装
- `ExpectEeSlotFirstByte(slot, val)` — 读 EE 槽首字节断言
- Helper 使用 `@brief` 一行说明；不在每个用例重复

### 4.2 用例注释规范

每个 `TEST_F` 前一行写测试目的，步骤注释内联在对应代码处：

```cpp
// 测试内容：<验证的行为/策略，对应 CONTEXT 或 8.2 条目>
TEST_F(..., CaseName)
{
    // 1. Arrange: ...
    // 2. Act: ...
    // 3. Assert: ...
}
```

## 5. 用例清单

### 5.1 `variable_layout_test.cpp`（8.2.1）

| 用例 | 验证点 |
|------|--------|
| `EeSlotAddresses` | A/B/D EE 基址、各槽位地址、`VAR_EE_BACKUP_BANKS` 条件分支 |
| `StructSizesMatchEndAddr` | `sizeof(var_layout_a/b/c/d_t) == VAR_*_END_ADDR` |
| `ApiTableRowCount` | `tVariableApiTableCount` == cfg 中 A+B+C+D 条目总数 |
| `ApiTableOffsetsInBounds` | 每行 `eVariableAddr + ucLength` 不越对应分区 END_ADDR |
| `ApiTableTypePartition` | 表中 A→B→C→D 连续分段，`ucType` 与分区一致 |
| `EeMapContiguous` | B 基址 = A 基址 + A 总长；D 同理；`VAR_EE_TOTAL` 一致 |

### 5.2 `variable_rw_test.cpp`

| 用例 | 验证点 |
|------|--------|
| `TypeA_ReadWrite` | A 类 DATE_TIME 读写回读 |
| `TypeB_ReadWrite` | B 类 USED_MONTH 读写 |
| `TypeC_ReadWriteMultiIndex` | C 类 RMS_VOLTAGE 三相逐 index |
| `TypeC_NoEeSideEffect` | C 类写后 EE 模拟区无变化 |
| `TypeD_DirectEe` | D 类写后 `DcTestStoragePtr()[VAR_D_EEPROM_BASE]` 一致 |
| `InvalidAlias` | 无效小类 → `DC_RET_ALIAS_ERR` |
| `IndexOutOfRange` | index+usLen 越界 → `DC_RET_PARAM_ERR` |
| `ZeroLength` | `usLen==0` → 返回 0 |
| `NullBufferWithLength` | `dataPtr==NULL && usLen!=0` → `DC_RET_PARAM_ERR` |
| `InvalidEeSlot` | 非法 slot 读写 → `DC_RET_PARAM_ERR` |

### 5.3 `variable_recovery_test.cpp`

| 用例 | 验证点 |
|------|--------|
| `PwrUpPrefersPwrDwn` | 上电：掉电区优先于 PWR_ON_0 |
| `PwrUpFallbackPwrOn0` | 掉电区 CRC 坏，PWR_ON_0 好 → 恢复 PWR_ON_0 |
| `PwrUpFallbackPwrOn1` | 掉电+PWR_ON_0 坏，PWR_ON_1 好 → 恢复 PWR_ON_1（`BANKS>=2`） |
| `RuntimeMagicBadCrcOk` | 运行中 magic 坏、CRC 好 → 读成功，仅补 magic |
| `RuntimeCrcBadRestoreFromPwrOn0` | 运行中 CRC 坏，EE PWR_ON_0 有备份 → 恢复 |
| `RuntimeCrcBadFallbackPwrOn1` | PWR_ON_0 坏，PWR_ON_1 好 → 恢复链（`BANKS>=2`） |
| `RuntimeRestoreFails` | SRAM 全坏且 EE 全坏 → `DC_RET_PARAM_ERR` |
| `TypeB_SameRecoveryChain` | B 区镜像：Magic/Crc/失败 三条 |

**触发路径：**

- 上电恢复：`DcTestVarReset()` → seed EE → 首次 `dc_read_alias` → `var_ensure_init`
- 运行中恢复：正常读写建立 SRAM → `DcTestVarCorrupt*` → 再次 `dc_read_alias` → `var_*_prepare_access`

### 5.4 `variable_backup_test.cpp`

| 用例 | 验证点 |
|------|--------|
| `TypeA_PeriodicPwrOnBackup` | 写 A → `var_backup_tick(VAR_A_BACKUP_INTERVAL_SEC)` → PWR_ON 槽含新数据 |
| `TypeB_DirtyGatedBackup` | 写 B 后未达间隔不备份；达间隔且脏则备份；未写 B 则不备份 |
| `PwrDwnIntervalBackup` | tick `VAR_PWR_DWN_INTERVAL_SEC` → A/B 掉电槽更新 |
| `ImmediatePowerDown` | `var_backup_power_down()` → 掉电槽立即写入 |
| `BackupSkippedWhenInvalid` | `DcTestVarInvalidateAll(A)` 后 tick → EE 不变 |

## 6. CMake 改动

`dc/test/CMakeLists.txt`：

```cmake
add_executable(dc_tests
    variable_layout_test.cpp
    variable_rw_test.cpp
    variable_recovery_test.cpp
    variable_backup_test.cpp
    param_test.cpp
)
target_compile_definitions(datacenter PRIVATE DC_TEST)
target_compile_definitions(dc_tests PRIVATE DC_TEST)
target_include_directories(dc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

移除 `variable_test.cpp`。

## 7. 验收

```bash
cmake --build <build-dir> --target dc_tests
ctest --test-dir <build-dir> -R dc_tests --output-on-failure
```

1. 全部变量用例绿灯
2. 覆盖项与第 5 节清单一致
3. 用例注释符合第 4.2 节
4. 未定义 `DC_TEST` 的 `datacenter` 构建不含 hook 符号

## 8. 不在范围

- `VAR_EE_BACKUP_BANKS=1` 独立 CI target
- 修改 `var_backup_power_down` 名称或实现
- 参变量测试扩展
- `dc_variable_pack --verify` host 工具

## 9. 参考

- `CONTEXT.md` — 变量 A/B/C/D 与恢复/备份策略
- `02-详细设计文档模板(数据中心).md` §8.2
- `dc/src/dc_variable.c` — 实现
- `dc/test/param_test.cpp` — 错误码测试风格参考
