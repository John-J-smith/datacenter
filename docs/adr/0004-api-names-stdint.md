# 对外函数名跟模板，标量类型用 stdint

公开函数与分发表结构体仍用模板标识符（`ReadAliasData`、`WriteAliasData`、`STR_ALIAS_WSTORAGE_TABLE` 等），参数名保留 `genre`。标量类型改为 `stdint.h`（`uint8_t`/`uint16_t`/`uint32_t`），不再使用 `u8`/`mRet` 作为对外类型名，避免与将被替换的 `dc_*` 类型体系纠缠，同时让骨架仍能对照详细设计阅读。读写返回类型为 `int16_t`：成功为本次传输**字节数**；失败为负错误码。`usLen` 仍表示成员个数。不保留 `mRet` 别名。
