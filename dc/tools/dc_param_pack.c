#define DC_PARAM_PACK
#include "dc_param.h"
#include "dc_param_attr.h"
#include "dc_storage_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define PACK_MAX_ITEMS 64
#define PACK_MAX_BLOCKS 64
#define OUT_CAP (256u * 1024u)

typedef struct {
    const char *name;
    uint16_t id;
    uint8_t dtype;
    uint16_t total_len;
    uint8_t flags;
    const char *attr_sym;
    const uint8_t *attr;
    uint8_t index_count;
    uint8_t elem_bytes;
    uint8_t link_n;
    uint8_t link_m;
    uint8_t link_k;
    uint8_t resolved_attr[4];
    uint8_t attr_resolved;
} pack_item_t;

#define PACK_ROW(tok, dt, total, fl, atab) \
    do { \
        if (s_expect_on && ((uint8_t)(fl) != s_expect_store)) { \
            die("%s: store does not match list class", #tok); \
        } \
        if (s_nitems >= PACK_MAX_ITEMS) { \
            die("too many param items"); \
        } \
        s_items[s_nitems].name = #tok; \
        s_items[s_nitems].id = 0u; \
        s_items[s_nitems].dtype = (uint8_t)(dt); \
        s_items[s_nitems].total_len = (uint16_t)(total); \
        s_items[s_nitems].flags = (uint8_t)(fl); \
        s_items[s_nitems].attr_sym = #atab; \
        s_items[s_nitems].attr = (const uint8_t *)(atab); \
        s_items[s_nitems].index_count = 0u; \
        s_items[s_nitems].elem_bytes = 0u; \
        s_items[s_nitems].link_n = 0u; \
        s_items[s_nitems].link_m = 0u; \
        s_items[s_nitems].link_k = 0u; \
        s_items[s_nitems].attr_resolved = 0u; \
        s_nitems++; \
    } while (0);

static pack_item_t s_items[PACK_MAX_ITEMS];
static unsigned s_nitems;
static uint8_t s_expect_store;
static int s_expect_on;

static void die(const char *fmt, ...);

static void load_param_items(void)
{
    s_nitems = 0u;
    s_expect_on = 1;
    s_expect_store = (uint8_t)PARAM_STORE_RAM_EE_BK;
    PARAM_ITEM_LIST_RAM_EE_BK_ROWS(PACK_ROW, PARAM_STORE_RAM_EE_BK)
    s_expect_store = (uint8_t)PARAM_STORE_EE_BK;
    PARAM_ITEM_LIST_EE_BK_ROWS(PACK_ROW, PARAM_STORE_EE_BK)
    s_expect_store = (uint8_t)PARAM_STORE_RAM_EE;
    PARAM_ITEM_LIST_RAM_EE_ROWS(PACK_ROW, PARAM_STORE_RAM_EE)
    s_expect_store = (uint8_t)PARAM_STORE_EE;
    PARAM_ITEM_LIST_EE_ROWS(PACK_ROW, PARAM_STORE_EE)
    s_expect_on = 0;
}

#undef PACK_ROW

static void assign_param_ids(unsigned nitems)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        s_items[i].id = (uint16_t)i;
    }
}

static unsigned struct_attr_sum(const uint8_t *attr)
{
    unsigned i;
    unsigned n;
    unsigned sum;

    n = (unsigned)attr[1];
    sum = 0u;
    for (i = 0u; i < n; i++) {
        sum += (unsigned)attr[2u + i];
    }
    return sum;
}

static void resolve_item_attr(pack_item_t *item, uint16_t payload_max)
{
    const uint8_t *attr;
    unsigned dt_attr;

    if (item->attr == 0) {
        die("%s: missing attrib table", item->name);
    }
    attr = item->attr;
    dt_attr = (unsigned)attr[0];
    if (dt_attr != (unsigned)item->dtype) {
        die("%s: dtype %u != attrib type %u", item->name,
            (unsigned)item->dtype, dt_attr);
    }

    switch ((E_PARAM_STORAGE_DATATYPE)item->dtype) {
    case DATATYPE_INT:
        if (item->total_len == 0u) {
            die("%s: INT total_len 0", item->name);
        }
        item->index_count = 1u;
        item->elem_bytes = (uint8_t)item->total_len;
        break;

    case DATATYPE_ARRAY:
        if (attr[1] == 0u || attr[2] == 0u) {
            die("%s: ARRAY attrib n/b zero", item->name);
        }
        if ((unsigned)attr[1] * (unsigned)attr[2] != (unsigned)item->total_len) {
            die("%s: ARRAY %u*%u != total_len %u", item->name,
                (unsigned)attr[1], (unsigned)attr[2], (unsigned)item->total_len);
        }
        item->index_count = attr[1];
        item->elem_bytes = attr[2];
        break;

    case DATATYPE_STRUCT: {
        unsigned sum;

        if (attr[1] == 0u) {
            die("%s: STRUCT member count 0", item->name);
        }
        sum = struct_attr_sum(attr);
        if (item->total_len == PARAM_TOTAL_FROM_ATTR) {
            item->total_len = (uint16_t)sum;
        } else if (sum != (unsigned)item->total_len) {
            die("%s: STRUCT field sum %u != total_len %u", item->name,
                sum, (unsigned)item->total_len);
        }
        if (sum > (unsigned)payload_max) {
            die("%s: STRUCT %u bytes exceeds payload %u", item->name,
                sum, (unsigned)payload_max);
        }
        item->index_count = attr[1];
        item->elem_bytes = 0u;
        break;
    }

    case DATATYPE_LINKARRAY: {
        unsigned k;
        unsigned nrec;
        unsigned per_page;
        unsigned npage;

        if (attr[3] == 0u) {
            die("%s: LINKARRAY K zero", item->name);
        }
        k = (unsigned)attr[3];
        if ((attr[1] == 0u) && (attr[2] == 0u)) {
            uint8_t np8;
            uint8_t pp8;
            uint16_t nr16;

            if (param_linkarray_dims(item->total_len, (uint8_t)k, payload_max,
                                     &np8, &pp8, &nr16) == 0) {
                die("%s: LINKARRAY cannot pack total=%u k=%u payload=%u",
                    item->name, (unsigned)item->total_len, (unsigned)k,
                    (unsigned)payload_max);
            }
            npage = (unsigned)np8;
            per_page = (unsigned)pp8;
            nrec = (unsigned)nr16;
            item->resolved_attr[0] = (uint8_t)DATATYPE_LINKARRAY;
            item->resolved_attr[1] = np8;
            item->resolved_attr[2] = pp8;
            item->resolved_attr[3] = (uint8_t)k;
            item->attr_resolved = 1u;
        } else {
            if (attr[1] == 0u || attr[2] == 0u) {
                die("%s: LINKARRAY attrib N/M zero", item->name);
            }
            if ((unsigned)attr[1] * (unsigned)attr[2] * k != (unsigned)item->total_len) {
                die("%s: LINKARRAY N*M*K != total_len %u", item->name,
                    (unsigned)item->total_len);
            }
            nrec = (unsigned)item->total_len / k;
            per_page = (unsigned)payload_max / k;
            if (per_page == 0u) {
                die("%s: record %u exceeds payload %u", item->name,
                    (unsigned)k, (unsigned)payload_max);
            }
            npage = (nrec + per_page - 1u) / per_page;
            if (npage != (unsigned)attr[1]) {
                die("%s: LINKARRAY N=%u expected %u", item->name,
                    (unsigned)attr[1], (unsigned)npage);
            }
            if ((unsigned)attr[2] != per_page) {
                die("%s: LINKARRAY M=%u expected %u", item->name,
                    (unsigned)attr[2], (unsigned)per_page);
            }
        }
        item->link_n = (uint8_t)npage;
        item->link_m = (uint8_t)per_page;
        item->link_k = (uint8_t)k;
        item->index_count = (uint8_t)nrec;
        item->elem_bytes = (uint8_t)k;
        break;
    }

    case DATATYPE_LIST:
        die("%s: DATATYPE_LIST not supported", item->name);

    default:
        die("%s: unknown dtype %u", item->name, (unsigned)item->dtype);
    }
}

#define DEF_LOOKUP(tok) \
    if (strcmp(name, #tok) == 0) { \
        *len = sizeof(tok##_def); \
        return tok##_def; \
    }

static const uint8_t *lookup_def(const char *name, size_t *len)
{
    PARAM_ITEM_DEFAULTS(DEF_LOOKUP)
    *len = 0u;
    return 0;
}

#undef DEF_LOOKUP

typedef struct {
    uint8_t flags;
    uint16_t payload;
    uint8_t rom[PARAM_BLOCK_SIZE];
    unsigned nfields;
    unsigned item_i[PACK_MAX_ITEMS];
    uint16_t field_len[PACK_MAX_ITEMS];
    unsigned field_def_off[PACK_MAX_ITEMS];
} pack_block_t;

typedef struct {
    uint8_t blk;
    uint16_t off;
    uint8_t blk_len;
} pack_place_t;

static char s_out[OUT_CAP];
static size_t s_out_len;

static void fill_item_bytes(uint8_t *dst, uint16_t sz,
                            const uint8_t *def, size_t deflen, size_t def_off)
{
    memset(dst, 0xFF, (size_t)sz);
    if (def == 0) {
        return;
    }
    if (def_off + (size_t)sz > deflen) {
        die("default length mismatch");
    }
    memcpy(dst, def + def_off, (size_t)sz);
}

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

static void emit_param_enum(unsigned nitems)
{
    unsigned i;

    oputs("typedef enum {\n");
    for (i = 0u; i < nitems; i++) {
        oprintf("    %s = %uu,\n", s_items[i].name, (unsigned)s_items[i].id);
    }
    oputs("    PARAM_ID_SENTINEL\n");
    oputs("} E_PARAMETER_TYPE;\n\n");
}

static void emit_bytes(const uint8_t *p, unsigned n)
{
    unsigned i;

    for (i = 0u; i < n; i++) {
        if ((i % 16u) == 0u) {
            oputs("\n    ");
        }
        oprintf("0x%02Xu", (unsigned)p[i]);
        if ((i + 1u) < n) {
            oputs(", ");
        }
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
    unsigned blk;
    unsigned off;
    unsigned len;
    unsigned attr;
    unsigned def;
} param_tbl_cols_t;

static param_tbl_cols_t param_api_col_widths(unsigned nitems, const pack_place_t *place)
{
    param_tbl_cols_t c;
    unsigned i;

    memset(&c, 0, sizeof c);
    for (i = 0u; i < nitems; i++) {
        if (str_width(s_items[i].name) > c.name) {
            c.name = str_width(s_items[i].name);
        }
        if (uint_text_width((unsigned)place[i].blk) > c.blk) {
            c.blk = uint_text_width((unsigned)place[i].blk);
        }
        if (uint_text_width((unsigned)place[i].off) > c.off) {
            c.off = uint_text_width((unsigned)place[i].off);
        }
        if (uint_text_width((unsigned)place[i].blk_len) > c.len) {
            c.len = uint_text_width((unsigned)place[i].blk_len);
        }
        if (str_width(s_items[i].attr_sym) > c.attr) {
            c.attr = str_width(s_items[i].attr_sym);
        }
        if (s_items[i].attr_resolved != 0u) {
            char gen[64];

            snprintf(gen, sizeof gen, "g_param_attr_%s", s_items[i].name);
            if (str_width(gen) > c.attr) {
                c.attr = str_width(gen);
            }
        }
        {
            const uint8_t *def;
            size_t deflen;
            char def_sym[64];

            def = lookup_def(s_items[i].name, &deflen);
            if (def != 0) {
                snprintf(def_sym, sizeof def_sym, "g_default_%s", s_items[i].name);
                if (str_width(def_sym) > c.def) {
                    c.def = str_width(def_sym);
                }
            } else if (str_width("NULL") > c.def) {
                c.def = str_width("NULL");
            }
        }
    }
    return c;
}

static void emit_resolved_attr_tables(unsigned nitems)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        if (s_items[i].attr_resolved == 0u) {
            continue;
        }
        oprintf("const uint8_t g_param_attr_%s[] = { %uu, %uu, %uu, %uu };\n\n",
                s_items[i].name,
                (unsigned)s_items[i].resolved_attr[0],
                (unsigned)s_items[i].resolved_attr[1],
                (unsigned)s_items[i].resolved_attr[2],
                (unsigned)s_items[i].resolved_attr[3]);
    }
}

static void emit_param_defaults(unsigned nitems)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        const uint8_t *def;
        size_t deflen;

        def = lookup_def(s_items[i].name, &deflen);
        if (def == 0) {
            continue;
        }
        oprintf("const uint8_t g_default_%s[%uu] = {",
                s_items[i].name, (unsigned)deflen);
        emit_bytes(def, (unsigned)deflen);
        oputs("\n};\n\n");
    }
}

static void emit_param_api_table(unsigned nitems, const pack_place_t *place)
{
    param_tbl_cols_t c;
    unsigned i;
    char num_buf[16];
    char attr_buf[64];
    char def_buf[64];
    const char *attr_ref;
    const char *def_ref;

    c = param_api_col_widths(nitems, place);
    emit_resolved_attr_tables(nitems);
    emit_param_defaults(nitems);
    oputs("const ST_PARAM_TABLE tParamApiTable[] = {\n");
    for (i = 0u; i < nitems; i++) {
        const uint8_t *def;
        size_t deflen;

        if (s_items[i].attr_resolved != 0u) {
            snprintf(attr_buf, sizeof attr_buf, "g_param_attr_%s", s_items[i].name);
            attr_ref = attr_buf;
        } else {
            attr_ref = s_items[i].attr_sym;
        }
        def = lookup_def(s_items[i].name, &deflen);
        if (def != 0) {
            snprintf(def_buf, sizeof def_buf, "g_default_%s", s_items[i].name);
            def_ref = def_buf;
        } else {
            def_ref = "NULL";
        }
        oprintf("    { %-*s, ", (int)c.name, s_items[i].name);
        snprintf(num_buf, sizeof num_buf, "%uu", (unsigned)place[i].blk);
        oprintf("%-*s, ", (int)c.blk, num_buf);
        snprintf(num_buf, sizeof num_buf, "%uu", (unsigned)place[i].off);
        oprintf("%-*s, ", (int)c.off, num_buf);
        snprintf(num_buf, sizeof num_buf, "%uu", (unsigned)place[i].blk_len);
        oprintf("%-*s, %-*s, %-*s",
                (int)c.len, num_buf, (int)c.attr, attr_ref, (int)c.def, def_ref);
        if ((i + 1u) < nitems) {
            oputs(" },\n");
        } else {
            oputs("  }\n");
        }
    }
    oputs("};\n\n");
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

static const char *dtype_name(uint8_t dtype)
{
    switch ((E_PARAM_STORAGE_DATATYPE)dtype) {
    case DATATYPE_INT:
        return "INT";
    case DATATYPE_ARRAY:
        return "ARRAY";
    case DATATYPE_STRUCT:
        return "STRUCT";
    case DATATYPE_LIST:
        return "LIST";
    case DATATYPE_LINKARRAY:
        return "LINKARRAY";
    default:
        return "?";
    }
}

static unsigned pack_param_blocks(pack_block_t blocks[PACK_MAX_BLOCKS],
                                  pack_place_t place[PACK_MAX_ITEMS],
                                  unsigned nitems, uint16_t payload_max)
{
    unsigned nblocks;
    unsigned i;
    uint16_t used;
    uint8_t cur_flags;

    nblocks = 0u;
    used = 0u;
    cur_flags = 0u;
    memset(blocks, 0, sizeof(pack_block_t) * PACK_MAX_BLOCKS);

    for (i = 0u; i < nitems; i++) {
        const uint8_t *def;
        size_t deflen;
        uint16_t sz;

        sz = (uint16_t)s_items[i].total_len;
        def = lookup_def(s_items[i].name, &deflen);
        if (def != 0 && deflen != (size_t)sz) {
            die("%s: default length %u != %u", s_items[i].name,
                (unsigned)deflen, (unsigned)sz);
        }
        if (s_items[i].dtype == (uint8_t)DATATYPE_LINKARRAY) {
            unsigned k;
            unsigned nrec;
            unsigned per_page;
            unsigned npage;
            unsigned page;
            unsigned def_off;

            k = (unsigned)s_items[i].link_k;
            nrec = (unsigned)s_items[i].index_count;
            per_page = (unsigned)s_items[i].link_m;
            npage = (unsigned)s_items[i].link_n;
            if (nblocks > (255u - npage)) {
                die("%s: too many param blocks", s_items[i].name);
            }
            place[i].blk = (uint8_t)nblocks;
            place[i].off = 0u;
            place[i].blk_len = (uint8_t)s_items[i].total_len;
            def_off = 0u;
            for (page = 0u; page < npage; page++) {
                unsigned remain;
                unsigned nthis;
                unsigned psz;
                unsigned fi;

                remain = nrec - (page * per_page);
                nthis = (remain > per_page) ? per_page : remain;
                psz = nthis * k;
                if (nblocks >= PACK_MAX_BLOCKS) {
                    die("too many param blocks");
                }
                nblocks++;
                fi = 0u;
                blocks[nblocks - 1u].flags = s_items[i].flags;
                blocks[nblocks - 1u].payload = (uint16_t)psz;
                blocks[nblocks - 1u].nfields = 1u;
                blocks[nblocks - 1u].item_i[fi] = i;
                blocks[nblocks - 1u].field_len[fi] = (uint16_t)psz;
                blocks[nblocks - 1u].field_def_off[fi] = def_off;
                fill_item_bytes(blocks[nblocks - 1u].rom, (uint16_t)psz,
                                def, deflen, def_off);
                def_off += psz;
            }
            used = payload_max;
            cur_flags = s_items[i].flags;
            continue;
        }
        if (sz > payload_max) {
            die("%s: %u bytes exceeds payload %u", s_items[i].name,
                (unsigned)sz, (unsigned)payload_max);
        }
        if ((nblocks == 0u) || (s_items[i].flags != cur_flags) ||
            ((used + sz) > payload_max)) {
            if (nblocks >= PACK_MAX_BLOCKS) {
                die("too many param blocks");
            }
            nblocks++;
            used = 0u;
            cur_flags = s_items[i].flags;
            blocks[nblocks - 1u].flags = cur_flags;
            blocks[nblocks - 1u].payload = 0u;
            blocks[nblocks - 1u].nfields = 0u;
        }
        {
            unsigned fi = blocks[nblocks - 1u].nfields;

            fill_item_bytes(blocks[nblocks - 1u].rom + used, sz, def, deflen, 0u);
            blocks[nblocks - 1u].item_i[fi] = i;
            blocks[nblocks - 1u].field_len[fi] = sz;
            blocks[nblocks - 1u].field_def_off[fi] = 0u;
            blocks[nblocks - 1u].nfields++;
        }
        place[i].blk = (uint8_t)(nblocks - 1u);
        place[i].off = (uint8_t)used;
        place[i].blk_len = (uint8_t)sz;
        used = (uint16_t)(used + sz);
        blocks[nblocks - 1u].payload = used;
    }

    if (nblocks == 0u) {
        die("no packable param blocks");
    }
    return nblocks;
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

static const char *store_label(uint8_t flags)
{
    if (((flags & FLAG_SRAM) != 0u) && ((flags & FLAG_EEPROM_BAK) != 0u)) {
        return "RAM_EE_BK";
    }
    if ((flags & FLAG_SRAM) != 0u) {
        return "RAM_EE";
    }
    if ((flags & FLAG_EEPROM_BAK) != 0u) {
        return "EE_BK";
    }
    if ((flags & FLAG_EEPROM) != 0u) {
        return "EE";
    }
    return "?";
}

static uint32_t pack_bak_span(const pack_block_t *blocks, unsigned nblocks)
{
    uint32_t span = 0u;
    unsigned i;

    for (i = 0u; i < nblocks; i++) {
        if ((blocks[i].flags & FLAG_EEPROM_BAK) == 0u) {
            continue;
        }
        span += (uint32_t)PARAM_BLOCK_SIZE;
    }
    return span;
}

static uint32_t pack_primary_ee_offs(const pack_block_t *blocks, unsigned nblocks,
                                    uint32_t *blk_ee)
{
    uint32_t ee_off = 0u;
    unsigned i;

    for (i = 0u; i < nblocks; i++) {
        if ((blocks[i].flags & FLAG_EEPROM) != 0u) {
            blk_ee[i] = ee_off;
            ee_off += (uint32_t)PARAM_BLOCK_SIZE;
        } else {
            blk_ee[i] = PARAM_BLOCK_NULL_EE_OFF;
        }
    }
    return ee_off;
}

static int store_kind(uint8_t flags)
{
    if (((flags & FLAG_SRAM) != 0u) && ((flags & FLAG_EEPROM_BAK) != 0u)) {
        return 0;
    }
    if (((flags & FLAG_SRAM) != 0u) && ((flags & FLAG_EEPROM) != 0u)) {
        return 2;
    }
    if ((flags & FLAG_EEPROM_BAK) != 0u) {
        return 1;
    }
    if ((flags & FLAG_EEPROM) != 0u) {
        return 3;
    }
    return -1;
}

static void dump_off_cell(FILE *out, int has, uint32_t off)
{
    char s[32];

    if (!has) {
        fputs(" - |", out);
        return;
    }
    fmt_off(s, sizeof s, off);
    fprintf(out, " %s |", s);
}

static void dump_param_ee_row(FILE *out, const char *name, unsigned ram,
                              int has_pri, uint32_t pri_lo, uint32_t pri_hi,
                              int has_bak, uint32_t bak_lo, uint32_t bak_hi,
                              uint32_t ee_bytes, uint32_t reserve)
{
    fprintf(out, "| %s | %u |", name, ram);
    dump_off_cell(out, has_pri, pri_lo);
    dump_off_cell(out, has_pri, pri_hi);
    dump_off_cell(out, has_bak, bak_lo);
    dump_off_cell(out, has_bak, bak_hi);
    fprintf(out, " %u | %u |\n", (unsigned)ee_bytes, (unsigned)reserve);
}

static void dump_summary(FILE *out, unsigned nblocks, const pack_block_t *blocks)
{
    static const char *names[4] = { "RAM_EE_BK", "EE_BK", "RAM_EE", "EE" };
    unsigned ram[4];
    uint32_t ee_bytes[4];
    uint32_t reserve[4];
    uint32_t pri_lo[4];
    uint32_t pri_hi[4];
    uint32_t bak_lo[4];
    uint32_t bak_hi[4];
    int has_pri[4];
    int has_bak[4];
    uint32_t blk_ee[PACK_MAX_BLOCKS];
    uint32_t primary_raw;
    uint32_t ee_total;
    uint32_t bak_span;
    unsigned ram_total;
    uint32_t ee_bytes_total;
    uint32_t reserve_total;
    int tot_pri;
    int tot_bak;
    uint32_t tot_pri_lo;
    uint32_t tot_pri_hi;
    uint32_t tot_bak_lo;
    uint32_t tot_bak_hi;
    unsigned i;
    int k;

    for (k = 0; k < 4; k++) {
        ram[k] = 0u;
        ee_bytes[k] = 0u;
        reserve[k] = 0u;
        pri_lo[k] = 0u;
        pri_hi[k] = 0u;
        bak_lo[k] = 0u;
        bak_hi[k] = 0u;
        has_pri[k] = 0;
        has_bak[k] = 0;
    }

    primary_raw = pack_primary_ee_offs(blocks, nblocks, blk_ee);
    ee_total = PARAM_EE_TOTAL_ALIGN(primary_raw);
    bak_span = pack_bak_span(blocks, nblocks);

    for (i = 0u; i < nblocks; i++) {
        unsigned compact;
        uint32_t start;
        uint32_t last;

        k = store_kind(blocks[i].flags);
        if (k < 0) {
            continue;
        }
        compact = (unsigned)blocks[i].payload + (unsigned)PARAM_CRC_BYTES_BLOCK;
        if ((blocks[i].flags & FLAG_SRAM) != 0u) {
            ram[k] += compact;
        }
        if (blk_ee[i] == PARAM_BLOCK_NULL_EE_OFF) {
            continue;
        }
        {
            uint32_t pad = (uint32_t)PARAM_BLOCK_SIZE - (uint32_t)compact;

            reserve[k] += pad;
            if ((blocks[i].flags & FLAG_EEPROM_BAK) != 0u) {
                reserve[k] += pad;
            }
        }
        start = blk_ee[i];
        last = start + (uint32_t)PARAM_BLOCK_SIZE - 1u;
        if (!has_pri[k] || (start < pri_lo[k])) {
            pri_lo[k] = start;
        }
        if (!has_pri[k] || (last > pri_hi[k])) {
            pri_hi[k] = last;
        }
        has_pri[k] = 1;
        ee_bytes[k] += (uint32_t)PARAM_BLOCK_SIZE;
        if ((blocks[i].flags & FLAG_EEPROM_BAK) != 0u) {
            start = ee_total + blk_ee[i];
            last = start + (uint32_t)PARAM_BLOCK_SIZE - 1u;
            if (!has_bak[k] || (start < bak_lo[k])) {
                bak_lo[k] = start;
            }
            if (!has_bak[k] || (last > bak_hi[k])) {
                bak_hi[k] = last;
            }
            has_bak[k] = 1;
            ee_bytes[k] += (uint32_t)PARAM_BLOCK_SIZE;
        }
    }

    ram_total = 0u;
    ee_bytes_total = 0u;
    reserve_total = 0u;
    tot_pri = 0;
    tot_bak = 0;
    tot_pri_lo = 0u;
    tot_pri_hi = 0u;
    tot_bak_lo = 0u;
    tot_bak_hi = 0u;
    for (k = 0; k < 4; k++) {
        ram_total += ram[k];
        ee_bytes_total += ee_bytes[k];
        reserve_total += reserve[k];
        if (has_pri[k]) {
            if (!tot_pri || (pri_lo[k] < tot_pri_lo)) {
                tot_pri_lo = pri_lo[k];
            }
            if (!tot_pri || (pri_hi[k] > tot_pri_hi)) {
                tot_pri_hi = pri_hi[k];
            }
            tot_pri = 1;
        }
        if (has_bak[k]) {
            if (!tot_bak || (bak_lo[k] < tot_bak_lo)) {
                tot_bak_lo = bak_lo[k];
            }
            if (!tot_bak || (bak_hi[k] > tot_bak_hi)) {
                tot_bak_hi = bak_hi[k];
            }
            tot_bak = 1;
        }
    }
    if (tot_pri && (primary_raw > 0u)) {
        tot_pri_lo = 0u;
        tot_pri_hi = primary_raw - 1u;
    }
    if (tot_bak && (bak_span > 0u)) {
        tot_bak_lo = ee_total;
        tot_bak_hi = ee_total + bak_span - 1u;
    }

    fprintf(out, "## 参变量分类消耗\n\n");
    fprintf(out, "RAM 为 SRAM 工作区（compact：payload + CRC）；无 SRAM 的类型为 0。\n");
    fprintf(out, "EE 偏移相对 `PARAM_EEPROM_BASE`，结束为末字节（含）。\n");
    fprintf(out, "有 BAK 的类型另计备份槽（`PARAM_EE_TOTAL` + 主槽偏移）；EE占用 含主槽与备份槽。\n");
    fprintf(out, "`reserve` = `blk_size` − `compact`，合计含备份槽内的尾部空洞。\n\n");
    fprintf(out, "| 类型 | RAM | 主槽起始 | 主槽结束 | 备份起始 | 备份结束 | EE占用 | 预留 |\n");
    fprintf(out, "|------|-----|----------|----------|----------|----------|--------|------|\n");
    for (k = 0; k < 4; k++) {
        dump_param_ee_row(out, names[k], ram[k],
                          has_pri[k], pri_lo[k], pri_hi[k],
                          has_bak[k], bak_lo[k], bak_hi[k],
                          ee_bytes[k], reserve[k]);
    }
    dump_param_ee_row(out, "合计", ram_total,
                      tot_pri, tot_pri_lo, tot_pri_hi,
                      tot_bak, tot_bak_lo, tot_bak_hi,
                      ee_bytes_total, reserve_total);
    fprintf(out, "\n");
}

static void dump_layout(FILE *out, unsigned nitems, unsigned nblocks,
                        const pack_block_t *blocks, const pack_place_t *place)
{
    unsigned bi;
    unsigned i;
    uint32_t blk_ee[PACK_MAX_BLOCKS];
    uint32_t primary_raw;
    uint32_t ee_total;
    uint32_t bak_span;
    char off_s[32];
    char bak_s[32];
    size_t def_len;

    primary_raw = pack_primary_ee_offs(blocks, nblocks, blk_ee);
    ee_total = PARAM_EE_TOTAL_ALIGN(primary_raw);
    bak_span = pack_bak_span(blocks, nblocks);

    fprintf(out, "# 参变量布局\n\n");
    fprintf(out, "## 存储类型说明\n\n");
    fprintf(out, "| 类型 | flags | 说明 |\n");
    fprintf(out, "|------|-------|------|\n");
    fprintf(out, "| RAM_EE_BK | SRAM+EE+BAK | SRAM 工作区；EE 备份区 1 + 备份区 2 |\n");
    fprintf(out, "| EE_BK | EE+BAK | 无 SRAM；EE 备份区 1 + 备份区 2 |\n");
    fprintf(out, "| RAM_EE | SRAM+EE | SRAM 工作区；仅 EE 备份区 1 |\n");
    fprintf(out, "| EE | EE | 无 SRAM；仅 EE 备份区 1 |\n\n");
    fprintf(out, "EE 偏移相对 `PARAM_EEPROM_BASE`。\n");
    fprintf(out, "条目 `ee_off` = 块主槽起点 + 块内字段偏移。\n");
    fprintf(out, "双备份：备份槽 = `PARAM_EE_TOTAL` + 主槽偏移（与固件 bak2 一致）。\n\n");

    fprintf(out, "## EE 分区\n\n");
    fprintf(out, "| 项 | bytes |\n");
    fprintf(out, "|----|-------|\n");
    fmt_off(off_s, sizeof off_s, primary_raw);
    fprintf(out, "| primary_raw | %s |\n", off_s);
    fmt_off(off_s, sizeof off_s, ee_total);
    fprintf(out, "| primary_aligned (`PARAM_EE_TOTAL`) | %s |\n", off_s);
    fmt_off(off_s, sizeof off_s, bak_span);
    fprintf(out, "| bak_span | %s |\n", off_s);
    fmt_off(off_s, sizeof off_s, ee_total + bak_span);
    fprintf(out, "| map_end | %s |\n\n", off_s);

    fprintf(out, "## 块\n\n");
    fprintf(out, "主槽编号 0..N-1；备份槽接在主槽之后继续编号。`reserve` = `blk_size` − `compact`。\n\n");
    fprintf(out, "| blk_id | role | of | store | compact | reserve | ee_off | blk_size |\n");
    fprintf(out, "|--------|------|----|-------|---------|---------|--------|----------|\n");
    for (bi = 0u; bi < nblocks; bi++) {
        unsigned compact;
        unsigned pad;

        compact = (unsigned)blocks[bi].payload + (unsigned)PARAM_CRC_BYTES_BLOCK;
        pad = (unsigned)PARAM_BLOCK_SIZE - compact;
        if (blk_ee[bi] == PARAM_BLOCK_NULL_EE_OFF) {
            fprintf(out, "| %u | primary | - | %s | %u | %u | - | %u |\n",
                    bi, store_label(blocks[bi].flags),
                    compact, pad, (unsigned)PARAM_BLOCK_SIZE);
            continue;
        }
        fmt_off(off_s, sizeof off_s, blk_ee[bi]);
        fprintf(out, "| %u | primary | - | %s | %u | %u | %s | %u |\n",
                bi, store_label(blocks[bi].flags),
                compact, pad, off_s, (unsigned)PARAM_BLOCK_SIZE);
    }
    {
        unsigned bak_id = nblocks;

        for (bi = 0u; bi < nblocks; bi++) {
            unsigned compact;
            unsigned pad;

            if ((blocks[bi].flags & FLAG_EEPROM_BAK) == 0u) {
                continue;
            }
            if (blk_ee[bi] == PARAM_BLOCK_NULL_EE_OFF) {
                continue;
            }
            compact = (unsigned)blocks[bi].payload + (unsigned)PARAM_CRC_BYTES_BLOCK;
            pad = (unsigned)PARAM_BLOCK_SIZE - compact;
            fmt_off(bak_s, sizeof bak_s, ee_total + blk_ee[bi]);
            fprintf(out, "| %u | bak | %u | %s | %u | %u | %s | %u |\n",
                    bak_id, bi, store_label(blocks[bi].flags),
                    compact, pad, bak_s, (unsigned)PARAM_BLOCK_SIZE);
            bak_id++;
        }
    }

    fprintf(out, "\n## 条目\n\n");
    fprintf(out, "| name | id | store | type | idx | len | default | ee_bk1 | ee_bk2 |\n");
    fprintf(out, "|------|----|-------|------|-----|-----|---------|--------|--------|\n");
    for (i = 0u; i < nitems; i++) {
        unsigned blk;
        uint32_t ee_item;
        uint8_t flags;
        const char *has_def;

        blk = (unsigned)place[i].blk;
        flags = blocks[blk].flags;
        has_def = (lookup_def(s_items[i].name, &def_len) != 0) ? "✔" : " ";
        fprintf(out, "| %s | %u | %s | %s | %u | %u | %s |",
                s_items[i].name, (unsigned)s_items[i].id,
                store_label(flags), dtype_name(s_items[i].dtype),
                (unsigned)s_items[i].index_count, (unsigned)s_items[i].total_len,
                has_def);
        if (blk_ee[blk] == PARAM_BLOCK_NULL_EE_OFF) {
            fprintf(out, " - | - |\n");
            continue;
        }
        ee_item = blk_ee[blk] + (uint32_t)place[i].off;
        fmt_off(off_s, sizeof off_s, ee_item);
        if ((flags & FLAG_EEPROM_BAK) != 0u) {
            fmt_off(bak_s, sizeof bak_s, ee_total + ee_item);
            fprintf(out, " %s | %s |\n", off_s, bak_s);
        } else {
            fprintf(out, " %s | - |\n", off_s);
        }
    }
}

static void dump_layout_stdout(unsigned nitems, unsigned nblocks,
                               const pack_block_t *blocks, const pack_place_t *place)
{
    dump_summary(stdout, nblocks, blocks);
    dump_layout(stdout, nitems, nblocks, blocks, place);
}

static void dump_layout_md(const char *h_path, unsigned nitems, unsigned nblocks,
                           const pack_block_t *blocks, const pack_place_t *place)
{
    char md_path[512];
    char tmp_path[540];
    char *body;
    size_t body_len;
    FILE *fp;
    int n;

    combined_md_path(h_path, md_path, sizeof md_path);
    n = snprintf(tmp_path, sizeof tmp_path, "%s.param.tmp", md_path);
    if ((n < 0) || ((size_t)n >= sizeof tmp_path)) {
        die("tmp path too long");
    }

    fp = fopen(tmp_path, "wb");
    if (fp == 0) {
        die("cannot write %s", tmp_path);
    }
    dump_summary(fp, nblocks, blocks);
    fclose(fp);
    body = read_section_file(tmp_path, &body_len);
    upsert_md_section(md_path,
                      "<!-- BEGIN:SUMMARY:PARAM -->",
                      "<!-- END:SUMMARY:PARAM -->",
                      body);
    free(body);

    fp = fopen(tmp_path, "wb");
    if (fp == 0) {
        die("cannot write %s", tmp_path);
    }
    dump_layout(fp, nitems, nblocks, blocks, place);
    fclose(fp);
    body = read_section_file(tmp_path, &body_len);
    remove(tmp_path);
    upsert_md_section(md_path,
                      "<!-- BEGIN:PARAM -->",
                      "<!-- END:PARAM -->",
                      body);
    free(body);
    reorder_layout_md(md_path);
}

int main(int argc, char **argv)
{
    const char *path;
    FILE *fp;
    unsigned nitems;
    unsigned i;
    unsigned nblocks;
    pack_block_t blocks[PACK_MAX_BLOCKS];
    pack_place_t place[PACK_MAX_ITEMS];
    uint16_t payload_max;

    load_param_items();
    nitems = s_nitems;
    if (nitems == 0u) {
        die("no param items");
    }
    if (nitems > PACK_MAX_ITEMS) {
        die("too many param items");
    }

    payload_max = (uint16_t)PARAM_BLOCK_PAYLOAD_MAX;
    assign_param_ids(nitems);
    for (i = 0u; i < nitems; i++) {
        resolve_item_attr(&s_items[i], payload_max);
    }
    nblocks = pack_param_blocks(blocks, place, nitems, payload_max);

    if (argc == 2 && strcmp(argv[1], "--dump") == 0) {
        dump_layout_stdout(nitems, nblocks, blocks, place);
        return 0;
    }

    if (argc != 2) {
        die("usage: dc_param_pack <dc_param_layout.h>\n"
            "       dc_param_pack --dump");
    }
    path = argv[1];

    s_out_len = 0u;
    oputs("/* Generated by dc_param_pack. Do not edit. */\n");
    oputs("#ifndef DC_PARAM_LAYOUT_H\n");
    oputs("#define DC_PARAM_LAYOUT_H\n\n");
    oputs("#include <stddef.h>\n");
    oputs("#include <stdint.h>\n");
    oputs("#include \"dc_param.h\"\n");
    oputs("#include \"dc_storage_cfg.h\"\n\n");

    emit_param_enum(nitems);

    oprintf("#define PARAM_LAYOUT_BLOCK_COUNT (%uu)\n", nblocks);
    oprintf("#define PARAM_LAYOUT_ITEM_COUNT (%uu)\n\n", nitems);

    {
        uint32_t ee_off = 0u;
        unsigned last_ee_blk = 0u;
        int has_last_ee = 0;
        uint32_t bak_span;

        for (i = 0u; i < nblocks; i++) {
            if ((blocks[i].flags & FLAG_EEPROM) != 0u) {
                if (ee_off == 0u) {
                    oprintf("#define PARAM_LAYOUT_BLOCK_%u_EE_OFF (0u)\n", i);
                } else {
                    oprintf("#define PARAM_LAYOUT_BLOCK_%u_EE_OFF "
                            "(PARAM_LAYOUT_BLOCK_%u_EE_OFF + "
                            "PARAM_BLOCK_SIZE)\n",
                            i, last_ee_blk);
                }
                last_ee_blk = i;
                has_last_ee = 1;
                ee_off += (uint32_t)PARAM_BLOCK_SIZE;
            } else {
                oprintf("#define PARAM_LAYOUT_BLOCK_%u_EE_OFF (PARAM_BLOCK_NULL_EE_OFF)\n",
                        i);
            }
        }
        if (has_last_ee != 0) {
            oprintf("#define PARAM_EE_TOTAL (PARAM_EE_TOTAL_ALIGN("
                    "PARAM_LAYOUT_BLOCK_%u_EE_OFF + PARAM_BLOCK_SIZE))\n",
                    last_ee_blk);
        } else {
            oputs("#define PARAM_EE_TOTAL (0u)\n");
        }
        bak_span = pack_bak_span(blocks, nblocks);
        oprintf("#define PARAM_EE_BAK_BASE (PARAM_EE_TOTAL)\n");
        oprintf("#define PARAM_EE_BAK_SPAN (%uu)\n", (unsigned)bak_span);
        oprintf("#define PARAM_EE_MAP_END (PARAM_EE_BAK_BASE + PARAM_EE_BAK_SPAN)\n");
        for (i = 0u; i < nblocks; i++) {
            if ((blocks[i].flags & FLAG_EEPROM_BAK) != 0u) {
                oprintf("#define PARAM_LAYOUT_BLOCK_%u_EE_BK_OFF "
                        "(PARAM_EE_BAK_BASE + PARAM_LAYOUT_BLOCK_%u_EE_OFF)\n",
                        i, i);
            }
        }
        oputs("\n");

        oputs("/* tParamBlockTable: primary slots only. bak2 = PARAM_EE_BAK_BASE + uBlockEeOff */\n");
    }

    for (i = 0u; i < nblocks; i++) {
        unsigned f;
        unsigned compact;

        compact = (unsigned)blocks[i].payload + (unsigned)PARAM_CRC_BYTES_BLOCK;
        if (compact > (unsigned)PARAM_BLOCK_SIZE) {
            die("block %u: compact %u exceeds PARAM_BLOCK_SIZE", i, compact);
        }
        oprintf("#define PARAM_LAYOUT_BLOCK_%u_PAYLOAD (%uu)\n", i,
                (unsigned)blocks[i].payload);
        oprintf("#define PARAM_LAYOUT_BLOCK_%u_LEN (%uu)\n\n", i, compact);
        oprintf("typedef struct {\n");
        for (f = 0u; f < blocks[i].nfields; f++) {
            unsigned ii = blocks[i].item_i[f];

            oprintf("    uint8_t %s[%uu];\n", s_items[ii].name,
                    (unsigned)blocks[i].field_len[f]);
        }
        oputs("    uint8_t crc[2u];\n");
        oprintf("} param_layout_%u_t;\n", i);
        oprintf("typedef char param_layout_%u_szchk[(sizeof(param_layout_%u_t) == "
                "(size_t)PARAM_LAYOUT_BLOCK_%u_LEN) ? 1 : -1];\n\n",
                i, i, i);
    }

    oputs("#endif /* DC_PARAM_LAYOUT_H */\n\n");
    oputs("#if defined(DC_PARAM_LAYOUT_DEFINE)\n");
    oputs("#ifndef DC_PARAM_LAYOUT_TABLE_DEFINED\n");
    oputs("#define DC_PARAM_LAYOUT_TABLE_DEFINED\n\n");
    for (i = 0u; i < nblocks; i++) {
        if ((blocks[i].flags & FLAG_SRAM) != 0u) {
            oprintf("param_layout_%u_t g_param_ram_%u;\n", i, i);
        }
    }
    oputs("\n");
    oputs("const uint32_t PARAM_EEPROM_ORIGIN = (uint32_t)PARAM_EEPROM_BASE;\n\n");
    oputs("const ST_PARAM_BLOCK_TABLE tParamBlockTable[] = {\n");
    {
        char prev_lab[24];
        char lab_buf[24];
        int have_prev = 0;

        prev_lab[0] = '\0';

        /* Primary slots first. Dual-backup stores are tagged BK1. */
        for (i = 0u; i < nblocks; i++) {
            const char *base = store_label(blocks[i].flags);
            unsigned has_sram = ((blocks[i].flags & FLAG_SRAM) != 0u) ? 1u : 0u;
            unsigned has_bak = ((blocks[i].flags & FLAG_EEPROM_BAK) != 0u) ? 1u : 0u;

            if (has_bak != 0u) {
                snprintf(lab_buf, sizeof lab_buf, "%s1", base);
            } else {
                snprintf(lab_buf, sizeof lab_buf, "%s", base);
            }
            if ((have_prev == 0) || (strcmp(prev_lab, lab_buf) != 0)) {
                oprintf("    /* %s */\n", lab_buf);
                snprintf(prev_lab, sizeof prev_lab, "%s", lab_buf);
                have_prev = 1;
            }
            if (has_sram != 0u) {
                oprintf("    { PARAM_LAYOUT_BLOCK_%u_EE_OFF, "
                        "(uint8_t *)&g_param_ram_%u, (uint16_t)sizeof(g_param_ram_%u), "
                        "0x%02Xu }",
                        i, i, i, (unsigned)blocks[i].flags);
            } else {
                oprintf("    { PARAM_LAYOUT_BLOCK_%u_EE_OFF, "
                        "NULL, (uint16_t)PARAM_LAYOUT_BLOCK_%u_LEN, "
                        "0x%02Xu }",
                        i, i, (unsigned)blocks[i].flags);
            }
            if ((i + 1u) < nblocks) {
                oputs(",");
            }
            oputs("\n");
        }

        /* Backup slots last, commented out (docs only; not in table).
         * Logical block ids continue after primaries: N, N+1, ... */
        have_prev = 0;
        prev_lab[0] = '\0';
        {
            unsigned bak_id = nblocks;

            for (i = 0u; i < nblocks; i++) {
                const char *base = store_label(blocks[i].flags);
                unsigned has_sram = ((blocks[i].flags & FLAG_SRAM) != 0u) ? 1u : 0u;
                unsigned has_bak = ((blocks[i].flags & FLAG_EEPROM_BAK) != 0u) ? 1u : 0u;

                if (has_bak == 0u) {
                    continue;
                }
                snprintf(lab_buf, sizeof lab_buf, "%s2", base);
                if ((have_prev == 0) || (strcmp(prev_lab, lab_buf) != 0)) {
                    oprintf("    /* %s */\n", lab_buf);
                    snprintf(prev_lab, sizeof prev_lab, "%s", lab_buf);
                    have_prev = 1;
                }
                if (has_sram != 0u) {
                    oprintf("    /* block %u (bak of %u): "
                            "{ PARAM_LAYOUT_BLOCK_%u_EE_BK_OFF, "
                            "(uint8_t *)&g_param_ram_%u, (uint16_t)sizeof(g_param_ram_%u), "
                            "0x%02Xu }, */\n",
                            bak_id, i, i, i, i, (unsigned)blocks[i].flags);
                } else {
                    oprintf("    /* block %u (bak of %u): "
                            "{ PARAM_LAYOUT_BLOCK_%u_EE_BK_OFF, "
                            "NULL, (uint16_t)PARAM_LAYOUT_BLOCK_%u_LEN, "
                            "0x%02Xu }, */\n",
                            bak_id, i, i, i, (unsigned)blocks[i].flags);
                }
                bak_id++;
            }
        }
    }
    oputs("};\n\n");

    oputs("const uint16_t tParamBlockTableCount = (uint16_t)PARAM_LAYOUT_BLOCK_COUNT;\n\n");
    emit_param_api_table(nitems, place);
    oputs("const uint16_t tParamApiTableCount = (uint16_t)PARAM_LAYOUT_ITEM_COUNT;\n\n");
    oputs("#endif /* DC_PARAM_LAYOUT_TABLE_DEFINED */\n");
    oputs("#endif /* DC_PARAM_LAYOUT_DEFINE */\n");

    if (file_same(path, s_out, s_out_len) == 0) {
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
    dump_layout_stdout(nitems, nblocks, blocks, place);
    dump_layout_md(path, nitems, nblocks, blocks, place);
    return 0;
}
