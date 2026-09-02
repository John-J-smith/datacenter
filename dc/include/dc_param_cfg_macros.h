#ifndef DC_PARAM_CFG_MACROS_H
#define DC_PARAM_CFG_MACROS_H

/* Storage presets for PARAM_ITEM_LIST rows (mapped to FLAG_* by dc_param_pack). */
#define PARAM_STORE_FULL   (FLAG_SRAM | FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK)
#define PARAM_STORE_EE_BK  (FLAG_ROM | FLAG_EEPROM | FLAG_EEPROM_BAK)
#define PARAM_STORE_ROM_EE (FLAG_ROM | FLAG_EEPROM)

#ifndef DC_PARAM_ATTR_INT_DEFINED
#define DC_PARAM_ATTR_INT_DEFINED
static const uint8_t _PARAM_ATTR_INT[] = { DATATYPE_INT };
#endif

/* Sum 1..16 field byte widths for PARAM_STRUCT total_len. */
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
 * Unified catalog macros: emit _param_attr_* and expand list row X(...) in one step.
 *
 * Shared ATTR: declare once with PARAM_ATTR_* (tag), reference with PARAM_*_USE.
 *
 * Firmware (non-pack) includes dc_param_cfg.h which calls
 * PARAM_ITEM_LIST(_PARAM_ATTR_EMIT_) so attrs exist at file scope.
 * dc_param_pack expands the same list with PACK_ROW inside load_param_items().
 */

#define PARAM_STRUCT_ATTR(name, n, ...) \
    static const uint8_t _param_attr_##name[] = { DATATYPE_STRUCT, (n), __VA_ARGS__ };

#define PARAM_ARRAY_ATTR(name, n, elem) \
    static const uint8_t _param_attr_##name[] = { DATATYPE_ARRAY, (n), (elem) };

#define PARAM_LINK_ATTR(name, n, elem) \
    static const uint8_t _param_attr_##name[] = { DATATYPE_LINKARRAY, 0u, 0u, (elem) };

/* Pack-only: total_len in X(...) — derive byte count from shared attrib (STRUCT *_USE). */
#define PARAM_TOTAL_FROM_ATTR  (0xFFFFu)

#define PARAM_INT(X, name, len, store) \
    X(name, DATATYPE_INT, (len), (store), _PARAM_ATTR_INT)

#define PARAM_STRUCT(X, name, store, n, ...) \
    PARAM_STRUCT_ATTR(name, n, __VA_ARGS__) \
    X(name, DATATYPE_STRUCT, DC_PARAM_SUM(__VA_ARGS__), (store), _param_attr_##name)

#define PARAM_ARRAY(X, name, n, elem, store) \
    PARAM_ARRAY_ATTR(name, n, elem) \
    X(name, DATATYPE_ARRAY, (n) * (elem), (store), _param_attr_##name)

#define PARAM_LINK(X, name, n, elem, store) \
    PARAM_LINK_ATTR(name, n, elem) \
    X(name, DATATYPE_LINKARRAY, (n) * (elem), (store), _param_attr_##name)

/* Shared attribute tables — one tag, many PARAM_*_USE rows. */
#define PARAM_ATTR_STRUCT(tag, n, ...) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_STRUCT, (n), __VA_ARGS__ };

#define PARAM_ATTR_ARRAY(tag, n, elem) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_ARRAY, (n), (elem) };

#define PARAM_ATTR_LINK(tag, n, elem) \
    static const uint8_t _param_attr_##tag[] = { DATATYPE_LINKARRAY, 0u, 0u, (elem) };

#define PARAM_STRUCT_USE(X, name, store, tag) \
    X(name, DATATYPE_STRUCT, PARAM_TOTAL_FROM_ATTR, (store), _param_attr_##tag)

#define PARAM_ARRAY_USE(X, name, n, elem, store, tag) \
    X(name, DATATYPE_ARRAY, (n) * (elem), (store), _param_attr_##tag)

#define PARAM_LINK_USE(X, name, n, elem, store, tag) \
    X(name, DATATYPE_LINKARRAY, (n) * (elem), (store), _param_attr_##tag)

#if !defined(DC_PARAM_PACK)
#define _PARAM_ATTR_EMIT_(name, dt, total, store, attr)
#endif

#endif /* DC_PARAM_CFG_MACROS_H */
