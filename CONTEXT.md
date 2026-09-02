# 电能表数据中心

电表侧参变量、变量、电量等对象的标识与存储边界用语。下方 **Language** 为领域词汇；**已实施** 记录当前代码库中已落地、经验证的设计与边界。

## Language

**参变量块**:
同一套存储特性（SRAM / EEPROM / ROM / 备份）下、带独立校验的一段连续存储。存储特性不同的条目不得同块。块长度上限 128 字节（与 EEPROM 一页相同，含校验），不足则按 128 对齐。
_Avoid_: 页（指代块本身）, 扇区, 参数组, 存储区

**参变量条目**:
一条可按别名读写的参变量小类（标识、元素个数、单元素字节、存储特性）。普通条目整条落在同一个参变量块内；链接数组占用连续独占块，单条记录仍不得跨块。单条记录尺寸不得超过一块的有效载荷（块上限减去校验）。
_Avoid_: 寄存器, 字段, 成员（与块内偏移混用时）

**链接数组**:
记录数 × 单条字节超过一块有效载荷时的参变量条目。按整条记录切成连续独占块，每块最多 `floor(有效载荷 / 单条字节)` 条；块内仍是数据 + 校验 + 对齐填充。
_Avoid_: 按字节切满页, 与其它条目混装的超长数组

**块布局**:
由条目清单在编译前生成的、编译期确定的块划分与条目偏移。按清单顺序扫描：存储特性变化或本块剩余放不下下一条时新开一块；不把不相邻的同标志条目合并。布局随清单重算，不在版本间钉死块号与偏移。
_Avoid_: 运行时装箱, 手工块号, 先按标志归并再装箱, 只存在于构建目录的布局

**出厂默认**:
每条参变量可在 cfg 中给出初始字节；未给出的条目按 n×b 字节全部填 0xFF 后装箱。
_Avoid_: 块默认, ucDefaultBlock, 写在清单同一行, 每条都必须写侧车

**变量类（A/B/C/D）**:
A/B 为 SRAM 镜像 + 非易失备份；C 仅 SRAM；D 仅非易失、无 SRAM 镜像。A/B 各有一套 head/tail 魔数与 body CRC，备份槽位与恢复策略可配置。
_Avoid_: 把 D 类也做 RAM shadow, 运行时再算 layout

---

## 已实施

### 库与 port 边界

| 位置 | 内容 |
|------|------|
| `dc/include/` | 库公共 API 头：`datacenter.h`、`dc_alias.h`、`dc_param.h`、`dc_variable.h` |
| `dc/port/` | **移植模板**（产品复制到自有 port 目录后改）：`dc_storage_cfg.h`、`dc_variable_cfg.h`、`dc_param_cfg.h` |
| `dc/test/port/` | 测试用 port 实例 + 构建生成的 `dc_*_layout.h` |
| `dc/src/` | 固件实现；`dc/tools/` 为 host 布局生成器 |

库 **不包含**：参变量/变量清单、出厂默认、layout 头、storage 驱动、条目宽度常量（`DC_VAR_*` 在 `dc_variable_cfg.h`）。

CMake 变量 `DC_PORT_DIR` 指向产品 port 目录（默认 `test/port`）。编译库与测试均需将 `${DC_PORT_DIR}` 加入 include 路径。

### 统一 storage

- 接口：`DcCfgStorageRead` / `DcCfgStorageWrite`（`dc_storage_cfg.h`）
- 宏：`DC_STORAGE_READ` / `DC_STORAGE_WRITE`
- 统一编址：`DC_STORAGE_BASE_EE` → `DC_STORAGE_BASE_FLASH` → `DC_STORAGE_BASE_FILE`，产品在 port 中按区间分发到 EE / Flash / 文件后端
- 变量 EE 槽位读写经上述接口；不再暴露 EE 回调注册或公开 reload API

### 变量（`dc_variable.c`）

**SRAM**：单块 `var_variable_ram_t`——`head_a` / `body_a` / `tail_a` / `head_b` / `body_b` / `tail_b` / `body_c`（由 `dc_variable_pack` 生成 struct 成员）。

**A/B 读写前**（`var_*_prepare_access`，A/B 各自独立，**运行中**）：
1. head/tail 魔数 OK → 直接访问
2. 否则查 body CRC16-CCITT
3. CRC 坏 → 从 EE **备份区**恢复（PWR_ON_0 → PWR_ON_1，不用掉电区）
4. CRC 好 → 仅补 head/tail 魔数
5. 恢复失败 → `DC_RET_PARAM_ERR`

**上电初始化**（`var_ensure_init`，仅一次）：
- SRAM body CRC 已正确 → 不读 EE，仅补 head/tail 魔数（如有需要）
- CRC 不对 → 先读 **掉电区**（PWR_DWN）；仍不对再读备份区（PWR_ON_0 → PWR_ON_1）

**EE 备份**（`var_backup_tick` / `var_backup_power_down`）：
- `VAR_EE_BACKUP_BANKS`：1 = 单备份（1×PWR_ON + 1×PWR_DWN）；2 = 双备份（2×PWR_ON + 1×PWR_DWN）
- 写 EE 前对 body 填 CRC；head/tail 与 CRC **均**坏时跳过备份
- A 类定时备份；B 类脏标记 + 定时；掉电槽按间隔或 `var_backup_power_down` 写入

**C 类**：仅 SRAM。**D 类**：仅 EE，无 RAM 镜像；读写直接走 storage 绝对地址。

**布局生成**：`dc_variable_pack` → `${DC_PORT_DIR}/dc_variable_layout.h`；`dc_variable_pack --dump` 或 layout 目标后打印 EE/SRAM 映射（host only）。

### 参变量（`dc_param.c`）

- 清单与默认：`dc_param_cfg.h`（`PARAM_ITEM_LIST`；默认字节在 `#if defined(DC_PARAM_PACK)` 段，仅 pack 工具编译）
- 布局生成：`dc_param_pack` → `${DC_PORT_DIR}/dc_param_layout.h`
- 装箱规则：按清单顺序；flags 变化或块满则新块；`DATATYPE_LINKARRAY` 独占连续块、按记录分页
- 当前实现块长 `PARAM_BLOCK_BYTES_MAX = 64`（有效载荷 62，含 2 字节 CRC）
- 布局 dump：`dc_param_pack --dump`（已从固件移除 `ParamDumpLayout`）

### 别名入口

- `dc_read_alias` / `dc_write_alias`（`dc_alias.c`）按大类分派；变量、参变量已实现；电量/需量/列表/记录等仍返回 `DC_RET_UNSUPPORTED` 或 `DC_RET_ALIAS_ERR`

### 构建与测试

- Host：`dc_variable_pack`、`dc_param_pack`；自定义 target `dc_variable_layout`、`dc_param_layout`、`dc_variable_dump`、`dc_param_dump`
- 库 target：`datacenter`
- 测试：`dc/test/`，Google Test（`dc_tests`）；模拟 storage 在 `test/port/dc_storage_sim.c`
- 生成的 `dc_*_layout.h` 在构建时写入 port 目录，**不入库**（见 `.gitignore`）

### 尚未实施 / 不在当前库内

- 参变量 EEPROM 读写（仍 RAM 镜像；`FLAG_EEPROM` 未接 storage）
- 电量、需量、列表参、记录等大类业务逻辑
- IAR 产品工程中的真实 storage 驱动（模板在 `dc/port/`）
