#include <stdint.h>
#include "dc_variable_cfg.h"
#include "dc_storage_cfg.h"

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

static void replace_ext(const char *src, const char *ext, char *dst, size_t cap)
{
    const char *slash;
    const char *dot;
    int n;

    slash = strrchr(src, '/');
#ifdef _WIN32
    {
        const char *bslash = strrchr(src, '\\');

        if ((bslash != 0) && ((slash == 0) || (bslash > slash))) {
            slash = bslash;
        }
    }
#endif
    dot = strrchr(src, '.');
    if ((dot == 0) || ((slash != 0) && (dot < slash))) {
        n = snprintf(dst, cap, "%s%s", src, ext);
    } else {
        n = snprintf(dst, cap, "%.*s%s", (int)(dot - src), src, ext);
    }
    if ((n < 0) || ((size_t)n >= cap)) {
        die("path too long");
    }
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

static unsigned str_width(const char *s)
{
    return (unsigned)strlen(s);
}

static unsigned uint_text_width(unsigned v)
{
    char buf[16];

    return (unsigned)snprintf(buf, sizeof buf, "%uu", v);
}

typedef struct {
    unsigned name;
    unsigned len;
    unsigned n;
    unsigned b;
} var_tbl_cols_t;

static void var_col_max_item(const var_item_t *item, var_tbl_cols_t *c)
{
    unsigned total;
    unsigned w;

    w = str_width(item->name);
    if (w > c->name) {
        c->name = w;
    }
    total = (unsigned)((unsigned)item->n * (unsigned)item->b);
    w = uint_text_width(total);
    if (w > c->len) {
        c->len = w;
    }
    w = uint_text_width((unsigned)item->n);
    if (w > c->n) {
        c->n = w;
    }
    w = uint_text_width((unsigned)item->b);
    if (w > c->b) {
        c->b = w;
    }
}

static void var_col_max_list(const var_item_t *items, unsigned nitems, var_tbl_cols_t *c)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        var_col_max_item(&items[i], c);
    }
}

static var_tbl_cols_t var_api_col_widths(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    var_tbl_cols_t c;

    memset(&c, 0, sizeof c);
    var_col_max_list(s_a, na, &c);
    var_col_max_list(s_b, nb, &c);
    var_col_max_list(s_c, nc, &c);
    var_col_max_list(s_d, nd, &c);
    return c;
}

static void emit_api_rows(const var_item_t *items, unsigned nitems,
                          const char *layout, uint8_t stor,
                          const var_tbl_cols_t *c, unsigned *row_i,
                          unsigned row_total)
{
    unsigned i;
    const char *type_enum;
    char buf[16];

    type_enum = stor_type_enum(stor);
    for (i = 0u; i < nitems; i++) {
        oprintf("    { (uint16_t)%-*s, (uint16_t)offsetof(%s, %-*s), ",
                (int)c->name, items[i].name,
                layout,
                (int)c->name, items[i].name);
        snprintf(buf, sizeof buf, "%uu",
                 (unsigned)((unsigned)items[i].n * (unsigned)items[i].b));
        oprintf("%-*s, ", (int)c->len, buf);
        snprintf(buf, sizeof buf, "%uu", (unsigned)items[i].n);
        oprintf("%-*s, ", (int)c->n, buf);
        snprintf(buf, sizeof buf, "%uu", (unsigned)items[i].b);
        oprintf("%-*s, (uint8_t)%s", (int)c->b, buf, type_enum);
        (*row_i)++;
        if (*row_i < row_total) {
            oputs(" },\n");
        } else {
            oputs("  }\n");
        }
    }
}

static void emit_all_api_rows(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    var_tbl_cols_t c;
    unsigned row_i;
    unsigned row_total;

    c = var_api_col_widths(na, nb, nc, nd);
    row_total = na + nb + nc + nd;
    row_i = 0u;
    emit_api_rows(s_a, na, "var_layout_a_t", 0u, &c, &row_i, row_total);
    emit_api_rows(s_b, nb, "var_layout_b_t", 1u, &c, &row_i, row_total);
    emit_api_rows(s_c, nc, "var_layout_c_t", 2u, &c, &row_i, row_total);
    emit_api_rows(s_d, nd, "var_layout_d_t", 3u, &c, &row_i, row_total);
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


static void fmt_off(char *buf, size_t cap, uint32_t off)
{
    int n;

    n = snprintf(buf, cap, "0x%X(%u)", (unsigned)off, (unsigned)off);
    if ((n < 0) || ((size_t)n >= cap)) {
        die("fmt_off overflow");
    }
}

static void combined_md_path(const char *h_path, char *dst, size_t cap)
{
    const char *slash;
    size_t dir_len;
    int n;

    slash = strrchr(h_path, '/');
#ifdef _WIN32
    {
        const char *bslash = strrchr(h_path, '\\');

        if ((bslash != 0) && ((slash == 0) || (bslash > slash))) {
            slash = bslash;
        }
    }
#endif
    if (slash == 0) {
        n = snprintf(dst, cap, "dc_layout.md");
        if ((n < 0) || ((size_t)n >= cap)) {
            die("md path too long");
        }
        return;
    }
    dir_len = (size_t)(slash - h_path + 1);
    if (dir_len + 12u >= cap) {
        die("md path too long");
    }
    memcpy(dst, h_path, dir_len);
    memcpy(dst + dir_len, "dc_layout.md", 13u);
}

static void upsert_md_section(const char *md_path, const char *begin, const char *end,
                              const char *body)
{
    FILE *fp;
    char *old = 0;
    size_t old_len = 0u;
    char *out;
    size_t out_cap;
    size_t out_len = 0u;
    const char *post;
    size_t begin_len;
    size_t end_len;
    size_t body_len;
    char *bpos;
    char *epos;

    begin_len = strlen(begin);
    end_len = strlen(end);
    body_len = strlen(body);

    fp = fopen(md_path, "rb");
    if (fp != 0) {
        if (fseek(fp, 0, SEEK_END) == 0) {
            long sz = ftell(fp);

            if (sz > 0) {
                old_len = (size_t)sz;
                old = (char *)malloc(old_len + 1u);
                if (old == 0) {
                    fclose(fp);
                    die("out of memory");
                }
                if (fseek(fp, 0, SEEK_SET) != 0) {
                    free(old);
                    fclose(fp);
                    die("seek failed: %s", md_path);
                }
                if (fread(old, 1, old_len, fp) != old_len) {
                    free(old);
                    fclose(fp);
                    die("read failed: %s", md_path);
                }
                old[old_len] = '\0';
            }
        }
        fclose(fp);
    }

    out_cap = old_len + body_len + begin_len + end_len + 64u;
    out = (char *)malloc(out_cap);
    if (out == 0) {
        free(old);
        die("out of memory");
    }

    bpos = (old != 0) ? strstr(old, begin) : 0;
    epos = (bpos != 0) ? strstr(bpos, end) : 0;
    if ((bpos != 0) && (epos != 0)) {
        post = epos + end_len;
        while ((*post == '\r') || (*post == '\n')) {
            post++;
        }
        memcpy(out, old, (size_t)(bpos - old));
        out_len = (size_t)(bpos - old);
    } else if (old != 0) {
        memcpy(out, old, old_len);
        out_len = old_len;
        if ((out_len > 0u) && (out[out_len - 1u] != '\n')) {
            out[out_len++] = '\n';
        }
        if (out_len > 0u) {
            out[out_len++] = '\n';
        }
        post = 0;
    } else {
        out_len = 0u;
        post = 0;
    }

    if (out_len + begin_len + 1u + body_len + end_len + 2u >= out_cap) {
        free(old);
        free(out);
        die("md buffer overflow");
    }
    memcpy(out + out_len, begin, begin_len);
    out_len += begin_len;
    out[out_len++] = '\n';
    memcpy(out + out_len, body, body_len);
    out_len += body_len;
    if ((body_len == 0u) || (body[body_len - 1u] != '\n')) {
        out[out_len++] = '\n';
    }
    memcpy(out + out_len, end, end_len);
    out_len += end_len;
    out[out_len++] = '\n';

    if ((bpos != 0) && (epos != 0) && (old != 0)) {
        size_t post_len = (size_t)((old + old_len) - post);

        if (out_len + post_len >= out_cap) {
            free(old);
            free(out);
            die("md buffer overflow");
        }
        if (post_len > 0u) {
            memcpy(out + out_len, post, post_len);
            out_len += post_len;
        }
    }

    free(old);
    fp = fopen(md_path, "wb");
    if (fp == 0) {
        free(out);
        die("cannot write %s", md_path);
    }
    if (fwrite(out, 1, out_len, fp) != out_len) {
        fclose(fp);
        free(out);
        die("write failed: %s", md_path);
    }
    fclose(fp);
    free(out);
}

static char *read_section_file(const char *path, size_t *out_len)
{
    FILE *fp;
    char *body;
    long sz;

    fp = fopen(path, "rb");
    if (fp == 0) {
        die("cannot read %s", path);
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        die("seek failed: %s", path);
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        die("ftell failed: %s", path);
    }
    body = (char *)malloc((size_t)sz + 1u);
    if (body == 0) {
        fclose(fp);
        die("out of memory");
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        free(body);
        fclose(fp);
        die("seek failed: %s", path);
    }
    if (fread(body, 1, (size_t)sz, fp) != (size_t)sz) {
        free(body);
        fclose(fp);
        die("read failed: %s", path);
    }
    body[sz] = '\0';
    fclose(fp);
    *out_len = (size_t)sz;
    return body;
}

static char *dup_marked_section(const char *src, const char *begin, const char *end,
                                size_t *out_len)
{
    const char *b;
    const char *e;
    size_t n;
    char *d;

    *out_len = 0u;
    if (src == 0) {
        return 0;
    }
    b = strstr(src, begin);
    if (b == 0) {
        return 0;
    }
    e = strstr(b, end);
    if (e == 0) {
        return 0;
    }
    e += strlen(end);
    n = (size_t)(e - b);
    d = (char *)malloc(n + 1u);
    if (d == 0) {
        die("out of memory");
    }
    memcpy(d, b, n);
    d[n] = '\0';
    *out_len = n;
    return d;
}

static void append_md(char **dst, size_t *len, size_t *cap, const char *s, size_t n)
{
    if ((s == 0) || (n == 0u)) {
        return;
    }
    if (*len + n + 4u >= *cap) {
        *cap = (*len + n + 256u) * 2u;
        *dst = (char *)realloc(*dst, *cap);
        if (*dst == 0) {
            die("out of memory");
        }
    }
    memcpy(*dst + *len, s, n);
    *len += n;
}

static void reorder_layout_md(const char *md_path)
{
    char *old;
    size_t old_len;
    char *sv;
    char *sp;
    char *vv;
    char *pp;
    size_t lsv;
    size_t lsp;
    size_t lvv;
    size_t lpp;
    char *out;
    size_t out_len;
    size_t cap;
    FILE *fp;
    const char *hdr = "# 布局总览\n\n";

    old = read_section_file(md_path, &old_len);
    sv = dup_marked_section(old, "<!-- BEGIN:SUMMARY:VARIABLE -->",
                            "<!-- END:SUMMARY:VARIABLE -->", &lsv);
    sp = dup_marked_section(old, "<!-- BEGIN:SUMMARY:PARAM -->",
                            "<!-- END:SUMMARY:PARAM -->", &lsp);
    vv = dup_marked_section(old, "<!-- BEGIN:VARIABLE -->",
                            "<!-- END:VARIABLE -->", &lvv);
    pp = dup_marked_section(old, "<!-- BEGIN:PARAM -->",
                            "<!-- END:PARAM -->", &lpp);
    free(old);

    cap = strlen(hdr) + lsv + lsp + lvv + lpp + 32u;
    out = (char *)malloc(cap);
    if (out == 0) {
        die("out of memory");
    }
    out_len = 0u;
    append_md(&out, &out_len, &cap, hdr, strlen(hdr));
    if (sv != 0) {
        append_md(&out, &out_len, &cap, sv, lsv);
        append_md(&out, &out_len, &cap, "\n", 1u);
    }
    if (sp != 0) {
        append_md(&out, &out_len, &cap, sp, lsp);
        append_md(&out, &out_len, &cap, "\n", 1u);
    }
    if (vv != 0) {
        append_md(&out, &out_len, &cap, vv, lvv);
        append_md(&out, &out_len, &cap, "\n", 1u);
    }
    if (pp != 0) {
        append_md(&out, &out_len, &cap, pp, lpp);
        append_md(&out, &out_len, &cap, "\n", 1u);
    }
    free(sv);
    free(sp);
    free(vv);
    free(pp);

    fp = fopen(md_path, "wb");
    if (fp == 0) {
        free(out);
        die("cannot write %s", md_path);
    }
    if (fwrite(out, 1, out_len, fp) != out_len) {
        fclose(fp);
        free(out);
        die("write failed: %s", md_path);
    }
    fclose(fp);
    free(out);
}

static void dump_items(FILE *out, const var_item_t *items, unsigned nitems, uint8_t stor,
                       uint32_t ee_base, uint16_t bank)
{
    unsigned i;
    uint16_t off;
    char sram_s[32];
    char ee0_s[32];
    char ee1_s[32];
    char eed_s[32];

    off = 0u;
    for (i = 0u; i < nitems; i++) {
        uint16_t len;

        len = (uint16_t)((unsigned)items[i].n * (unsigned)items[i].b);
        fmt_off(sram_s, sizeof sram_s, (uint32_t)off);
        fprintf(out, "| %s | %u | %s | %s | %u | %u | %u |",
                items[i].name,
                (unsigned)items[i].id,
                stor_name(stor),
                sram_s,
                (unsigned)items[i].n,
                (unsigned)items[i].b,
                (unsigned)len);
        if (stor == 2u) {
            fprintf(out, " - | - | - |\n");
        } else if (stor == 3u) {
            fmt_off(ee0_s, sizeof ee0_s, ee_base + off);
            fprintf(out, " %s | - | - |\n", ee0_s);
        } else {
            fmt_off(ee0_s, sizeof ee0_s, ee_base + off);
            fmt_off(ee1_s, sizeof ee1_s, ee_base + (uint32_t)bank + off);
            fmt_off(eed_s, sizeof eed_s,
                    ee_base + (uint32_t)bank * (uint32_t)(VAR_EE_BACKUP_BANKS >= 2 ? 2u : 1u) + off);
            fprintf(out, " %s | %s | %s |\n", ee0_s, ee1_s, eed_s);
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
    oputs("#include \"dc_storage_cfg.h\"\n");
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

typedef struct {
    uint16_t a_end;
    uint16_t b_end;
    uint16_t c_end;
    uint16_t d_end;
    uint16_t a_ee_total;
    uint16_t b_ee_total;
    uint16_t ee_total;
    uint32_t a_base;
    uint32_t b_base;
    uint32_t d_base;
} var_sizes_t;

static void var_compute_sizes(unsigned na, unsigned nb, unsigned nc, unsigned nd,
                              var_sizes_t *sz)
{
    unsigned banks;

    sz->a_end = (uint16_t)(class_data_len(s_a, na) + 2u);
    sz->b_end = (uint16_t)(class_data_len(s_b, nb) + 2u);
    sz->c_end = class_data_len(s_c, nc);
    sz->d_end = class_data_len(s_d, nd);
    banks = var_ee_banks_per_class();
    sz->a_ee_total = (uint16_t)(sz->a_end * banks);
    sz->b_ee_total = (uint16_t)(sz->b_end * banks);
    sz->ee_total = (uint16_t)(sz->a_ee_total + sz->b_ee_total + sz->d_end);
    sz->a_base = 0u;
    sz->b_base = (uint32_t)sz->a_ee_total;
    sz->d_base = (uint32_t)sz->a_ee_total + (uint32_t)sz->b_ee_total;
}

static void dump_ee_rel_row(FILE *out, const char *name, unsigned ram,
                            uint32_t rel_off, uint32_t ee_sz)
{
    char s0[32];
    char s1[32];

    if (ee_sz == 0u) {
        fprintf(out, "| %s | %u | - | - | 0 |\n", name, ram);
        return;
    }
    fmt_off(s0, sizeof s0, rel_off);
    fmt_off(s1, sizeof s1, rel_off + ee_sz - 1u);
    fprintf(out, "| %s | %u | %s | %s | %u |\n",
            name, ram, s0, s1, (unsigned)ee_sz);
}

static void dump_summary(FILE *out, unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    var_sizes_t sz;
    unsigned ram_total;

    var_compute_sizes(na, nb, nc, nd, &sz);
    ram_total = (unsigned)sz.a_end + (unsigned)sz.b_end + (unsigned)sz.c_end;

    fprintf(out, "## 变量分类消耗\n\n");
    fprintf(out, "RAM 为工作区字节（A/B 含 CRC；不含 A/B 镜像 head/tail 魔数）。\n");
    fprintf(out, "EE 偏移相对 `VAR_EEPROM_BASE`，结束为末字节（含）。A/B 占用含全部备份槽。\n\n");
    fprintf(out, "| 类 | RAM | EE起始 | EE结束 | EE占用 |\n");
    fprintf(out, "|----|-----|--------|--------|--------|\n");
    dump_ee_rel_row(out, "A", (unsigned)sz.a_end, sz.a_base, (uint32_t)sz.a_ee_total);
    dump_ee_rel_row(out, "B", (unsigned)sz.b_end, sz.b_base, (uint32_t)sz.b_ee_total);
    dump_ee_rel_row(out, "C", (unsigned)sz.c_end, 0u, 0u);
    dump_ee_rel_row(out, "D", 0u, sz.d_base, (uint32_t)sz.d_end);
    dump_ee_rel_row(out, "合计", ram_total, 0u, (uint32_t)sz.ee_total);
    fprintf(out, "\n");
}

static void dump_layout(FILE *out, unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    var_sizes_t sz;
    uint16_t a_end;
    uint16_t b_end;
    uint16_t c_end;
    uint16_t d_end;
    uint16_t a_ee_total;
    uint16_t b_ee_total;
    uint16_t ee_total;
    uint32_t a_base;
    uint32_t b_base;
    uint32_t d_base;
    char off_s[32];
    char size_s[32];

    var_compute_sizes(na, nb, nc, nd, &sz);
    a_end = sz.a_end;
    b_end = sz.b_end;
    c_end = sz.c_end;
    d_end = sz.d_end;
    a_ee_total = sz.a_ee_total;
    b_ee_total = sz.b_ee_total;
    ee_total = sz.ee_total;
    a_base = sz.a_base;
    b_base = sz.b_base;
    d_base = sz.d_base;

    fprintf(out, "# 变量布局\n\n");
    fprintf(out, "## 存储类说明\n\n");
    fprintf(out, "| 类 | 说明 |\n");
    fprintf(out, "|----|------|\n");
    fprintf(out, "| A | SRAM 镜像 + EE 定时备份（PWR_ON_0 / PWR_ON_1 / PWR_DWN） |\n");
    fprintf(out, "| B | 存储类型同 A，但数据有变化时才刷到 EE |\n");
    fprintf(out, "| C | 仅 SRAM，无 EE |\n");
    fprintf(out, "| D | 仅 EE，无 SRAM 镜像 |\n\n");
    fprintf(out, "EE 偏移相对 `VAR_EEPROM_BASE`，格式 `十六进制(十进制)`。\n");
    fprintf(out, "A/B 的 `ee_off` 为 PWR_ON_0 槽内该条目偏移。\n");
    fprintf(out, "备份：%s（每类 %u×PWR_ON + 1×PWR_DWN）\n\n",
            (VAR_EE_BACKUP_BANKS >= 2) ? "双备份" : "单备份",
            (unsigned)VAR_EE_BACKUP_BANKS);

    fprintf(out, "## SRAM\n\n");
    fprintf(out, "| 区 | end | crc_off |\n");
    fprintf(out, "|----|-----|--------|\n");
    fmt_off(off_s, sizeof off_s, (uint32_t)a_end);
    fmt_off(size_s, sizeof size_s, (uint32_t)(a_end - 2u));
    fprintf(out, "| A | %s | %s |\n", off_s, size_s);
    fmt_off(off_s, sizeof off_s, (uint32_t)b_end);
    fmt_off(size_s, sizeof size_s, (uint32_t)(b_end - 2u));
    fprintf(out, "| B | %s | %s |\n", off_s, size_s);
    fmt_off(off_s, sizeof off_s, (uint32_t)c_end);
    fprintf(out, "| C | %s | - |\n", off_s);
    fmt_off(off_s, sizeof off_s, (uint32_t)d_end);
    fprintf(out, "| D | %s (ee-only) | - |\n\n", off_s);

    fprintf(out, "## EE 分区\n\n");
    fprintf(out, "| 类型 | 槽 | ee_offset | size |\n");
    fprintf(out, "|------|----|-----------|------|\n");
    fmt_off(off_s, sizeof off_s, a_base);
    fprintf(out, "| A | PWR_ON_0 | %s | %u |\n", off_s, (unsigned)a_end);
#if (VAR_EE_BACKUP_BANKS >= 2)
    fmt_off(off_s, sizeof off_s, a_base + a_end);
    fprintf(out, "| A | PWR_ON_1 | %s | %u |\n", off_s, (unsigned)a_end);
    fmt_off(off_s, sizeof off_s, a_base + 2u * a_end);
    fprintf(out, "| A | PWR_DWN | %s | %u |\n", off_s, (unsigned)a_end);
#else
    fmt_off(off_s, sizeof off_s, a_base + a_end);
    fprintf(out, "| A | PWR_DWN | %s | %u |\n", off_s, (unsigned)a_end);
#endif
    fmt_off(off_s, sizeof off_s, b_base);
    fprintf(out, "| B | PWR_ON_0 | %s | %u |\n", off_s, (unsigned)b_end);
#if (VAR_EE_BACKUP_BANKS >= 2)
    fmt_off(off_s, sizeof off_s, b_base + b_end);
    fprintf(out, "| B | PWR_ON_1 | %s | %u |\n", off_s, (unsigned)b_end);
    fmt_off(off_s, sizeof off_s, b_base + 2u * b_end);
    fprintf(out, "| B | PWR_DWN | %s | %u |\n", off_s, (unsigned)b_end);
#else
    fmt_off(off_s, sizeof off_s, b_base + b_end);
    fprintf(out, "| B | PWR_DWN | %s | %u |\n", off_s, (unsigned)b_end);
#endif
    fmt_off(off_s, sizeof off_s, d_base);
    fprintf(out, "| D | DATA | %s | %u |\n", off_s, (unsigned)d_end);
    fprintf(out, "| | **total** | | %u |\n\n", (unsigned)ee_total);

    fprintf(out, "## 条目\n\n");
    fprintf(out, "| name | id | class | ram_off | n | b | len | ee_bk0 | ee_bk1 | ee_pwrdwn |\n");
    fprintf(out, "|------|----|-------|---------|---|---|-----|--------|--------|-----------|\n");
    dump_items(out, s_a, na, 0u, a_base, a_end);
    dump_items(out, s_b, nb, 1u, b_base, b_end);
    dump_items(out, s_c, nc, 2u, 0u, 0u);
    dump_items(out, s_d, nd, 3u, d_base, 0u);
}

static void dump_layout_stdout(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    dump_summary(stdout, na, nb, nc, nd);
    dump_layout(stdout, na, nb, nc, nd);
}

static void dump_layout_md(const char *h_path, unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    char md_path[512];
    char tmp_path[540];
    char *body;
    size_t body_len;
    FILE *fp;
    int n;

    combined_md_path(h_path, md_path, sizeof md_path);
    n = snprintf(tmp_path, sizeof tmp_path, "%s.var.tmp", md_path);
    if ((n < 0) || ((size_t)n >= sizeof tmp_path)) {
        die("tmp path too long");
    }

    fp = fopen(tmp_path, "wb");
    if (fp == 0) {
        die("cannot write %s", tmp_path);
    }
    dump_summary(fp, na, nb, nc, nd);
    fclose(fp);
    body = read_section_file(tmp_path, &body_len);
    upsert_md_section(md_path,
                      "<!-- BEGIN:SUMMARY:VARIABLE -->",
                      "<!-- END:SUMMARY:VARIABLE -->",
                      body);
    free(body);

    fp = fopen(tmp_path, "wb");
    if (fp == 0) {
        die("cannot write %s", tmp_path);
    }
    dump_layout(fp, na, nb, nc, nd);
    fclose(fp);
    body = read_section_file(tmp_path, &body_len);
    remove(tmp_path);
    upsert_md_section(md_path,
                      "<!-- BEGIN:VARIABLE -->",
                      "<!-- END:VARIABLE -->",
                      body);
    free(body);
    reorder_layout_md(md_path);
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
        dump_layout_stdout(na, nb, nc, nd);
        return 0;
    }

    if (argc != 2) {
        die("usage: dc_variable_pack <dc_variable_layout.h>\n"
            "       dc_variable_pack --dump");
    }
    layout_path = argv[1];

    emit_layout(na, nb, nc, nd);
    write_if_changed(layout_path);

    dump_layout_stdout(na, nb, nc, nd);
    dump_layout_md(layout_path, na, nb, nc, nd);
    return 0;
}
