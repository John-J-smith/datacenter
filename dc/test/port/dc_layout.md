# 布局总览

<!-- BEGIN:SUMMARY:VARIABLE -->
## 变量分类消耗

RAM 为工作区字节（A/B 含 CRC；不含 A/B 镜像 head/tail 魔数）。
EE 偏移相对 `VAR_EEPROM_BASE`，结束为末字节（含）。A/B 占用含全部备份槽。

| 类 | RAM | EE起始 | EE结束 | EE占用 |
|----|-----|--------|--------|--------|
| A | 29 | 0x0(0) | 0x56(86) | 87 |
| B | 30 | 0x57(87) | 0xB0(176) | 90 |
| C | 169 | - | - | 0 |
| D | 0 | 0xB1(177) | 0xB8(184) | 8 |
| 合计 | 228 | 0x0(0) | 0xB8(184) | 185 |

<!-- END:SUMMARY:VARIABLE -->
<!-- BEGIN:SUMMARY:PARAM -->
## 参变量分类消耗

RAM 为 SRAM 工作区（compact：payload + CRC）；无 SRAM 的类型为 0。
EE 偏移相对 `PARAM_EEPROM_BASE`，结束为末字节（含）。
有 BAK 的类型另计备份槽（`PARAM_EE_TOTAL` + 主槽偏移）；`EE占用` 含主槽与备份槽。

| 类型 | RAM | 主槽起始 | 主槽结束 | 备份起始 | 备份结束 | EE占用 |
|------|-----|----------|----------|----------|----------|--------|
| RAM_EE_BK | 216 | 0x0(0) | 0xFF(255) | 0x280(640) | 0x37F(895) | 512 |
| EE_BK | 0 | 0x100(256) | 0x13F(319) | 0x380(896) | 0x3BF(959) | 128 |
| RAM_EE | 76 | 0x140(320) | 0x1BF(447) | - | - | 128 |
| EE | 0 | 0x1C0(448) | 0x23F(575) | - | - | 128 |
| 合计 | 292 | 0x0(0) | 0x23F(575) | 0x280(640) | 0x3BF(959) | 896 |

<!-- END:SUMMARY:PARAM -->
<!-- BEGIN:VARIABLE -->
# 变量布局

## 存储类说明

| 类 | 说明 |
|----|------|
| A | SRAM 镜像 + EE 定时备份（PWR_ON_0 / PWR_ON_1 / PWR_DWN） |
| B | 存储类型同 A，但数据有变化时才刷到 EE |
| C | 仅 SRAM，无 EE |
| D | 仅 EE，无 SRAM 镜像 |

EE 偏移相对 `VAR_EEPROM_BASE`，格式 `十六进制(十进制)`。
A/B 的 `ee_off` 为 PWR_ON_0 槽内该条目偏移。
备份：双备份（每类 2×PWR_ON + 1×PWR_DWN）

## SRAM

| 区 | end | crc_off |
|----|-----|--------|
| A | 0x1D(29) | 0x1B(27) |
| B | 0x1E(30) | 0x1C(28) |
| C | 0xA9(169) | - |
| D | 0x8(8) (ee-only) | - |

## EE 分区

| 类型 | 槽 | ee_offset | size |
|------|----|-----------|------|
| A | PWR_ON_0 | 0x0(0) | 29 |
| A | PWR_ON_1 | 0x1D(29) | 29 |
| A | PWR_DWN | 0x3A(58) | 29 |
| B | PWR_ON_0 | 0x57(87) | 30 |
| B | PWR_ON_1 | 0x75(117) | 30 |
| B | PWR_DWN | 0x93(147) | 30 |
| D | DATA | 0xB1(177) | 8 |
| | **total** | | 185 |

## 条目

| name | id | class | ram_off | n | b | len | ee_bk0 | ee_bk1 | ee_pwrdwn |
|------|----|-------|---------|---|---|-----|--------|--------|-----------|
| VAR_DATE_TIME | 0 | A | 0x0(0) | 1 | 7 | 7 | 0x0(0) | 0x1D(29) | 0x3A(58) |
| VAR_RUN_TIME | 1 | A | 0x7(7) | 4 | 4 | 16 | 0x7(7) | 0x24(36) | 0x41(65) |
| VAR_WORK_TIME_BAT | 2 | A | 0x17(23) | 1 | 4 | 4 | 0x17(23) | 0x34(52) | 0x51(81) |
| VAR_USED_MONTH | 3 | B | 0x0(0) | 1 | 4 | 4 | 0x57(87) | 0x75(117) | 0x93(147) |
| VAR_ENERGY_DEC | 4 | B | 0x4(4) | 1 | 16 | 16 | 0x5B(91) | 0x79(121) | 0x97(151) |
| VAR_INTVENY_DEC | 5 | B | 0x14(20) | 1 | 8 | 8 | 0x6B(107) | 0x89(137) | 0xA7(167) |
| VAR_RMS_VOLTAGE | 6 | C | 0x0(0) | 3 | 2 | 6 | - | - | - |
| VAR_RMS_CURRENT | 7 | C | 0x6(6) | 5 | 4 | 20 | - | - | - |
| VAR_VOLT_ANGLE | 8 | C | 0x1A(26) | 3 | 2 | 6 | - | - | - |
| VAR_PHASE_ANGLE | 9 | C | 0x20(32) | 3 | 2 | 6 | - | - | - |
| VAR_ACTIVE_POWER | 10 | C | 0x26(38) | 4 | 4 | 16 | - | - | - |
| VAR_REACTIVE_POWER | 11 | C | 0x36(54) | 4 | 4 | 16 | - | - | - |
| VAR_APPARENT_POWER | 12 | C | 0x46(70) | 4 | 4 | 16 | - | - | - |
| VAR_ACTPOW_PERMIN | 13 | C | 0x56(86) | 4 | 4 | 16 | - | - | - |
| VAR_REACTPOW_PERMIN | 14 | C | 0x66(102) | 4 | 4 | 16 | - | - | - |
| VAR_POWER_FACT | 15 | C | 0x76(118) | 4 | 2 | 8 | - | - | - |
| VAR_POWER_FREQ | 16 | C | 0x7E(126) | 1 | 2 | 2 | - | - | - |
| VAR_METER_TMP | 17 | C | 0x80(128) | 1 | 2 | 2 | - | - | - |
| VAR_VOLT_BATTIM | 18 | C | 0x82(130) | 1 | 2 | 2 | - | - | - |
| VAR_VOLT_BATDIS | 19 | C | 0x84(132) | 1 | 2 | 2 | - | - | - |
| VAR_STAWDS_METER | 20 | C | 0x86(134) | 7 | 2 | 14 | - | - | - |
| VAR_STAWDS_FLRPT | 21 | C | 0x94(148) | 1 | 4 | 4 | - | - | - |
| VAR_DEMAND_ACTIVE | 22 | C | 0x98(152) | 1 | 4 | 4 | - | - | - |
| VAR_DEMAND_REACTIVE | 23 | C | 0x9C(156) | 1 | 4 | 4 | - | - | - |
| VAR_WORK_FEENO | 24 | C | 0xA0(160) | 1 | 1 | 1 | - | - | - |
| VAR_RTC_SECMIN | 25 | C | 0xA1(161) | 2 | 4 | 8 | - | - | - |
| VAR_MTWORK_EVTKEY | 26 | D | 0x0(0) | 1 | 8 | 8 | 0xB1(177) | - | - |
<!-- END:VARIABLE -->
<!-- BEGIN:PARAM -->
# 参变量布局

## 存储类型说明

| 类型 | flags | 说明 |
|------|-------|------|
| RAM_EE_BK | SRAM+EE+BAK | SRAM 工作区；EE 备份区 1 + 备份区 2 |
| EE_BK | EE+BAK | 无 SRAM；EE 备份区 1 + 备份区 2 |
| RAM_EE | SRAM+EE | SRAM 工作区；仅 EE 备份区 1 |
| EE | EE | 无 SRAM；仅 EE 备份区 1 |

EE 偏移相对 `PARAM_EEPROM_BASE`。
条目 `ee_off` = 块主槽起点 + 块内字段偏移。
双备份：备份槽 = `PARAM_EE_TOTAL` + 主槽偏移（与固件 bak2 一致）。

## EE 分区

| 项 | bytes |
|----|-------|
| primary_raw | 0x240(576) |
| primary_aligned (`PARAM_EE_TOTAL`) | 0x280(640) |
| bak_span | 0x140(320) |
| map_end | 0x3C0(960) |

## 块

主槽编号 0..N-1；备份槽接在主槽之后继续编号。

| blk_id | role | of | store | compact | ee_off | blk_size |
|--------|------|----|-------|---------|--------|----------|
| 0 | primary | - | RAM_EE_BK | 60 | 0x0(0) | 64 |
| 1 | primary | - | RAM_EE_BK | 62 | 0x40(64) | 64 |
| 2 | primary | - | RAM_EE_BK | 62 | 0x80(128) | 64 |
| 3 | primary | - | RAM_EE_BK | 32 | 0xC0(192) | 64 |
| 4 | primary | - | EE_BK | 23 | 0x100(256) | 64 |
| 5 | primary | - | RAM_EE | 14 | 0x140(320) | 64 |
| 6 | primary | - | RAM_EE | 62 | 0x180(384) | 64 |
| 7 | primary | - | EE | 62 | 0x1C0(448) | 64 |
| 8 | primary | - | EE | 38 | 0x200(512) | 64 |
| 9 | bak | 0 | RAM_EE_BK | 60 | 0x280(640) | 64 |
| 10 | bak | 1 | RAM_EE_BK | 62 | 0x2C0(704) | 64 |
| 11 | bak | 2 | RAM_EE_BK | 62 | 0x300(768) | 64 |
| 12 | bak | 3 | RAM_EE_BK | 32 | 0x340(832) | 64 |
| 13 | bak | 4 | EE_BK | 23 | 0x380(896) | 64 |

## 条目

| name | id | store | type | idx | len | default | ee_bk1 | ee_bk2 |
|------|----|-------|------|-----|-----|---------|--------|--------|
| PARAM_REMOTECTRL | 0 | RAM_EE_BK | STRUCT | 2 | 6 |   | 0x0(0) | 0x280(640) |
| PARAM_LOCALCTRL | 1 | RAM_EE_BK | STRUCT | 2 | 6 |   | 0x6(6) | 0x286(646) |
| PARAM_TCP_UDP_SETUP | 2 | RAM_EE_BK | STRUCT | 5 | 39 |   | 0xC(12) | 0x28C(652) |
| PARAM_SEASON_SWTIME | 3 | RAM_EE_BK | INT | 1 | 7 | ✔ | 0x33(51) | 0x2B3(691) |
| PARAM_LINK_TEST | 4 | RAM_EE_BK | LINKARRAY | 5 | 150 |   | 0x40(64) | 0x2C0(704) |
| PARAM_DAY_SWTIME | 5 | EE_BK | INT | 1 | 7 | ✔ | 0x100(256) | 0x380(896) |
| PARAM_FEE_SWTIME | 6 | EE_BK | INT | 1 | 7 | ✔ | 0x107(263) | 0x387(903) |
| PARAM_LADDER_SWTIME | 7 | EE_BK | INT | 1 | 7 | ✔ | 0x10E(270) | 0x38E(910) |
| PARAM_UN | 8 | RAM_EE | INT | 1 | 4 |   | 0x140(320) | - |
| PARAM_IB | 9 | RAM_EE | INT | 1 | 4 |   | 0x144(324) | - |
| PARAM_IMAX | 10 | RAM_EE | INT | 1 | 4 |   | 0x148(328) | - |
| PARAM_HOLIDAY_DATA | 11 | RAM_EE | ARRAY | 5 | 60 |   | 0x180(384) | - |
| PARAM_CALIB_DATA | 12 | EE | LINKARRAY | 8 | 96 |   | 0x1C0(448) | - |
<!-- END:PARAM -->
