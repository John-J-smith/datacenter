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
 * 统一清单宏：pack 展开生成 _param_attr_*，固件 STRUCT/LIST 表由 layout 的 g_param_attr_* 提供。
 *
 * 共享属性表：用 PARAM_ATTR_*（tag）声明一次，用 PARAM_*_USE 引用。
 *
 * 固件（非 pack）包含 dc_param_cfg.h 时调用 PARAM_ITEM_LIST(_PARAM_ATTR_EMIT_)，
 * ARRAY/LINK 仍在此生成 _param_attr_*。dc_param_pack 在 load_param_items() 里用 PACK_ROW 展开同一清单。
 */

#define PARAM_LG(...) (__VA_ARGS__)

#define DC_PARAM_HEAD(n, ...) (n)
#define DC_PARAM_TAIL(n, ...) DC_PARAM_SUM(__VA_ARGS__)
#define DC_PARAM_TUP_N(t) DC_PARAM_HEAD t
#define DC_PARAM_TUP_SUM(t) DC_PARAM_TAIL t

#define DC_PARAM_LIST_XY(g, y) ((uint8_t)(((unsigned)(g) << 4) | (unsigned)(y)))

#define DC_PARAM_LIST_LEAVES_EXPAND2(n) DC_PARAM_LIST_LEAVES_##n
#define DC_PARAM_LIST_LEAVES_EXPAND1(n) DC_PARAM_LIST_LEAVES_EXPAND2(n)
#define DC_PARAM_LIST_LEAVES(g, ...) \
    DC_PARAM_LIST_LEAVES_EXPAND1(DC_PARAM_NARG(__VA_ARGS__))(g, __VA_ARGS__)

#define DC_PARAM_LIST_LEAVES_1(g, a) \
    DC_PARAM_LIST_XY((g), 0), (uint8_t)(a)
#define DC_PARAM_LIST_LEAVES_2(g, a, b) \
    DC_PARAM_LIST_XY((g), 0), (uint8_t)(a), DC_PARAM_LIST_XY((g), 1), (uint8_t)(b)
#define DC_PARAM_LIST_LEAVES_3(g, a, b, c) \
    DC_PARAM_LIST_LEAVES_2(g, a, b), DC_PARAM_LIST_XY((g), 2), (uint8_t)(c)
#define DC_PARAM_LIST_LEAVES_4(g, a, b, c, d) \
    DC_PARAM_LIST_LEAVES_3(g, a, b, c), DC_PARAM_LIST_XY((g), 3), (uint8_t)(d)
#define DC_PARAM_LIST_LEAVES_5(g, a, b, c, d, e) \
    DC_PARAM_LIST_LEAVES_4(g, a, b, c, d), DC_PARAM_LIST_XY((g), 4), (uint8_t)(e)
#define DC_PARAM_LIST_LEAVES_6(g, a, b, c, d, e, f) \
    DC_PARAM_LIST_LEAVES_5(g, a, b, c, d, e), DC_PARAM_LIST_XY((g), 5), (uint8_t)(f)
#define DC_PARAM_LIST_LEAVES_7(g, a, b, c, d, e, f, p) \
    DC_PARAM_LIST_LEAVES_6(g, a, b, c, d, e, f), DC_PARAM_LIST_XY((g), 6), (uint8_t)(p)
#define DC_PARAM_LIST_LEAVES_8(g, a, b, c, d, e, f, p, h) \
    DC_PARAM_LIST_LEAVES_7(g, a, b, c, d, e, f, p), DC_PARAM_LIST_XY((g), 7), (uint8_t)(h)
#define DC_PARAM_LIST_LEAVES_9(g, a, b, c, d, e, f, p, h, i) \
    DC_PARAM_LIST_LEAVES_8(g, a, b, c, d, e, f, p, h), DC_PARAM_LIST_XY((g), 8), (uint8_t)(i)
#define DC_PARAM_LIST_LEAVES_10(g, a, b, c, d, e, f, p, h, i, j) \
    DC_PARAM_LIST_LEAVES_9(g, a, b, c, d, e, f, p, h, i), DC_PARAM_LIST_XY((g), 9), (uint8_t)(j)
#define DC_PARAM_LIST_LEAVES_11(g, a, b, c, d, e, f, p, h, i, j, k) \
    DC_PARAM_LIST_LEAVES_10(g, a, b, c, d, e, f, p, h, i, j), DC_PARAM_LIST_XY((g), 10), (uint8_t)(k)
#define DC_PARAM_LIST_LEAVES_12(g, a, b, c, d, e, f, p, h, i, j, k, l) \
    DC_PARAM_LIST_LEAVES_11(g, a, b, c, d, e, f, p, h, i, j, k), DC_PARAM_LIST_XY((g), 11), (uint8_t)(l)
#define DC_PARAM_LIST_LEAVES_13(g, a, b, c, d, e, f, p, h, i, j, k, l, m) \
    DC_PARAM_LIST_LEAVES_12(g, a, b, c, d, e, f, p, h, i, j, k, l), DC_PARAM_LIST_XY((g), 12), (uint8_t)(m)
#define DC_PARAM_LIST_LEAVES_14(g, a, b, c, d, e, f, p, h, i, j, k, l, m, n) \
    DC_PARAM_LIST_LEAVES_13(g, a, b, c, d, e, f, p, h, i, j, k, l, m), DC_PARAM_LIST_XY((g), 13), (uint8_t)(n)
#define DC_PARAM_LIST_LEAVES_15(g, a, b, c, d, e, f, p, h, i, j, k, l, m, n, o) \
    DC_PARAM_LIST_LEAVES_14(g, a, b, c, d, e, f, p, h, i, j, k, l, m, n), DC_PARAM_LIST_XY((g), 14), (uint8_t)(o)

#define DC_PARAM_LIST_GROUP_APPLY(n, g, ...) DC_PARAM_LIST_LEAVES_##n(g, __VA_ARGS__)
#define DC_PARAM_LIST_GROUP_EXPAND(n, g, ...) DC_PARAM_LIST_GROUP_APPLY(n, g, __VA_ARGS__)
#define DC_PARAM_LIST_GROUP_I(g, n, ...) DC_PARAM_LIST_GROUP_EXPAND(n, g, __VA_ARGS__)
#define DC_PARAM_LIST_GROUP_Y(...) DC_PARAM_LIST_GROUP_I(__VA_ARGS__)
#define DC_PARAM_LIST_GROUP(g, tup) DC_PARAM_LIST_GROUP_Y(g, DC_PARAM_UNPAREN tup)

#define DC_PARAM_LIST_PAIRS_1(a1) DC_PARAM_LIST_GROUP(0, a1)
#define DC_PARAM_LIST_PAIRS_2(a1, a2) \
    DC_PARAM_LIST_PAIRS_1(a1), DC_PARAM_LIST_GROUP(1, a2)
#define DC_PARAM_LIST_PAIRS_3(a1, a2, a3) \
    DC_PARAM_LIST_PAIRS_2(a1, a2), DC_PARAM_LIST_GROUP(2, a3)
#define DC_PARAM_LIST_PAIRS_4(a1, a2, a3, a4) \
    DC_PARAM_LIST_PAIRS_3(a1, a2, a3), DC_PARAM_LIST_GROUP(3, a4)
#define DC_PARAM_LIST_PAIRS_5(a1, a2, a3, a4, a5) \
    DC_PARAM_LIST_PAIRS_4(a1, a2, a3, a4), DC_PARAM_LIST_GROUP(4, a5)
#define DC_PARAM_LIST_PAIRS_6(a1, a2, a3, a4, a5, a6) \
    DC_PARAM_LIST_PAIRS_5(a1, a2, a3, a4, a5), DC_PARAM_LIST_GROUP(5, a6)
#define DC_PARAM_LIST_PAIRS_7(a1, a2, a3, a4, a5, a6, a7) \
    DC_PARAM_LIST_PAIRS_6(a1, a2, a3, a4, a5, a6), DC_PARAM_LIST_GROUP(6, a7)
#define DC_PARAM_LIST_PAIRS_8(a1, a2, a3, a4, a5, a6, a7, a8) \
    DC_PARAM_LIST_PAIRS_7(a1, a2, a3, a4, a5, a6, a7), DC_PARAM_LIST_GROUP(7, a8)
#define DC_PARAM_LIST_PAIRS_9(a1, a2, a3, a4, a5, a6, a7, a8, a9) \
    DC_PARAM_LIST_PAIRS_8(a1, a2, a3, a4, a5, a6, a7, a8), DC_PARAM_LIST_GROUP(8, a9)
#define DC_PARAM_LIST_PAIRS_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
    DC_PARAM_LIST_PAIRS_9(a1, a2, a3, a4, a5, a6, a7, a8, a9), DC_PARAM_LIST_GROUP(9, a10)
#define DC_PARAM_LIST_PAIRS_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    DC_PARAM_LIST_PAIRS_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10), DC_PARAM_LIST_GROUP(10, a11)
#define DC_PARAM_LIST_PAIRS_12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
    DC_PARAM_LIST_PAIRS_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11), DC_PARAM_LIST_GROUP(11, a12)
#define DC_PARAM_LIST_PAIRS_13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
    DC_PARAM_LIST_PAIRS_12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12), DC_PARAM_LIST_GROUP(12, a13)
#define DC_PARAM_LIST_PAIRS_14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) \
    DC_PARAM_LIST_PAIRS_13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13), DC_PARAM_LIST_GROUP(13, a14)
#define DC_PARAM_LIST_PAIRS_15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) \
    DC_PARAM_LIST_PAIRS_14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14), DC_PARAM_LIST_GROUP(14, a15)
#define DC_PARAM_LIST_PAIRS_16(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16) \
    DC_PARAM_LIST_PAIRS_15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15), DC_PARAM_LIST_GROUP(15, a16)

#define DC_PARAM_UNPAREN(...) __VA_ARGS__

#define DC_PARAM_LIST_N_1(a1) DC_PARAM_TUP_N(a1)
#define DC_PARAM_LIST_N_2(a1, a2) (DC_PARAM_TUP_N(a1) + DC_PARAM_TUP_N(a2))
#define DC_PARAM_LIST_N_3(a1, a2, a3) (DC_PARAM_LIST_N_2(a1, a2) + DC_PARAM_TUP_N(a3))
#define DC_PARAM_LIST_N_4(a1, a2, a3, a4) (DC_PARAM_LIST_N_3(a1, a2, a3) + DC_PARAM_TUP_N(a4))
#define DC_PARAM_LIST_N_5(a1, a2, a3, a4, a5) (DC_PARAM_LIST_N_4(a1, a2, a3, a4) + DC_PARAM_TUP_N(a5))
#define DC_PARAM_LIST_N_6(a1, a2, a3, a4, a5, a6) \
    (DC_PARAM_LIST_N_5(a1, a2, a3, a4, a5) + DC_PARAM_TUP_N(a6))
#define DC_PARAM_LIST_N_7(a1, a2, a3, a4, a5, a6, a7) \
    (DC_PARAM_LIST_N_6(a1, a2, a3, a4, a5, a6) + DC_PARAM_TUP_N(a7))
#define DC_PARAM_LIST_N_8(a1, a2, a3, a4, a5, a6, a7, a8) \
    (DC_PARAM_LIST_N_7(a1, a2, a3, a4, a5, a6, a7) + DC_PARAM_TUP_N(a8))
#define DC_PARAM_LIST_N_9(a1, a2, a3, a4, a5, a6, a7, a8, a9) \
    (DC_PARAM_LIST_N_8(a1, a2, a3, a4, a5, a6, a7, a8) + DC_PARAM_TUP_N(a9))
#define DC_PARAM_LIST_N_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
    (DC_PARAM_LIST_N_9(a1, a2, a3, a4, a5, a6, a7, a8, a9) + DC_PARAM_TUP_N(a10))
#define DC_PARAM_LIST_N_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    (DC_PARAM_LIST_N_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) + DC_PARAM_TUP_N(a11))
#define DC_PARAM_LIST_N_12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
    (DC_PARAM_LIST_N_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) + DC_PARAM_TUP_N(a12))
#define DC_PARAM_LIST_N_13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
    (DC_PARAM_LIST_N_12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) + DC_PARAM_TUP_N(a13))
#define DC_PARAM_LIST_N_14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) \
    (DC_PARAM_LIST_N_13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) + DC_PARAM_TUP_N(a14))
#define DC_PARAM_LIST_N_15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) \
    (DC_PARAM_LIST_N_14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) + DC_PARAM_TUP_N(a15))
#define DC_PARAM_LIST_N_16(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16) \
    (DC_PARAM_LIST_N_15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) + DC_PARAM_TUP_N(a16))

#define DC_PARAM_LIST_SUM_1(a1) DC_PARAM_TUP_SUM(a1)
#define DC_PARAM_LIST_SUM_2(a1, a2) (DC_PARAM_TUP_SUM(a1) + DC_PARAM_TUP_SUM(a2))
#define DC_PARAM_LIST_SUM_3(a1, a2, a3) (DC_PARAM_LIST_SUM_2(a1, a2) + DC_PARAM_TUP_SUM(a3))
#define DC_PARAM_LIST_SUM_4(a1, a2, a3, a4) (DC_PARAM_LIST_SUM_3(a1, a2, a3) + DC_PARAM_TUP_SUM(a4))
#define DC_PARAM_LIST_SUM_5(a1, a2, a3, a4, a5) \
    (DC_PARAM_LIST_SUM_4(a1, a2, a3, a4) + DC_PARAM_TUP_SUM(a5))
#define DC_PARAM_LIST_SUM_6(a1, a2, a3, a4, a5, a6) \
    (DC_PARAM_LIST_SUM_5(a1, a2, a3, a4, a5) + DC_PARAM_TUP_SUM(a6))
#define DC_PARAM_LIST_SUM_7(a1, a2, a3, a4, a5, a6, a7) \
    (DC_PARAM_LIST_SUM_6(a1, a2, a3, a4, a5, a6) + DC_PARAM_TUP_SUM(a7))
#define DC_PARAM_LIST_SUM_8(a1, a2, a3, a4, a5, a6, a7, a8) \
    (DC_PARAM_LIST_SUM_7(a1, a2, a3, a4, a5, a6, a7) + DC_PARAM_TUP_SUM(a8))
#define DC_PARAM_LIST_SUM_9(a1, a2, a3, a4, a5, a6, a7, a8, a9) \
    (DC_PARAM_LIST_SUM_8(a1, a2, a3, a4, a5, a6, a7, a8) + DC_PARAM_TUP_SUM(a9))
#define DC_PARAM_LIST_SUM_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
    (DC_PARAM_LIST_SUM_9(a1, a2, a3, a4, a5, a6, a7, a8, a9) + DC_PARAM_TUP_SUM(a10))
#define DC_PARAM_LIST_SUM_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
    (DC_PARAM_LIST_SUM_10(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) + DC_PARAM_TUP_SUM(a11))
#define DC_PARAM_LIST_SUM_12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
    (DC_PARAM_LIST_SUM_11(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) + DC_PARAM_TUP_SUM(a12))
#define DC_PARAM_LIST_SUM_13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
    (DC_PARAM_LIST_SUM_12(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) + DC_PARAM_TUP_SUM(a13))
#define DC_PARAM_LIST_SUM_14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) \
    (DC_PARAM_LIST_SUM_13(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) + DC_PARAM_TUP_SUM(a14))
#define DC_PARAM_LIST_SUM_15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) \
    (DC_PARAM_LIST_SUM_14(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14) + DC_PARAM_TUP_SUM(a15))
#define DC_PARAM_LIST_SUM_16(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16) \
    (DC_PARAM_LIST_SUM_15(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15) + DC_PARAM_TUP_SUM(a16))

#define DC_PARAM_LIST_PAIRS_EXPAND2(ng) DC_PARAM_LIST_PAIRS_##ng
#define DC_PARAM_LIST_PAIRS_EXPAND(ng) DC_PARAM_LIST_PAIRS_EXPAND2(ng)
#define DC_PARAM_LIST_N_EXPAND2(ng) DC_PARAM_LIST_N_##ng
#define DC_PARAM_LIST_N_EXPAND(ng) DC_PARAM_LIST_N_EXPAND2(ng)
#define DC_PARAM_LIST_SUM_EXPAND2(ng) DC_PARAM_LIST_SUM_##ng
#define DC_PARAM_LIST_SUM_EXPAND(ng) DC_PARAM_LIST_SUM_EXPAND2(ng)

#if defined(DC_PARAM_PACK)
#define PARAM_LIST_ATTR(name, ...) \
    static const uint8_t _param_attr_##name[] = { \
        DATATYPE_LIST, \
        (uint8_t)(DC_PARAM_LIST_N_EXPAND(DC_PARAM_NARG(__VA_ARGS__))(__VA_ARGS__)), \
        DC_PARAM_LIST_PAIRS_EXPAND(DC_PARAM_NARG(__VA_ARGS__))(__VA_ARGS__) \
    };

#define PARAM_STRUCT_ATTR(name, n, ...) \
    static const uint8_t _param_attr_##name[] = { DATATYPE_STRUCT, (n), __VA_ARGS__ };
#else
/* 固件 STRUCT/LIST attrib 由 pack 写入 dc_param_layout.h 的 g_param_attr_* */
#define PARAM_LIST_ATTR(name, ...)
#define PARAM_STRUCT_ATTR(name, n, ...)
#endif

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

#define PARAM_LIST(X, ST, name, ...) \
    PARAM_LIST_ATTR(name, __VA_ARGS__) \
    X(name, DATATYPE_LIST, \
      DC_PARAM_LIST_SUM_EXPAND(DC_PARAM_NARG(__VA_ARGS__))(__VA_ARGS__), \
      (ST), _param_attr_##name)

/* 共享属性表：一个 tag，多行 PARAM_*_USE 引用。 */
#if defined(DC_PARAM_PACK)
#define PARAM_ATTR_STRUCT(tag, n, ...) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_STRUCT, (n), __VA_ARGS__ };
#else
#define PARAM_ATTR_STRUCT(tag, n, ...)
#endif

#define PARAM_ATTR_ARRAY(tag, n, elem) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_ARRAY, (n), (elem) };

#define PARAM_ATTR_LINK(tag, n, elem) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_LINKARRAY, 0u, 0u, (elem) };

#define PARAM_ATTR_LIST(tag, ...) PARAM_LIST_ATTR(tag, __VA_ARGS__)

#define PARAM_STRUCT_USE(X, ST, name, tag) \
    X(name, DATATYPE_STRUCT, PARAM_TOTAL_FROM_ATTR, (ST), _param_attr_##tag)

#define PARAM_ARRAY_USE(X, ST, name, n, elem, tag) \
    X(name, DATATYPE_ARRAY, (n) * (elem), (ST), _param_attr_##tag)

#define PARAM_LINK_USE(X, ST, name, n, elem, tag) \
    X(name, DATATYPE_LINKARRAY, (n) * (elem), (ST), _param_attr_##tag)

#define PARAM_LIST_USE(X, ST, name, tag) \
    X(name, DATATYPE_LIST, PARAM_TOTAL_FROM_ATTR, (ST), _param_attr_##tag)

#if !defined(DC_PARAM_PACK)
#define _PARAM_ATTR_EMIT_(name, dt, total, store, attr)
#endif

#endif /* DC_PARAM_CFG_MACROS_H */
