#ifndef DC_PARAM_CFG_MACROS_H
#define DC_PARAM_CFG_MACROS_H

/* RAM EE双备份 */
#define PARAM_STORE_RAM_EE_BK   (FLAG_SRAM | FLAG_EEPROM | FLAG_EEPROM_BAK)
/* RAM EE单备份 */
#define PARAM_STORE_RAM_EE      (FLAG_SRAM | FLAG_EEPROM)
/* EE双备份 */
#define PARAM_STORE_EE_BK       (FLAG_EEPROM | FLAG_EEPROM_BAK)
/* EE单备份 */
#define PARAM_STORE_EE          (FLAG_EEPROM)

#ifndef DC_PARAM_ATTR_INT_DEFINED
#define DC_PARAM_ATTR_INT_DEFINED
static const uint8_t _PARAM_ATTR_INT[] = { DATATYPE_INT };
#endif

/* 对 PARAM_STRUCT 各字段字节宽求和，得到 total_len（最多 16 项）。 */
#define DC_PARAM_NARG_( \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N
#define DC_PARAM_NARG(...) \
    DC_PARAM_NARG_(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define DC_PARAM_SUM_1(a) (a)
#define DC_PARAM_SUM_2(a, b) ((a) + (b))
#define DC_PARAM_SUM_3(a, b, c) ((a) + (b) + (c))
#define DC_PARAM_SUM_4(a, b, c, d) ((a) + (b) + (c) + (d))
#define DC_PARAM_SUM_5(a, b, c, d, e) ((a) + (b) + (c) + (d) + (e))
#define DC_PARAM_SUM_6(a, b, c, d, e, f) ((a) + (b) + (c) + (d) + (e) + (f))
#define DC_PARAM_SUM_7(a, b, c, d, e, f, g) ((a) + (b) + (c) + (d) + (e) + (f) + (g))
#define DC_PARAM_SUM_8(a, b, c, d, e, f, g, h) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h))
#define DC_PARAM_SUM_9(a, b, c, d, e, f, g, h, i) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i))
#define DC_PARAM_SUM_10(a, b, c, d, e, f, g, h, i, j) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i) + (j))
#define DC_PARAM_SUM_11(a, b, c, d, e, f, g, h, i, j, k) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i) + (j) + (k))
#define DC_PARAM_SUM_12(a, b, c, d, e, f, g, h, i, j, k, l) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i) + (j) + (k) + (l))
#define DC_PARAM_SUM_13(a, b, c, d, e, f, g, h, i, j, k, l, m) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i) + (j) + (k) + (l) + (m))
#define DC_PARAM_SUM_14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i) + (j) + (k) + (l) + (m) + (n))
#define DC_PARAM_SUM_15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i) + (j) + (k) + (l) + (m) + (n) + (o))
#define DC_PARAM_SUM_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
    ((a) + (b) + (c) + (d) + (e) + (f) + (g) + (h) + (i) + (j) + (k) + (l) + (m) + (n) + (o) + (p))
#define DC_PARAM_SUM_EXPAND2(N) DC_PARAM_SUM_##N
#define DC_PARAM_SUM_EXPAND1(N) DC_PARAM_SUM_EXPAND2(N)
#define DC_PARAM_SUM(...) \
    DC_PARAM_SUM_EXPAND1(DC_PARAM_NARG(__VA_ARGS__))(__VA_ARGS__)

/*
 * 统一清单宏：一次展开同时生成 _param_attr_* 并输出 X(...) 行。
 *
 * 共享属性表：用 PARAM_ATTR_*（tag）声明一次，用 PARAM_*_USE 引用。
 *
 * 固件（非 pack）包含 dc_param_cfg.h 时调用 PARAM_ITEM_LIST(_PARAM_ATTR_EMIT_)，
 * 在文件作用域生成属性表。dc_param_pack 在 load_param_items() 里用 PACK_ROW 展开同一清单。
 */

#define PARAM_STRUCT_ATTR(name, n, ...) \
    static const uint8_t _param_attr_##name[] = { DATATYPE_STRUCT, (n), __VA_ARGS__ };

#define PARAM_ARRAY_ATTR(name, n, elem) \
    static const uint8_t _param_attr_##name[] = { DATATYPE_ARRAY, (n), (elem) };

#define PARAM_LINK_ATTR(name, n, elem) \
    static const uint8_t _param_attr_##name[] = { DATATYPE_LINKARRAY, 0u, 0u, (elem) };

/* 仅 pack：X(...) 的 total_len 哨兵，从共享属性表推导字节数（STRUCT *_USE）。 */
#define PARAM_TOTAL_FROM_ATTR  (0xFFFFu)

/* ST 为清单绑定的存储类型（PARAM_ITEM_LIST_*_ROWS 的第二参数），行内不要写 PARAM_STORE_*。 */
#define PARAM_INT(X, ST, name, len) \
    X(name, DATATYPE_INT, (len), (ST), _PARAM_ATTR_INT)

#define PARAM_STRUCT(X, ST, name, n, ...) \
    PARAM_STRUCT_ATTR(name, n, __VA_ARGS__) \
    X(name, DATATYPE_STRUCT, DC_PARAM_SUM(__VA_ARGS__), (ST), _param_attr_##name)

#define PARAM_ARRAY(X, ST, name, n, elem) \
    PARAM_ARRAY_ATTR(name, n, elem) \
    X(name, DATATYPE_ARRAY, (n) * (elem), (ST), _param_attr_##name)

#define PARAM_LINK(X, ST, name, n, elem) \
    PARAM_LINK_ATTR(name, n, elem) \
    X(name, DATATYPE_LINKARRAY, (n) * (elem), (ST), _param_attr_##name)

/* 共享属性表：一个 tag，多行 PARAM_*_USE 引用。 */
#define PARAM_ATTR_STRUCT(tag, n, ...) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_STRUCT, (n), __VA_ARGS__ };

#define PARAM_ATTR_ARRAY(tag, n, elem) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_ARRAY, (n), (elem) };

#define PARAM_ATTR_LINK(tag, n, elem) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_LINKARRAY, 0u, 0u, (elem) };

#define PARAM_STRUCT_USE(X, ST, name, tag) \
    X(name, DATATYPE_STRUCT, PARAM_TOTAL_FROM_ATTR, (ST), _param_attr_##tag)

#define PARAM_ARRAY_USE(X, ST, name, n, elem, tag) \
    X(name, DATATYPE_ARRAY, (n) * (elem), (ST), _param_attr_##tag)

#define PARAM_LINK_USE(X, ST, name, n, elem, tag) \
    X(name, DATATYPE_LINKARRAY, (n) * (elem), (ST), _param_attr_##tag)

#if !defined(DC_PARAM_PACK)
#define _PARAM_ATTR_EMIT_(name, dt, total, store, attr)
#endif

#endif /* DC_PARAM_CFG_MACROS_H */
