# dc_core 移植简要指南

可从 `port/dc_port_template.c` 复制一份产品端口，替换其中的 `TODO`。

## 1. 介质

为每个 Flash、EEPROM 或文件后端实现 `dc_media_ops_t`：

- `read`、`write` 成功时必须返回实际传输字节数（通常等于 `len`）。
- 失败时返回负的 `DC_ERR_*`；不支持擦除可返回 `DC_ERR_UNSUPPORTED`。
- 回调收到的 `off` 已包含 `dc_media_t.base`。端口仍应检查 `off + len`，并处理介质的擦写粒度、对齐和掉电约束。
- `ctx` 可保存驱动句柄；`capacity` 填可用区域字节数。

把配置好的 `dc_media_t` 填入 `dc_block_t.media`。需要写入的块还要提供
`ram_cache`；缓存至少为块的 `size`，启用 `DC_BLOCK_RAM_CRC16` 时再多留
2 字节。

## 2. noinit 缓存

需要跨热复位保留 RAM 镜像时，用 `DC_NOINIT` 声明缓存，并为块设置
`DC_BLOCK_NOINIT | DC_BLOCK_RAM_CRC16`。GNU 工具链会把该对象放入
`.noinit`；产品链接脚本必须把该段设为 `NOLOAD`，启动代码也不能清零它。

启动后 core 会校验缓存末尾的 CRC16。CRC 无效时从介质恢复，因此介质中
必须已有可读数据。冷启动、固件升级以及改变块布局时都应按 CRC 无效处理。
非 GNU 编译器上的 `DC_NOINIT` 默认为空，需在产品端改成对应的段属性。

## 3. 锁

CMake 的 `DC_LOCK` 可选：

- `NONE`：单线程或外部已串行化，`lock`/`unlock` 可为 `NULL`。
- `CRITICAL`：回调中进入/退出中断临界区；临界区应尽量短。
- `MUTEX`：回调中获取/释放递归规则明确的 RTOS/OS mutex，适合多线程。

将 `dc_os_port_t` 传给 `dc_init`。`lock` 与 `unlock` 必须成对提供，且不能在
持锁期间再次调用 dc_core API。`time_ms` 应返回单调递增毫秒值；当前不需要
时可为 `NULL`。

建议分别构建 `-DDC_LOCK=NONE`、`CRITICAL`、`MUTEX`，并在目标平台对介质
错误、热复位恢复和并发访问做集成测试。
