#include <stdint.h>
#include "dc_variable_cfg.h"

#define DC_PARAM_PACK
#include "dc_param.h"

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

typedef struct {
    const char *name;
    uint16_t id;
    uint8_t n;
} param_item_t;

#define PACK_VAR(tok, n, b) { #tok, 0u, (uint8_t)(n), (uint8_t)(b) },
#define PACK_PARAM(tok, dt, n, b, fl) \
    { #tok, 0u, (uint8_t)(n) },

static var_item_t s_a[] = { VAR_LIST_A(PACK_VAR) };
static var_item_t s_b[] = { VAR_LIST_B(PACK_VAR) };
static var_item_t s_c[] = { VAR_LIST_C(PACK_VAR) };
static var_item_t s_d[] = { VAR_LIST_D(PACK_VAR) };
static param_item_t s_params[] = { PARAM_ITEM_LIST(PACK_PARAM) };

#undef PACK_VAR
#undef PACK_PARAM

static void assign_param_ids(unsigned nparams)
{
    unsigned i;

    for (i = 0u; i < nparams; i++) {
        s_params[i].id = (uint16_t)i;
    }
}

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
        die("generated alias layout larger than buffer");
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
    char buf[512];
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if ((n < 0) || ((size_t)n >= sizeof buf)) {
        die("format overflow");
    }
    oappend(buf, (uint16_t)n);
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

static void assign_ids(var_item_t *items, unsigned nitems, uint16_t base)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        items[i].id = (uint16_t)(base + (uint16_t)i);
    }
}

static void assign_global_var_ids(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    uint16_t next;

    next = 0u;
    assign_ids(s_a, na, next);
    next = (uint16_t)(next + (uint16_t)na);
    assign_ids(s_b, nb, next);
    next = (uint16_t)(next + (uint16_t)nb);
    assign_ids(s_c, nc, next);
    next = (uint16_t)(next + (uint16_t)nc);
    assign_ids(s_d, nd, next);
}

static void var_alias_symbol(char *buf, size_t cap, const char *name, uint8_t n, unsigned idx)
{
    if (n == 1u) {
        snprintf(buf, cap, "DC_ALIAS_%s", name);
        return;
    }
    if (idx == (unsigned)n) {
        snprintf(buf, cap, "DC_ALIAS_%s_ALL", name);
        return;
    }
    if (n == 3u) {
        if (idx == 0u) {
            snprintf(buf, cap, "DC_ALIAS_%s_L1", name);
        } else if (idx == 1u) {
            snprintf(buf, cap, "DC_ALIAS_%s_L2", name);
        } else {
            snprintf(buf, cap, "DC_ALIAS_%s_L3", name);
        }
        return;
    }
    snprintf(buf, cap, "DC_ALIAS_%s_%u", name, (unsigned)idx);
}

static void param_alias_symbol(char *buf, size_t cap, const char *name, uint8_t n, unsigned idx)
{
    if (n == 1u) {
        snprintf(buf, cap, "DC_ALIAS_%s", name);
        return;
    }
    if (idx == (unsigned)n) {
        snprintf(buf, cap, "DC_ALIAS_%s_ALL", name);
        return;
    }
    snprintf(buf, cap, "DC_ALIAS_%s_%u", name, (unsigned)idx);
}

static unsigned var_alias_count(uint8_t n)
{
    if (n <= 1u) {
        return 1u;
    }
    return (unsigned)n + 1u;
}

static unsigned param_alias_count(uint8_t n)
{
    if (n <= 1u) {
        return 1u;
    }
    return (unsigned)n + 1u;
}

static void emit_alias_line_prefix(int *first, unsigned idx)
{
    if (idx > 0u) {
        oputs(",\n");
    }
    *first = 0;
}

static void emit_var_item_aliases(const var_item_t *item, int *first, int add_trailing_blank)
{
    unsigned count;
    unsigned i;
    char sym[128];

    count = var_alias_count(item->n);
    for (i = 0u; i < count; i++) {
        var_alias_symbol(sym, sizeof sym, item->name, item->n, i);
        emit_alias_line_prefix(first, i);
        if (item->n == 1u) {
            oprintf("    %s = VarAliasBuild(%s, 0u)", sym, item->name);
        } else if (i == 0u) {
            oprintf("    %s = VarAliasBuild(%s, 0u)", sym, item->name);
        } else if (i == (count - 1u)) {
            oprintf("    %s = VarAliasBuild(%s, 0xFFu)", sym, item->name);
        } else {
            oprintf("    %s", sym);
        }
    }
    if (add_trailing_blank != 0) {
        oputs(",\n\n");
    }
}

static void emit_var_aliases(const var_item_t *items, unsigned nitems, int *first)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        emit_var_item_aliases(&items[i], first, 1);
    }
}

static void emit_param_aliases(int *first)
{
    unsigned i;
    unsigned j;
    unsigned count;
    unsigned nparams;
    char sym[128];

    nparams = (unsigned)(sizeof s_params / sizeof s_params[0]);
    for (i = 0u; i < nparams; i++) {
        count = param_alias_count(s_params[i].n);
        for (j = 0u; j < count; j++) {
            param_alias_symbol(sym, sizeof sym, s_params[i].name, s_params[i].n, j);
            emit_alias_line_prefix(first, j);
            if (s_params[i].n == 1u) {
                oprintf("    %s = ParaAliasBuild(%s, 0u)", sym, s_params[i].name);
            } else if (j == 0u) {
                oprintf("    %s = ParaAliasBuild(%s, 0u)", sym, s_params[i].name);
            } else if (j == (count - 1u)) {
                oprintf("    %s = ParaAliasBuild(%s, 0xFFu)", sym, s_params[i].name);
            } else {
                oprintf("    %s", sym);
            }
        }
        if (i + 1u < nparams) {
            oputs(",\n\n");
        }
    }
}

static void emit_layout(unsigned na, unsigned nb, unsigned nc, unsigned nd)
{
    int first;

    s_out_len = 0u;
    oputs("/* Generated by dc_alias_pack. Do not edit. */\n");
    oputs("#ifndef DC_ALIAS_LAYOUT_H\n");
    oputs("#define DC_ALIAS_LAYOUT_H\n\n");
    oputs("#include \"dc_variable_layout.h\"\n");
    oputs("#include \"dc_param_layout.h\"\n\n");
    oputs("#ifndef VarAliasBuild\n");
    oputs("#error \"Include dc_alias.h before dc_alias_layout.h\"\n");
    oputs("#endif\n\n");
    oputs("typedef enum {\n");

    first = 1;
    emit_var_aliases(s_a, na, &first);
    emit_var_aliases(s_b, nb, &first);
    emit_var_aliases(s_c, nc, &first);
    emit_var_aliases(s_d, nd, &first);
    emit_param_aliases(&first);

    oputs(",\n\n    DC_ALIAS_SENTINEL = 0u\n");
    oputs("} E_DC_ALIAS;\n\n");
    oputs("#endif /* DC_ALIAS_LAYOUT_H */\n");
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

    assign_global_var_ids(na, nb, nc, nd);
    assign_param_ids((unsigned)(sizeof s_params / sizeof s_params[0]));

    if (argc != 2) {
        die("usage: dc_alias_pack <dc_alias_layout.h>");
    }
    layout_path = argv[1];

    emit_layout(na, nb, nc, nd);
    write_if_changed(layout_path);
    return 0;
}
