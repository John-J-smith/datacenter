# 嵌入式数据中心库 — 设计规格

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-08-17 | 头脑风暴定稿 |
| v1.1 | 2026-08-17 | 全文改为中文 |
| v1.2 | 2026-08-17 | 除变量外：RAM 2 字节校验 + noinit；失败从存储器恢复 |

## 1. 目标

面向 MCU 产品，提供一套**易于维护**的嵌入式数据中心库，要求：

- 同时支持**裸机**与 **RTOS**
- 用稳定 API 屏蔽存储介质、备份、CRC、磨损等细节
- **核心（core）与业务领域无关**
- **电表六大类数据**作为独立包，构建在 core 之上

参考材料（不视为最优方案）：仓库根目录现有三份电表数据中心模板。其中有价值的思路（别名访问、表驱动布局、块存储、策略、tick/掉电）予以通用化；电表业务逻辑不进入 core。

## 2. 已锁定决策

| 主题 | 选择 |
|------|------|
| 定位 | 领域无关 core；电表 profile 独立 |
| 访问模型 | 逻辑 ID（`dc_id_t`）+ 读写时的格式参数 `type` |
| 配置方式 | 仅编译期静态表（无运行时注册） |
| 可靠性 | 可插拔策略，按**块**选择 |
| 并发 | 编译期三选一：无锁 / 临界区 / mutex |
| 介质 | 统一块设备端口（`read` / `write` / `erase`） |
| 格式转换 | 走 core API 主路径（始终带 `type`） |
| 架构 | 扁平注册表 + 策略引擎 |
| 旧 API | **不兼容**；meter 使用 `alias`（不用 `genre`） |
| RAM 镜像完整性 | 除**变量类**外：载荷末尾 **2 字节校验** + **noinit**；校验失败则从非易失存储器恢复 |

## 3. 包划分

```text
应用
    │
    ├─ 可选 ──► dc_meter（电表 Profile）
    │              电量 / 需量 / 参变量 / 变量 / 列表 / 记录
    │              alias 编码、累加、清零、电表生命周期胶水
    │
    └─────────► dc_core
                   注册表 → 格式 → 策略 → 块存储 → 介质/OS 端口
```

| 包 | 职责 | 依赖 |
|----|------|------|
| **dc_core** | ID 查找、FMT 转换、策略、块、端口 | 无业务领域依赖 |
| **dc_meter** | 六大类、alias 表、电量累加、清零、meter 的 init/tick | 仅依赖 dc_core 公开 API |

产品可只链接 **core**，或链接 **core + meter**。Meter 不得调用 Port/Block 内部接口。

### 目录布局

| 路径 | 职责 |
|------|------|
| `include/dc/*.h` | Core 公开 API、类型、fmt、配置、端口 |
| `src/core/` | 注册表、块存储、init/tick/powerdown |
| `src/fmt/` | 格式转换引擎 |
| `src/policy/` | 各策略实现 |
| `port/` | 产品侧介质与 OS 移植模板 |
| `meter/include/`、`meter/src/` | 电表 Profile |
| `examples/minimal/` | 仅 core 示例 |
| `examples/meter_stub/` | core + meter 桩表示例 |
| `docs/` | 规格与维护说明 |

## 4. Core 架构

### 4.1 分层（应用不得跳级调用）

1. **API + 锁包装** — 可配置加锁
2. **Item 注册表** — 静态 `id →` 属性
3. **格式引擎** — `native_fmt` ↔ 调用方 `type`
4. **策略引擎** — load/store/flush/tick/powerdown
5. **块存储** — CRC、主备/环形布局、脏标记
6. **介质端口** — 统一块设备
7. **OS 端口** — lock/unlock，可选 `time_ms`

### 4.2 Core v1 明确不做

- 电表六大类、电量累加、需量等业务逻辑
- 运行时动态注册
- 按介质类型拆多套驱动 API（EEPROM / FRAM / Flash 分端口）
- 自动「选择最优介质」

## 5. Core 数据模型

### 5.1 ID

- `dc_id_t` = `uint32_t`
- Core 不赋予业务含义
- ID 段由 meter（或产品）规划；meter 内各大类使用互不重叠的段

### 5.2 Item 表（置于 ROM）

| 字段 | 含义 |
|------|------|
| `id` | 逻辑 ID |
| `block_id` | 所属存储块 |
| `offset` / `size` | 块内有效载荷中的偏移与字节长度 |
| `native_fmt` | 块内/介质上的权威存储格式 |
| `flags` | 如只读、掉电必存等 |

查找：表按 `id` 排序 + 二分查找（支持稀疏 ID）。

### 5.3 块描述符

策略**仅挂在块级**（避免同一块上 item 级策略冲突）。

| 字段 | 含义 |
|------|------|
| `block_id` | 块编号 |
| `media` | 指向 `dc_media_t` 区域的指针/句柄 |
| `size` | 有效载荷长度（不含尾部校验时指纯数据；开启 RAM 校验时见 §5.3.1） |
| `layout` | 单份 / 主备 / 环形槽数量 |
| `policy_id` | 本块策略 |
| `ram_cache` | 可选 SRAM 镜像；启用 noinit 时必须落在 noinit 段 |
| `flags` | 如 `DC_BLOCK_RAM_CRC16`、`DC_BLOCK_NOINIT`、`DC_BLOCK_SKIP_RAM_CRC`（变量类） |

#### 5.3.1 RAM 校验 + noinit（除变量外默认启用）

**适用范围（meter）**

| 大类 | RAM 2 字节校验 + noinit | 校验失败时 |
|------|-------------------------|------------|
| 电量 / 需量 / 参变量 / 列表 / 记录 | **是** | 从该块绑定的非易失介质按策略 `load` 恢复到 RAM，再重算/写回 RAM 校验 |
| 变量 | **否**（`DC_BLOCK_SKIP_RAM_CRC`） | 仍按变量类自身规则（A/B 首尾校验等，见 meter）；不套本条强制规则 |

**Core 通用语义（与业务无关）**

- 标志 `DC_BLOCK_RAM_CRC16`：`ram_cache` 指向的镜像布局为 `[payload | uint16_t crc]`，**2 字节小端 CRC16**（算法在 core 内固定一种，如 CRC16-CCITT，实现计划中写死并测）。
- 标志 `DC_BLOCK_NOINIT`：`ram_cache` 必须置于链接脚本的 **noinit / `.noinit` / `.bss.noinit`** 段；复位后内容可能残留，**不以 BSS 清零为准**。
- 端口增加可选：`dc_os_port_t` 或链接约定说明 noinit 段；产品负责把镜像对象放到正确段（提供 `DC_NOINIT` 属性宏）。

**何时检查**

1. `dc_init`：对每个带 `RAM_CRC16` 的块，先验 RAM CRC；失败则 `policy.load` 从存储器恢复，成功后再写 RAM CRC；介质也失败 → `DC_ERR_CRC`。
2. `dc_read` / 经 RAM 的快速路径：访问前快速验 CRC；失败则同上恢复，再读。
3. `dc_write`（写 RAM 镜像后）：更新 payload 后**重算并写入** 2 字节 CRC。
4. `dc_tick` 自检：可抽样或全量复验；失败则恢复。

**与 NV 侧 CRC 的关系**

- RAM 尾 2 字节校验：保护 **noinit 镜像**在复位/干扰后的完整性。
- 介质上的块仍按策略带自己的校验（MIRROR/WEAR 等）；恢复路径走策略 `load`，不是简单 memcpy 无校验介质。

**变量例外在 core 中的表达**

- Core 不识别「变量」业务名；产品/meter 对变量类块置 `DC_BLOCK_SKIP_RAM_CRC`（可不置 `RAM_CRC16`）。
- 仅有 `ram_cache`、未置 `RAM_CRC16` 的块：不做本条强制恢复流程。

### 5.4 内置策略（v1）

| `policy_id` | 行为 |
|-------------|------|
| `NONE` | 直写，无备份 |
| `MIRROR` | 主 + 备；读时选校验通过的副本 |
| `PERIODIC` | 脏标记；`dc_tick` 按间隔刷盘 |
| `ON_CHANGE` | 写入置脏；按间隔或 `dc_flush_*` 刷盘 |
| `WEAR_RING` | 定长槽环形写；读选最新且有效的槽 |

各策略提供：`load`、`store`、`flush`、`on_tick`、`on_powerdown`。未使用的策略可用配置宏裁剪。

### 5.5 格式模型

```c
/* 编码类型 + 字节宽度 + 小数位数 */
FMT(ctype, bytes, dotn)
```

- 存储侧始终使用 `native_fmt`
- `dc_read` / `dc_write` 在缓冲区 `type` 与 `native_fmt` 之间转换
- 不兼容组合 → `DC_ERR_FMT`（禁止静默截断）

v1 格式集合：HEX/BCD + 字节宽度 + 小数位；后续按需扩展。

### 5.6 介质句柄

```c
typedef struct {
  dc_media_ops_t const *ops; /* read / write / erase */
  uint32_t base;
  uint32_t capacity;
  void *ctx;
} dc_media_t;
```

易失/非易失由产品用不同 `ops` 或标志区分。Core 只需知道写前是否需要 erase。

### 5.7 静态装配

产品提供 `const` 的 `block[]`、`item[]` 以及带指针与个数的 `dc_config_t`。`dc_init` 只保存指针，大表留在 ROM。

## 6. Core API

### 6.1 函数

| API | 作用 |
|-----|------|
| `dc_init(cfg, port)` | 绑定配置与端口；对各块执行策略 `load` |
| `dc_deinit(void)` | 可选清理 |
| `dc_read(id, buf, len, type)` | 读；**`len` 为字节数**；成功返回传输字节数 |
| `dc_write(id, buf, len, type)` | 写；**`len` 为字节数** |
| `dc_clear(id)` | 清零/恢复默认（整块清零的特殊 id 约定见配置说明） |
| `dc_flush_item(id)` / `dc_flush_block(block_id)` | 强制将脏数据落盘 |
| `dc_tick(elapsed_ms)` | 周期备份 / 磨损推进 / 自检 |
| `dc_powerdown(void)` | 紧急刷写所有脏块 |
| `dc_get_info(id, &info)` | 查询 size、native_fmt、flags |
| `dc_last_error(void)` | 掉电部分失败后的粘滞诊断（v1） |

应用不得调用策略/块模块的内部符号。

### 6.2 返回值

```c
typedef int32_t dc_ret_t;
/* >= 0 : 成功，传输字节数（如 powerdown 成功可为 0） */
/* <  0 : 错误码 */
```

| 码 | 含义 |
|----|------|
| `DC_OK` (0) | 无长度语义时的成功 |
| `DC_ERR_PARAM` | 空指针 / len / 越界等 |
| `DC_ERR_NOTFOUND` | 未知 id |
| `DC_ERR_FMT` | 无法转换 |
| `DC_ERR_READONLY` | 禁止写 |
| `DC_ERR_IO` | 介质失败 |
| `DC_ERR_CRC` | 校验失败且恢复失败 |
| `DC_ERR_NOSPC` | 环形槽/布局空间不足 |
| `DC_ERR_STATE` | 生命周期状态不允许 |
| `DC_ERR_UNSUPPORTED` | 已裁剪的能力 |

### 6.3 加锁

| 宏 | 行为 |
|----|------|
| `DC_LOCK_NONE` | 不加锁；产品保证单写者 |
| `DC_LOCK_CRITICAL` | `port` 进出临界区 |
| `DC_LOCK_MUTEX` | `port` mutex 加解锁（RTOS 下推荐默认） |

v1：所有公开 API（含 tick/powerdown）共用一把库级锁。介质 ops 不得重入 `dc_*`。持锁期间无用户回调。

### 6.4 OS 端口

- `lock` / `unlock`
- 可选 `time_ms`
- 裸机：临界区；RTOS：mutex

### 6.5 生命周期（core）

| 状态 | 含义 |
|------|------|
| `UNINIT` | 未初始化 |
| `READY` | 可正常读写 |
| `POWERDOWN` | 掉电过程中/之后；普通写拒绝（除非策略允许） |

Meter 将更丰富的 `METER_SYS_*` 映射到上述调用；core 不内嵌电表状态机。

### 6.6 头文件

- `dc.h` — 应用主包含
- `dc_types.h`、`dc_fmt.h`、`dc_config.h`、`dc_port.h`
- 内部头不向产品安装

## 7. 电表 Profile（dc_meter）

### 7.1 对外 API（新命名；不兼容旧工程）

| API | 作用 |
|-----|------|
| `dc_meter_read(alias, buf, len, type)` | `len` = **成员个数**；换算字节后调用 `dc_read` |
| `dc_meter_write(alias, buf, len, type)` | 同上；电量类默认只读 |
| `dc_meter_add_energy(alias, add, check)` | 累加到 RAM 镜像；置 NV 块脏 |
| `dc_meter_clear(alias)` | 按 alias 规则清零（含整类通配） |
| `dc_meter_init(sys_state)` | 按需映射到 `dc_init` / `dc_powerdown` |
| `dc_meter_tick(void)` | 内部调用 `dc_tick(period_ms)` |

库内不提供 `ReadAliasData` / `WriteAliasData` / `genre` 等旧名或兼容宏。

### 7.2 Alias 编码

- 由 meter 定义位域：大类 | 小类 | 属性/费率/分项索引…
- 解析：`alias` → `dc_id`
- Core 从不识别 `alias`

### 7.3 六大类 → 块 / Item

| 大类 | 落表方式 | 典型块策略 | RAM CRC16 + noinit |
|------|----------|------------|---------------------|
| 电量 | 小类/块 → 若干 item（主值 + 增量）；类似原 `tEnyApiTable` | PERIODIC / ON_CHANGE / WEAR_RING / MIRROR | **启用** |
| 需量 | 需量块 → items | PERIODIC + 掉电 flush | **启用** |
| 参变量 | 二级表（小类→块内 offset；块默认值在 ROM） | MIRROR / NONE+CRC / RAM 缓存+PERIODIC | **启用**（有 RAM 镜像的块） |
| 变量 | A/B/C/D 分区为不同块 | A/B：周期+掉电；C：仅 RAM + NONE；D：直写 EE | **不启用**（`SKIP_RAM_CRC`；沿用类内校验） |
| 列表 | 表头 + 元素 items | PERIODIC 或 MIRROR | **启用** |
| 记录 | 定长记录槽 | WEAR_RING | **启用**（有 RAM 缓存时） |

原文档中多层 EE「按变化量 8192/128 备份」等**不进入 core**。Meter v1 用 WEAR_RING + 分辨率配置近似；不足时仅通过公开 `dc_*` 在 meter 侧组合扩展。

### 7.4 电量累加路径

```text
计量库 → dc_meter_add_energy
      → 更新 SRAM 侧 item
      → 标记 NV 块脏
      → dc_meter_tick / dc_powerdown 经策略刷盘
```

`check` 校验魔数（如 `0x55`）仅在 meter 内校验。

### 7.5 扩展与裁剪

1. 扩展 meter 枚举/映射表
2. 更新静态 `dc_block_t` / `dc_item_t` 表
3. 编译期 `_Static_assert` 做一致性检查
4. 按大类编译开关裁掉未用源文件

### 7.6 职责归属

| 关注点 | 归属 |
|--------|------|
| CRC / 主备 / 定时 / 环形磨损 | dc_core 策略 |
| RAM 2 字节校验、noinit 布局、失败后 `load` 恢复 | dc_core（块标志驱动） |
| 变量类跳过 RAM CRC；六大类表如何置标志 | dc_meter |
| Alias、六大类、累加、清零、电表状态胶水 | dc_meter |
| I2C/SPI/Flash 驱动；noinit 链接段 | 产品 port / 链接脚本 |
| 电量显示用 FMT 宏（`ENY_FMT_*`） | dc_meter |

## 8. 错误处理

- 失败返回负错误码；禁止静默丢数据
- 主备/环形：任一有效副本可恢复；否则 `DC_ERR_CRC`
- **RAM CRC 失败**：先尝试从存储器 `load`；介质恢复成功则修复 noinit 镜像；仍失败 → `DC_ERR_CRC`
- 掉电：尽力刷写；部分失败用 `dc_last_error` 粘滞记录
- Meter 可在不重叠的负码区间增加 `DC_METER_ERR_*`（如 check 失败）

## 9. 测试与验收

### 9.1 测试分层

| 层 | 覆盖内容 | 环境 |
|----|----------|------|
| Core 单元 | 注册表、FMT 往返、各策略、坏 CRC、**noinit 镜像 CRC 失败→从介质恢复** | Host + 假介质 |
| Port 契约 | 介质边界、三种锁模式冒烟 | Host 或目标板 |
| Meter 单元 | alias→id、表一致性、累加/清零、电量只读 | Host |
| 集成 | `minimal`、`meter_stub`：写入→复位→读回 | 板级/仿真 |

假介质：用 RAM 模拟 NV，可注入 CRC 损坏与写失败。

### 9.2 验收标准（v1 Done）

**Core**

1. 静态表产品可完成 init / read / write / tick / powerdown
2. 五种策略各至少一条自动化用例通过
3. 三种锁模式均可编译；MUTEX 在 host 上有多线程冒烟
4. 无动态分配；公开头文件无电表符号
5. 带 `RAM_CRC16` 的块：破坏 noinit CRC 后，读/init 能从假介质恢复；恢复失败返回 `DC_ERR_CRC`

**Meter**

1. 六大类均有可编译路径与最小表模板
2. 上文 Meter API 在 stub 上端到端打通
3. 按类裁剪宏可排除对应代码
4. 无旧 `genre` / 旧 API 名
5. 变量类块未启用 `RAM_CRC16`；其余带 RAM 镜像的大类已启用

**可维护性**

1. 新增一项 = 改表 + 断言，无需改 core 源码
2. Core 与 Meter 维护文档分开

## 10. 风险与 v1 刻意简化

- 完整多层 EE 变化量磨损方案延后；先用 WEAR_RING 近似
- 格式先支持 HEX/BCD + 宽度/小数位
- 采用单库级锁（非按块锁）以降低复杂度

## 11. 实现状态与验收对照

### 11.1 dc_core v1（已完成，2026-08-17）

对照 §9.2 **Core** 五条验收标准：

| # | 验收项 | 状态 | 证据 |
|---|--------|------|------|
| 1 | 静态表产品可完成 init / read / write / tick / powerdown | ✅ | `test_api_rw`（init/tick/read/write + RAM CRC 恢复读）；`test_api_powerdown`（powerdown 后写拒绝）；`examples/minimal` 端到端 |
| 2 | 五种策略各至少一条自动化用例 | ✅ | `policy_none`、`policy_mirror`、`policy_periodic`（含 ON_CHANGE）、`wear_ring`；五种策略在 `test_policy_periodic` 中 ON_CHANGE 与 PERIODIC 分测 |
| 3 | 三种锁模式均可编译；MUTEX 在 host 多线程冒烟 | ✅ | MinGW 分别 `-DDC_LOCK=NONE/CRITICAL/MUTEX` 构建成功；`test_lock_none` + `test_lock_mutex_pthread`（build-mutex，14/14 通过） |
| 4 | 无动态分配；公开头文件无电表符号 | ✅ | `src/` 无 `malloc`/`calloc`/`realloc`；`include/dc/*.h` 无 meter/genre 符号 |
| 5 | `RAM_CRC16` 块：破坏 noinit CRC 后从假介质恢复；失败返回 `DC_ERR_CRC` | ✅ | `test_ram_crc_recover`；`test_api_rw` 破坏 RAM 后 `dc_read` 自动恢复 |

**Host 测试（2026-08-17）：**

- `build-none`（`DC_LOCK=NONE`）：ctest **13/13** 通过
- `build-mutex`（`DC_LOCK=MUTEX`）：ctest **14/14** 通过（含 `lock_mutex_pthread`）

**已知限制（v1 可接受）：**

- `DC_LOCK=CRITICAL` 仅验证编译，无独立自动化用例（规格只要求可编译）
- 默认 CI/host 套件以 `DC_LOCK=NONE` 为准；MUTEX  pthread 冒烟需 `-DDC_LOCK=MUTEX` 构建
- Meter 与可维护性验收项（§9.2 后两段）尚未实现，见 `docs/superpowers/plans/2026-08-17-dc-meter.md`

### 11.2 后续步骤

- dc_meter v1 实现与 §9.2 Meter / 可维护性验收
- 板级集成：`minimal`、`meter_stub` 写入→复位→读回
