#include "dc_variable_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define OUT_CAP (128u * 1024u)

typedef struct {
    const char *name;
    uint16_t id;
    uint8_t n;
    uint8_t b;
} var_item_t;

#define PACK_A(tok, n, b) { #tok, 0u, (uint8_t)(n), (uint8_t)(b) },
#define PACK_B(tok, n, b) { #tok, 0u, (uint8_t)(n), (uint8_t)(b) },
#define PACK_C(tok, n, b) { #tok, 0u, (uint8_t)(n), (uint8_t)(b) },
#define PACK_D(tok, n, b) { #tok, 0u, (uint8_t)(n), (uint8_t)(b) },

static var_item_t s_a[] = { VAR_LIST_A(PACK_A) };
static var_item_t s_b[] = { VAR_LIST_B(PACK_B) };
static var_item_t s_c[] = { VAR_LIST_C(PACK_C) };
static var_item_t s_d[] = { VAR_LIST_D(PACK_D) };

#undef PACK_A
#undef PACK_B
#undef PACK_C
#undef PACK_D

static char s_out[OUT_CAP];
static size_t s_out_len;

static void die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void oappend(const char *s, size_t n)
{
    if (s_out_len + n >= OUT_CAP) {
        die("generated layout larger than buffer");
    }
    memcpy(s_out + s_out_len, s, n);
    s_out_len += n;
}

static void oputs(const char *s)
{
    oappend(s, strlen(s));
}

static void oprintf(const char *fmt, ...)
{
    char buf[1024];
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if ((n < 0) || ((size_t)n >= sizeof buf)) {
        die("format overflow");
    }
    oappend(buf, (size_t)n);
}

static int file_same(const char *path, const char *data, size_t len)
{
    FILE *fp;
    char *old;
    size_t got;
    int same;

    fp = fopen(path, "rb");
    if (fp == 0) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    if ((long)len != ftell(fp)) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    old = (char *)malloc(len);
    if (old == 0) {
        fclose(fp);
        die("out of memory");
    }
    got = fread(old, 1, len, fp);
    fclose(fp);
    same = (got == len) && (memcmp(old, data, len) == 0);
    free(old);
    return same;
}

static void write_if_changed(const char *path)
{
    FILE *fp;

    if (file_same(path, s_out, s_out_len)) {
        return;
    }
    fp = fopen(path, "wb");
    if (fp == 0) {
        die("cannot write %s", path);
    }
    if (fwrite(s_out, 1, s_out_len, fp) != s_out_len) {
        fclose(fp);
        die("write failed: %s", path);
    }
    fclose(fp);
}

static void assign_ids(var_item_t *items, unsigned nitems, uint16_t base, const char *cls)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        items[i].id = (uint16_t)(base + (uint16_t)i);
    }
    if (nitems > 0u) {
        uint16_t last;

        last = (uint16_t)(base + (uint16_t)(nitems - 1u));
        if (last < base) {
            die("%s ID range overflow (base=0x%04X count=%u)", cls, (unsigned)base,
                (unsigned)nitems);
        }
    }
}

static void assign_global_ids(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    uint16_t next;

    next = 0u;
    assign_ids(s_a, na, next, "A");
    next = (uint16_t)(next + (uint16_t)na);
    assign_ids(s_b, nb, next, "B");
    next = (uint16_t)(next + (uint16_t)nb);
    assign_ids(s_c, nc, next, "C");
    next = (uint16_t)(next + (uint16_t)nc);
    assign_ids(s_d, nd, next, "D");
    next = (uint16_t)(next + (uint16_t)nd);
    if (next == 0u) {
        return;
    }
    if ((unsigned)next != (na + nb + nc + nd)) {
        die("variable ID count overflow");
    }
}

static void emit_fields(const var_item_t *items, unsigned nitems)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        oprintf("    uint8_t %s[%uu];\n", items[i].name,
                (unsigned)((unsigned)items[i].n * (unsigned)items[i].b));
    }
}

static const char *stor_type_enum(uint8_t stor)
{
    switch (stor) {
    case 0u:
        return "VARIABLE_TYPEA";
    case 1u:
        return "VARIABLE_TYPEB";
    case 2u:
        return "VARIABLE_TYPEC";
    case 3u:
        return "VARIABLE_TYPED";
    default:
        return "VARIABLE_TYPEA";
    }
}

static void emit_api_rows(const var_item_t *items, unsigned nitems,
                          const char *layout, uint8_t stor, int *first_row)
{
    unsigned i;
    const char *type_enum;

    type_enum = stor_type_enum(stor);
    for (i = 0u; i < nitems; i++) {
        if (!*first_row) {
            oputs(",\n");
        }
        *first_row = 0;
        oprintf("    { (uint16_t)%s, (uint16_t)offsetof(%s, %s), %uu, %uu, %uu, (uint8_t)%s }",
                items[i].name,
                layout,
                items[i].name,
                (unsigned)((unsigned)items[i].n * (unsigned)items[i].b),
                (unsigned)items[i].n,
                (unsigned)items[i].b,
                type_enum);
    }
}

static void emit_all_api_rows(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    int first_row;

    first_row = 1;
    emit_api_rows(s_a, na, "var_layout_a_t", 0u, &first_row);
    emit_api_rows(s_b, nb, "var_layout_b_t", 1u, &first_row);
    emit_api_rows(s_c, nc, "var_layout_c_t", 2u, &first_row);
    emit_api_rows(s_d, nd, "var_layout_d_t", 3u, &first_row);
    oputs("\n");
}

static void emit_enum_rows(const var_item_t *items, unsigned nitems)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        oprintf("    %s = %uu", items[i].name, (unsigned)items[i].id);
        oputs(",\n");
    }
}

static uint16_t class_data_len(const var_item_t *items, unsigned nitems)
{
    unsigned i;
    uint16_t sum;

    sum = 0u;
    for (i = 0u; i < nitems; i++) {
        sum = (uint16_t)(sum + (uint16_t)((unsigned)items[i].n * (unsigned)items[i].b));
    }
    return sum;
}

static const char *stor_name(uint8_t stor)
{
    switch (stor) {
    case 0u:
        return "A";
    case 1u:
        return "B";
    case 2u:
        return "C";
    case 3u:
        return "D";
    default:
        return "?";
    }
}

static void dump_items(const var_item_t *items, unsigned nitems, uint8_t stor,
                       uint32_t ee_base, uint16_t a_ee_total, uint16_t b_ee_total)
{
    unsigned i;
    uint16_t off;

    off = 0u;
    for (i = 0u; i < nitems; i++) {
        uint16_t len;
        uint32_t ee;

        len = (uint16_t)((unsigned)items[i].n * (unsigned)items[i].b);
        if (stor == 2u) {
            ee = 0u;
        } else if (stor == 0u) {
            ee = ee_base + (uint32_t)off;
        } else if (stor == 1u) {
            ee = ee_base + (uint32_t)a_ee_total + (uint32_t)off;
        } else {
            ee = ee_base + (uint32_t)a_ee_total + (uint32_t)b_ee_total + (uint32_t)off;
        }

        printf("  %-24s id=%u stor=%s sram_off=%u len=%u n=%u b=%u",
               items[i].name,
               (unsigned)items[i].id,
               stor_name(stor),
               (unsigned)off,
               (unsigned)len,
               (unsigned)items[i].n,
               (unsigned)items[i].b);
        if (stor == 2u) {
            printf(" ee=n/a\n");
        } else {
            printf(" ee=0x%06lX\n", (unsigned long)ee);
        }
        off = (uint16_t)(off + len);
    }
}

static unsigned var_ee_banks_per_class(void)
{
#if (VAR_EE_BACKUP_BANKS >= 2)
    return 3u;
#else
    return 2u;
#endif
}

static void emit_ee_class_map(const char *cls, const char *end_sym)
{
    oputs("const uint16_t ");
    oprintf("%s_EE_BANK_SIZE = %s;\n", cls, end_sym);
    oprintf("const uint16_t %s_EE_PWR_ON_COUNT = %uu;\n", cls,
            (unsigned)VAR_EE_BACKUP_BANKS);
    oputs("const uint16_t ");
    oprintf("%s_EE_PWR_ON_0 = 0u;\n", cls);
#if (VAR_EE_BACKUP_BANKS >= 2)
    oputs("const uint16_t ");
    oprintf("%s_EE_PWR_ON_1 = %s;\n", cls, end_sym);
    oputs("const uint16_t ");
    oprintf("%s_EE_PWR_DWN = (uint16_t)(2u * %s);\n", cls, end_sym);
    oputs("const uint16_t ");
    oprintf("%s_EE_TOTAL = (uint16_t)(3u * %s);\n\n", cls, end_sym);
#else
    oputs("const uint16_t ");
    oprintf("%s_EE_PWR_DWN = %s;\n", cls, end_sym);
    oputs("const uint16_t ");
    oprintf("%s_EE_TOTAL = (uint16_t)(2u * %s);\n\n", cls, end_sym);
#endif
}

static void emit_eeprom_bases(void)
{
    oputs("const uint32_t VAR_A_EEPROM_BASE = (uint32_t)VAR_EEPROM_BASE;\n");
#if (VAR_EE_BACKUP_BANKS >= 2)
    oputs("const uint32_t VAR_B_EEPROM_BASE = (uint32_t)VAR_EEPROM_BASE + (uint32_t)(3u * VAR_A_END_ADDR);\n");
    oputs("const uint32_t VAR_D_EEPROM_BASE = (uint32_t)VAR_EEPROM_BASE + (uint32_t)(3u * VAR_A_END_ADDR) + (uint32_t)(3u * VAR_B_END_ADDR);\n");
    oputs("const uint16_t VAR_EE_TOTAL = (uint16_t)((3u * VAR_A_END_ADDR) + (3u * VAR_B_END_ADDR) + VAR_D_END_ADDR);\n\n");
#else
    oputs("const uint32_t VAR_B_EEPROM_BASE = (uint32_t)VAR_EEPROM_BASE + (uint32_t)(2u * VAR_A_END_ADDR);\n");
    oputs("const uint32_t VAR_D_EEPROM_BASE = (uint32_t)VAR_EEPROM_BASE + (uint32_t)(2u * VAR_A_END_ADDR) + (uint32_t)(2u * VAR_B_END_ADDR);\n");
    oputs("const uint16_t VAR_EE_TOTAL = (uint16_t)((2u * VAR_A_END_ADDR) + (2u * VAR_B_END_ADDR) + VAR_D_END_ADDR);\n\n");
#endif
}

static void emit_layout(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    s_out_len = 0u;
    oputs("/* Generated by dc_variable_pack. Do not edit. */\n");
    oputs("#ifndef DC_VARIABLE_LAYOUT_H\n");
    oputs("#define DC_VARIABLE_LAYOUT_H\n\n");
    oputs("#include \"dc_variable_cfg.h\"\n");
    oputs("#include <stddef.h>\n");
    oputs("#include <stdint.h>\n\n");

    oputs("typedef enum {\n");
    emit_enum_rows(s_a, na);
    emit_enum_rows(s_b, nb);
    emit_enum_rows(s_c, nc);
    emit_enum_rows(s_d, nd);
    if ((na + nb + nc + nd) == 0u) {
        oputs("    VARIABLE_ID_SENTINEL = 0\n");
    }
    oputs("} E_VARIABLE_ID;\n\n");

    oprintf("#define VAR_LIST_A_COUNT %uu\n", na);
    oprintf("#define VAR_LIST_B_COUNT %uu\n", nb);
    oprintf("#define VAR_LIST_C_COUNT %uu\n", nc);
    oprintf("#define VAR_LIST_D_COUNT %uu\n\n", nd);

    oputs("typedef struct {\n");
    emit_fields(s_a, na);
    oputs("    uint16_t crc;\n");
    oputs("} var_layout_a_t;\n\n");

    oputs("typedef struct {\n");
    emit_fields(s_b, nb);
    oputs("    uint16_t crc;\n");
    oputs("} var_layout_b_t;\n\n");

    oputs("typedef struct {\n");
    emit_fields(s_c, nc);
    oputs("} var_layout_c_t;\n\n");

    oputs("typedef struct {\n");
    emit_fields(s_d, nd);
    oputs("} var_layout_d_t;\n\n");

    oputs("#define VAR_A_CRC_ADDR (uint16_t)offsetof(var_layout_a_t, crc)\n");
    oputs("#define VAR_A_END_ADDR (uint16_t)sizeof(var_layout_a_t)\n");
    oputs("#define VAR_B_CRC_ADDR (uint16_t)offsetof(var_layout_b_t, crc)\n");
    oputs("#define VAR_B_END_ADDR (uint16_t)sizeof(var_layout_b_t)\n");
    oputs("#define VAR_C_END_ADDR (uint16_t)sizeof(var_layout_c_t)\n");
    oputs("#define VAR_D_END_ADDR (uint16_t)sizeof(var_layout_d_t)\n\n");

    oputs("#endif /* DC_VARIABLE_LAYOUT_H */\n\n");

    oputs("#if defined(DC_VARIABLE_LAYOUT_DEFINE)\n");
    oputs("#ifndef DC_VARIABLE_LAYOUT_TABLE_DEFINED\n");
    oputs("#define DC_VARIABLE_LAYOUT_TABLE_DEFINED\n\n");

    oputs("const ST_DC_VARIABLE_TABLE tVariableApiTable[] = {\n");
    emit_all_api_rows(na, nb, nc, nd);
    oputs("};\n\n");

    oputs("const uint16_t tVariableApiTableCount =\n");
    oputs("    (uint16_t)(sizeof(tVariableApiTable) / sizeof(tVariableApiTable[0]));\n\n");

    oputs("/* Contiguous EE map: one origin, A then B then D. */\n");
    emit_ee_class_map("VAR_A", "VAR_A_END_ADDR");
    emit_ee_class_map("VAR_B", "VAR_B_END_ADDR");

    oputs("const uint16_t VAR_D_EE_SIZE = VAR_D_END_ADDR;\n\n");

    emit_eeprom_bases();

    oputs("#endif /* DC_VARIABLE_LAYOUT_TABLE_DEFINED */\n");
    oputs("#endif /* DC_VARIABLE_LAYOUT_DEFINE */\n");
}

static void dump_layout(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    uint16_t a_end;
    uint16_t b_end;
    uint16_t c_end;
    uint16_t d_end;
    uint16_t a_ee_total;
    uint16_t b_ee_total;
    uint16_t ee_total;
    uint32_t ee_base;

    a_end = (uint16_t)(class_data_len(s_a, na) + 2u);
    b_end = (uint16_t)(class_data_len(s_b, nb) + 2u);
    c_end = class_data_len(s_c, nc);
    d_end = class_data_len(s_d, nd);
    a_ee_total = (uint16_t)(a_end * var_ee_banks_per_class());
    b_ee_total = (uint16_t)(b_end * var_ee_banks_per_class());
    ee_total = (uint16_t)(a_ee_total + b_ee_total + d_end);
    ee_base = (uint32_t)VAR_EEPROM_BASE;

    printf("=== variable layout (host dump) ===\n");
    printf("=== IDs: global 0..%u (A/B/C/D list order) ===\n",
           (unsigned)((na + nb + nc + nd) > 0u ? (na + nb + nc + nd - 1u) : 0u));
    printf("=== EE backup: %s (%u PWR_ON + 1 PWR_DWN per A/B) ===\n",
           (VAR_EE_BACKUP_BANKS >= 2) ? "dual" : "single",
           (unsigned)VAR_EE_BACKUP_BANKS);
    printf("=== SRAM block sizes ===\n");
    printf("  A: end=%u crc@%u (+ head/tail wrap in firmware)\n",
           (unsigned)a_end, (unsigned)(a_end - 2u));
    printf("  B: end=%u crc@%u\n", (unsigned)b_end, (unsigned)(b_end - 2u));
    printf("  C: end=%u\n", (unsigned)c_end);
    printf("  D: ee-only size=%u\n", (unsigned)d_end);

    printf("=== EE map (contiguous from VAR_EEPROM_BASE=0x%06lX) ===\n",
           (unsigned long)ee_base);
    printf("  total=%u\n", (unsigned)ee_total);
    printf("  A @+0 size=%u (bank=%u)\n", (unsigned)a_ee_total, (unsigned)a_end);
    printf("  B @+%u size=%u (bank=%u)\n",
           (unsigned)a_ee_total, (unsigned)b_ee_total, (unsigned)b_end);
    printf("  D @+%u size=%u\n",
           (unsigned)(a_ee_total + b_ee_total), (unsigned)d_end);

    printf("=== variable items (%u) ===\n", na + nb + nc + nd);
    dump_items(s_a, na, 0u, ee_base, a_ee_total, b_ee_total);
    dump_items(s_b, nb, 1u, ee_base, a_ee_total, b_ee_total);
    dump_items(s_c, nc, 2u, ee_base, a_ee_total, b_ee_total);
    dump_items(s_d, nd, 3u, ee_base, a_ee_total, b_ee_total);
}

int main(int argc, char **argv)
{
    const char *layout_path;
    unsigned na;
    unsigned nb;
    unsigned nc;
    unsigned nd;

    na = (unsigned)(sizeof s_a / sizeof s_a[0]);
    nb = (unsigned)(sizeof s_b / sizeof s_b[0]);
    nc = (unsigned)(sizeof s_c / sizeof s_c[0]);
    nd = (unsigned)(sizeof s_d / sizeof s_d[0]);

    assign_global_ids(na, nb, nc, nd);

    if (argc == 2 && strcmp(argv[1], "--dump") == 0) {
        dump_layout(na, nb, nc, nd);
        return 0;
    }

    if (argc != 2) {
        die("usage: dc_variable_pack <dc_variable_layout.h>\n"
            "       dc_variable_pack --dump");
    }
    layout_path = argv[1];

    emit_layout(na, nb, nc, nd);
    write_if_changed(layout_path);

    dump_layout(na, nb, nc, nd);
    return 0;
}
