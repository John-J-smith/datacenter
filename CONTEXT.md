# 电能表数据中心

电表侧参变量、变量、电量等对象的标识与存储边界用语。下方 **Language** 为领域词汇；**已实施** 记录当前代码库中已落地、经验证的设计与边界。

## Language

**参变量块**:
同一套存储特性下、带独立 CRC 的一段连续存储。存储特性不同的条目不得同块。逻辑块上限 `PARAM_BLOCK_SIZE`（含 2 字节 CRC），不得跨 `PARAM_EE_PAGE_SIZE`；EE 主区总长向上对齐到页。
_Avoid_: 页（指代块本身）, 扇区, 参数组, 存储区

**参变量条目**:
一条可按别名读写的参变量小类（标识、元素个数、单元素字节、存储特性）。普通条目整条落在同一个参变量块内；链接数组占用连续独占块，单条记录仍不得跨块。单条记录尺寸不得超过一块的有效载荷（块上限减去校验）。
_Avoid_: 寄存器, 字段, 成员（与块内偏移混用时）

**链接数组**:
记录数 × 单条字节超过一块有效载荷时的参变量条目。按整条记录切成连续独占块，每块最多 `floor(有效载荷 / 单条字节)` 条；块内仍是数据 + 校验 + 对齐填充。
_Avoid_: 按字节切满页, 与其它条目混装的超长数组

**块布局**:
由条目清单在编译前生成的、编译期确定的块划分与条目偏移。清单用四段宏固定顺序（RAM+EE 双备份 → EE 双备份 → RAM+EE 单备份 → EE 单备份），pack 按该顺序扫描：flags 变化或本块放不下则新开一块。`tParamBlockTable` 只登记主槽；双备份的备份区 2 用 `PARAM_EE_BAK_BASE + uBlockEeOff` 推导，不占 table 行。
_Avoid_: 运行时装箱, 手工块号, 把备份区 2 再写进 table

**出厂默认**:
有默认的条目在 API 表带 `pDefault`（pack 生成 `g_default_*`）；`NULL` 表示无默认，工作区填 0xFF。上电填默认**不写** EEPROM。
_Avoid_: 块级 ROM 数组, 初始化时把默认回写 EE

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

### 变量类（`dc_variable.c`）

清单：`dc_variable_cfg.h` 的 `VAR_LIST_A/B/C/D`。布局：`dc_variable_pack` → `dc_variable_layout.h`。

**SRAM**：一块 `var_variable_ram_t`（`head_a` / `body_a` / `tail_a` / `head_b` / `body_b` / `tail_b` / `body_c`）。A/B 各有魔数 + body CRC16-CCITT。

| 类 | 工作区 | 非易失 |
|----|--------|--------|
| A | SRAM body_a | EE：PWR_ON（1 或 2 bank）+ PWR_DWN |
| B | SRAM body_b | 同上（独立槽位） |
| C | 仅 SRAM body_c | 无 |
| D | 无 RAM 镜像 | 仅 EE，绝对地址直读直写 |

**上电**（`var_ensure_init`，一次）：SRAM body CRC 好 → 不读 EE，必要时只补魔数；CRC 坏 → PWR_DWN → PWR_ON_0 → PWR_ON_1。

**运行中访问前**（`var_*_prepare_access`）：魔数 OK 则直接访问；否则查 CRC；CRC 坏从**备份区**恢复（PWR_ON_0 → PWR_ON_1，不用掉电区）；CRC 好只补魔数；恢复失败 `DC_RET_PARAM_ERR`。

**写 EE**（`var_backup_tick` / `var_backup_power_down`）：`VAR_EE_BACKUP_BANKS` 为 1 或 2。写前填 body CRC；魔数与 CRC 都坏则跳过。A 定时备份；B 脏标记 + 定时；掉电槽按间隔或显式掉电写入。

### 数据类 / 参变量（`dc_param.c`）

清单：`dc_param_cfg.h` 四段 `*_ROWS` 拼成 `PARAM_ITEM_LIST`（顺序即小类 ID 与装箱顺序）；每段第二个参数绑定 store，行里写 `ST`。`dc_param_pack` 按段核对 flags。默认：`PARAM_ITEM_DEFAULTS` + `*_def[]`（仅 pack 编译）。布局：`dc_param_pack` → `dc_param_layout.h`。

**几何**：`PARAM_BLOCK_SIZE` 一块（含 2 字节 CRC）；`PARAM_EE_PAGE_SIZE` 必须是其整数倍。主区槽步进为 `PARAM_BLOCK_SIZE`；`PARAM_EE_TOTAL` 向上对齐到页；`PARAM_EE_BAK_BASE = PARAM_EE_TOTAL`，`PARAM_EE_BAK_SPAN` 为带 `FLAG_EEPROM_BAK` 的块所占主区长度。

**四种存储**（`dc_param_cfg_macros.h`）：

| 宏 | flags | 工作区 | EE |
|----|-------|--------|-----|
| `PARAM_STORE_RAM_EE_BK` | SRAM+EEPROM+BAK | `g_param_ram_*` | 主槽 + 备份区 2 |
| `PARAM_STORE_EE_BK` | EEPROM+BAK | 无（`ram=NULL`，scratch） | 主槽 + 备份区 2 |
| `PARAM_STORE_RAM_EE` | SRAM+EEPROM | `g_param_ram_*` | 仅主槽 |
| `PARAM_STORE_EE` | EEPROM | 无（scratch） | 仅主槽 |

备份区 2 **不进** `tParamBlockTable`：`addr = PARAM_EEPROM_ORIGIN + PARAM_EE_BAK_BASE + uBlockEeOff`。EE-only 行仍带紧凑 `ucBlockLen`（含 CRC），不能为 0。

**API 表** `tParamApiTable`：小类、块下标、块内偏移、`ucParamLen`（逻辑总长；LINKARRAY 为记录总字节）、`pAttr`、`pDefault`。

**上电**（`param_ensure_init`）：有 SRAM 且块 CRC 好 → 保留 RAM；否则主槽 → 备份区 2 → `pDefault`/0xFF。恢复与填默认**都不写** EE。

**读写**：INT / ARRAY / STRUCT / LINKARRAY（跨连续块、按记录分页）。EE-only 经 `PARAM_BLOCK_SIZE` scratch 装主槽/备份，失败再套默认。**仅 `dc_write_*` 落盘**：刷新块 CRC 后写主槽；有 BAK 再写备份区 2。

**pack dump**：生成 `dc_*_layout.h` 时同步打印，并 upsert 同目录 `dc_layout.md`。文件开头为分类消耗（各类 RAM、相对 EE 起止与占用、参变量 `reserve`；参变量含备份槽）；其后为变量段 / 参变量段。块表 `reserve` = `blk_size` − `compact`。明细偏移格式 `十六进制(十进制)`，相对各自 EE 起点。参变量块表在 md 与注释掉的 `.h` 行中体现备份槽。`--dump` 只打 stdout（含分类消耗）。

### 别名入口

- `dc_read_alias` / `dc_write_alias`（`dc_alias.c`）按大类分派；变量、参变量已实现；电量/需量/列表/记录等仍返回 `DC_RET_UNSUPPORTED` 或 `DC_RET_ALIAS_ERR`

### 构建与测试

- Host：`dc_variable_pack`、`dc_param_pack`；自定义 target `dc_variable_layout`、`dc_param_layout`、`dc_variable_dump`、`dc_param_dump`
- 库 target：`datacenter`
- 测试：`dc/test/`，Google Test（`dc_tests`）；模拟 storage 在 `test/port/dc_storage_sim.c`
- 生成的 `dc_*_layout.h` 在构建时写入 port 目录，**不入库**（见 `.gitignore`）

### 尚未实施 / 不在当前库内

- 电量、需量、列表参、记录等大类业务逻辑
- IAR 产品工程中的真实 storage 驱动（模板在 `dc/port/`）
