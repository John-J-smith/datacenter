# 电表数据中心骨架 — 设计规格

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-08-20 | 绿地骨架：别名分发 + 变量 X-macro 参数表 |

相关决策：`docs/adr/0001` … `0007`。术语：仓库根目录 `CONTEXT.md`。

本文替代「在现有 `dc_core` / `dc_meter` 上继续长」的路线。仓库里 2026-08-17 那份通用核规格**不约束**本骨架。

## 1. 目标

在 `dc/` 下新建一套**电表专用**数据中心骨架，对外按别名读写，对内按大类分发。第一期只交付：

1. `dc_read_alias` / `dc_write_alias` 及大类索引表
2. 各大类空入口（写路径无电量）
3. 变量类参数表（X-macro 单一维护点，对齐详细设计 7.4.2 的表意）

不实现真实存储、格式转换、tick、掉电。不删除现有 `include/`、`src/`、`meter/`。

## 2. 已锁定决策

| 主题 | 选择 |
|------|------|
| 与现码关系 | 忽略现有实现，绿地重写 |
| 定位 | 电表专用；不抽可单独链接的通用核 |
| 第一期范围 | 分发骨架 + 变量参数表；变量读写仍为空 |
| 别名 | `[大类 8 \| 小类 16 \| 分项 8]` |
| 符号 | 函数/宏/表结构体名单跟模板；标量用 `stdint` |
| 返回值 | `int16_t`；成功 = **字节数**；失败 = 负码；`usLen` = 成员个数 |
| 目录 | `dc/include`、`dc/src`；旧树暂留 |
| 变量表 | 跟 7.4.2，修正笔误；X-macro；偏移自动算 |
| 源文件 | 按大类拆分 |

## 3. 架构

调用方只看见别名 API。分发模块查表后进入大类函数。第一期大类函数立即返回 `DC_RET_UNSUPPORTED`。变量参数表是独立深模块：维护者只改 `.inc`，枚举、长度、偏移、映射表全部展开生成。

```text
应用
  └─ dc_read_alias / dc_write_alias     （dc_alias.c）
        ├─ dc_read_energy                 （空）
        ├─ Read/dc_write_demand           （空）
        ├─ Read/dc_write_param            （空）
        ├─ Read/dc_write_variable         （空；表在 dc_variable_table.inc）
        ├─ Read/dc_write_list        （空）
        └─ Read/dc_write_record           （空）
```

写表不含电量：电量别名走 `dc_write_alias` 时大类未命中，返回 `DC_RET_ALIAS_ERR`。

## 4. 目录

```text
dc/
  include/
    datacenter.h
    dc_alias.h
    dc_types.h
    dc_variable.h
  src/
    dc_alias.c
    dc_energy.c
    dc_demand.c
    dc_param.c
    dc_variable.c
    dc_variable_table.inc
    dc_list.c
    dc_record.c
```

CMake 独立目标（建议名 `dc_meter_fw`），不链接现有 `dc_core`。Host 测试只链该目标。

## 5. 对外接口

```c
int16_t dc_read_alias(uint32_t alias, uint8_t *dataPtr, uint16_t usLen, uint8_t type);
int16_t dc_write_alias(uint32_t alias, const uint8_t *dataPtr, uint16_t usLen, uint8_t type);
```

- 写缓冲为 `const uint8_t *`（对齐模板 3.2.2）
- `type` 第一期原样下传，空入口忽略
- `dataPtr == NULL` 且 `usLen != 0` → `DC_RET_PARAM_ERR`
- 成功本应返回传输字节数；第一期空入口在缓冲合法时仍返回 `DC_RET_UNSUPPORTED`，不写缓冲

### 5.1 别名

```c
#define GetAliasClass(a)     ((uint8_t)(((a) >> 24) & 0xFFu))
#define ParaAliasToType(a)   ((uint16_t)(((a) >> 8) & 0xFFFFu))
#define GetAliasIndex(a)     ((uint8_t)((a) & 0xFFu))

#define VarAliasBuild(varType, idx) \
    ((((uint32_t)ALIAS_CLASS_VARIABLE) << 24) + (((uint32_t)(varType)) << 8) + (uint32_t)(idx))
```

电量/需量/参变量/列表/记录的 `*AliasBuild` 同样提供，大类号 0..5：

`ENERGY=0, DEMAND=1, PARAMETER=2, VARIABLE=3, LISTPARAM=4, RECORD=5`

枚举名 `E_ALIAS_CLASS`（不使用模板笔误 `TPYE`）。

### 5.2 分发表

线性查找 `ucClassId == GetAliasClass(alias)`，命中则调用 `entry`。不把大类号直接当数组下标。

```c
typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, uint8_t *, uint16_t, uint8_t);
} STR_ALIAS_RSTORAGE_TABLE;

typedef struct {
    uint8_t ucClassId;
    int16_t (*entry)(uint32_t, const uint8_t *, uint16_t, uint8_t);
} STR_ALIAS_WSTORAGE_TABLE;
```

读表六项：电量、需量、参变量、变量、列表、记录。  
写表五项：需量、参变量、变量、列表、记录。

### 5.3 错误码

| 宏 | 值 | 含义 |
|----|----|------|
| `DC_RET_ALIAS_ERR` | -1 | 大类不在对应表中 |
| `DC_RET_UNSUPPORTED` | -2 | 大类已注册，入口未实现 |
| `DC_RET_PARAM_ERR` | -3 | 参数非法（空指针且 `usLen != 0`） |

## 6. 变量参数表

唯一维护文件：`dc/src/dc_variable_table.inc`。

四个列表宏：`VAR_LIST_A`、`VAR_LIST_B`、`VAR_LIST_C`、`VAR_LIST_D`。类别由列表名表达，行内不重复 A/B/C/D。

行格式：

```c
X(TOKEN, subclass_u16, member_count, bytes_per_member)
```

展开生成：

- `VARIABLE_<TOKEN> = subclass_u16`
- `VAR_<TOKEN>_LEN = member_count * bytes_per_member`
- 块内偏移：对该列表生成仅用于计算的 packed 布局（`uint8_t token[len]`），`offsetof` 得到 `VAR_<TOKEN>_ADDR`。禁止在 `.inc` 手写地址。
- `tVariableApiTable[]` 行：`{ VARIABLE_<TOKEN>, addr, len, member_count, bytes_per_member, 存储类 }`，顺序 A→B→C→D

A/B 在各项之后另有 **2 字节 CRC 槽**（不是小类）。C 无 CRC 槽。D 不进入 A/B/C 的 SRAM 布局，自身偏移从 0 起。

第一期**不**实例化 `STR_VARIABLE_RAM`，**不**做备份标志。`dc_read_variable` / `dc_write_variable` 返回 `DC_RET_UNSUPPORTED`。表本身编译进库，测试直接读 `tVariableApiTable` 与生成的 `ADDR`/`LEN`。

### 6.1 占位宽度

下列宽度第一期用带名常量，**不等于**电量模块里那些随宏变化的 `sizeof`。能量/密钥结构出现后再改常量；测试不断言这些宽度的绝对值，只断言同块内相对偏移。

| 常量 | 第一期值 | 用于 |
|------|----------|------|
| `DC_VAR_CALENDAR_BYTES` | 7 | 日期时间 |
| `DC_VAR_KEY_BYTES` | 8 | 更新前密钥状态 |
| `DC_VAR_ENERGY_PULSE_BYTES` | 16 | 当前电量能量尾数 |
| `DC_VAR_INTERVAL_PULSE_BYTES` | 8 | 当前区间电量能量尾数 |

### 6.2 表项（与 7.4.2 对齐，含修正）

**A**

| TOKEN | 小类 | n | 分项字节 |
|-------|------|---|----------|
| DATE_TIME | 0x4000 | 1 | `DC_VAR_CALENDAR_BYTES` |
| RUN_TIME | 0xE000 | 4 | 4 |
| WORK_TIME_BAT | 0x2013 | 1 | 4 |

**B**

| TOKEN | 小类 | n | 分项字节 |
|-------|------|---|----------|
| USED_MONTH | 0x2031 | 1 | 4 |
| ENERGY_DEC | 0xE007 | 1 | `DC_VAR_ENERGY_PULSE_BYTES` |
| INTVENY_DEC | 0xE008 | 1 | `DC_VAR_INTERVAL_PULSE_BYTES` |

**C**

| TOKEN | 小类 | n | 分项字节 |
|-------|------|---|----------|
| RMS_VOLTAGE | 0x2000 | 3 | 2 |
| RMS_CURRENT | 0x2001 | 5 | 4 |
| VOLT_ANGLE | 0x2002 | 3 | 2 |
| PHASE_ANGLE | 0x2003 | 3 | 2 |
| ACTIVE_POWER | 0x2004 | 4 | 4 |
| REACTIVE_POWER | 0x2005 | 4 | 4 |
| APPARENT_POWER | 0x2006 | 4 | 4 |
| ACTPOW_PERMIN | 0x2007 | 4 | 4 |
| REACTPOW_PERMIN | 0x2008 | 4 | 4 |
| POWER_FACT | 0x200A | 4 | 2 |
| POWER_FREQ | 0x200F | 1 | 2 |
| METER_TMP | 0x2010 | 1 | 2 |
| VOLT_BATTIM | 0x2011 | 1 | 2 |
| VOLT_BATDIS | 0x2012 | 1 | 2 |
| STAWDS_METER | 0x2014 | 7 | 2 |
| STAWDS_FLRPT | 0x2015 | 1 | 4 |
| DEMAND_ACTIVE | 0x2017 | 1 | 4 |
| DEMAND_REACTIVE | 0x2018 | 1 | 4 |
| WORK_FEENO | 0xE003 | 1 | 1 |
| RTC_SECMIN | 0xE205 | 2 | 4 |

**D**

| TOKEN | 小类 | n | 分项字节 |
|-------|------|---|----------|
| MTWORK_EVTKEY | 0xE100 | 1 | `DC_VAR_KEY_BYTES` |

修正相对模板原文：`E_VARIABLE_TPYE` → 生成名 `VARIABLE_<TOKEN>`；补上 `RTC_SECMIN` 长度；去掉映射表笔误逗号；D 不并入 A/B/C 布局。

映射表结构体名保留 `STR_VARIABLE_API_TABLE`，字段：小类、地址、总长、分项行数、分项字节、存储类（`VARIABLE_TYPEA`…`D`）。

## 7. 测试

Host 测试，只链 `dc_meter_fw`。

**分发**

- 未知大类 → `DC_RET_ALIAS_ERR`
- `dc_write_alias(电量别名, …)` → `DC_RET_ALIAS_ERR`
- 变量/需量等已注册入口 → `DC_RET_UNSUPPORTED`
- `dataPtr == NULL` 且 `usLen != 0` → `DC_RET_PARAM_ERR`

**参数表**

- 小类号与 §6.2 一致
- 同块内后一项地址 = 前一项地址 + 前一项长度
- A/B：最后一项之后 CRC 槽宽 2
- C 无 CRC 槽；D 地址为 0，且不落在 A/B/C 的 `offsetof` 布局里
- 不断言写死的绝对地址魔数（占位宽度可变）

## 8. 第一期不做

- 真实读写、FMT、SRAM/EE、tick、掉电、电量累加、清零
- 公开 `dc_id` / 通用核
- 删除旧 `include/`、`src/`、`meter/`

## 9. 风险与后续

- 占位宽度与将来电量/日历结构不一致：只改 `dc_types.h` 常量，偏移自动变。
- 下一期实现 `Read/dc_write_variable` 时：按小类查 `tVariableApiTable`，`usLen` 转字节，再碰存储。
