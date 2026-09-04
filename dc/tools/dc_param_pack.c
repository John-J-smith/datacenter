#define DC_PARAM_PACK
#include "dc_param.h"
#include "dc_param_attr.h"

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

static void die(const char *fmt, ...);

static void load_param_items(void)
{
    s_nitems = 0u;
    PARAM_ITEM_LIST(PACK_ROW)
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

static void dump_layout(unsigned nitems, unsigned nblocks,
                        const pack_block_t *blocks, const pack_place_t *place)
{
    unsigned bi;
    unsigned i;
    uint32_t ee_off = 0u;
    uint32_t ee_total = 0u;

    printf("=== param layout (host dump) ===\n");
    printf("=== blocks (%u) ===\n", nblocks);
    for (bi = 0u; bi < nblocks; bi++) {
        unsigned f;
        unsigned compact;
        uint32_t ee_this;

        compact = (unsigned)blocks[bi].payload + (unsigned)PARAM_CRC_BYTES_BLOCK;
        ee_this = PARAM_BLOCK_NULL_EE_OFF;
        if ((blocks[bi].flags & FLAG_EEPROM) != 0u) {
            ee_this = ee_off;
            ee_off += (uint32_t)PARAM_BLOCK_SIZE;
        }

        printf("  block %u: flags=0x%02X payload=%u ram=%u ee_slot=%u ee_off=%u fields=%u\n",
               bi,
               (unsigned)blocks[bi].flags,
               (unsigned)blocks[bi].payload,
               compact,
               (unsigned)PARAM_BLOCK_SIZE,
               ee_this,
               blocks[bi].nfields);
        for (f = 0u; f < blocks[bi].nfields; f++) {
            unsigned ii = blocks[bi].item_i[f];

            printf("    %s[%u]\n", s_items[ii].name,
                   (unsigned)blocks[bi].field_len[f]);
        }
    }

    ee_total = ee_off;
    printf("=== EE map (relative to PARAM_EEPROM_BASE) ===\n");
    printf("  total=%u\n", (unsigned)ee_total);
    ee_off = 0u;
    for (bi = 0u; bi < nblocks; bi++) {
        if ((blocks[bi].flags & FLAG_EEPROM) != 0u) {
            printf("  block %u @+%u size=%u\n",
                   bi, (unsigned)ee_off, (unsigned)PARAM_BLOCK_SIZE);
            ee_off += (uint32_t)PARAM_BLOCK_SIZE;
        }
    }

    printf("=== param items (%u) ===\n", nitems);
    for (i = 0u; i < nitems; i++) {
        printf("  %-22s id=%u blk=%u off=%u blk_len=%u type=%s idx=%u",
               s_items[i].name,
               (unsigned)s_items[i].id,
               (unsigned)place[i].blk,
               (unsigned)place[i].off,
               (unsigned)place[i].blk_len,
               dtype_name(s_items[i].dtype),
               (unsigned)s_items[i].index_count);

        if (s_items[i].dtype == (uint8_t)DATATYPE_LINKARRAY) {
            printf("  N=%u M=%u K=%u total=%u blk%u..%u",
                   (unsigned)s_items[i].link_n,
                   (unsigned)s_items[i].link_m,
                   (unsigned)s_items[i].link_k,
                   (unsigned)s_items[i].total_len,
                   (unsigned)place[i].blk,
                   (unsigned)(place[i].blk + s_items[i].link_n - 1u));
        }
        printf("\n");
    }
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
        dump_layout(nitems, nblocks, blocks, place);
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
    oputs("#include \"dc_storage_cfg.h\"\n");
    oputs("#include \"dc_param_cfg.h\"\n\n");

    emit_param_enum(nitems);

    oprintf("#define PARAM_LAYOUT_BLOCK_COUNT (%uu)\n", nblocks);
    oprintf("#define PARAM_LAYOUT_ITEM_COUNT (%uu)\n\n", nitems);

    {
        uint32_t ee_off = 0u;
        unsigned last_ee_blk = 0u;
        int has_last_ee = 0;

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
            oprintf("#define PARAM_EE_TOTAL (PARAM_LAYOUT_BLOCK_%u_EE_OFF + "
                    "PARAM_BLOCK_SIZE)\n\n",
                    last_ee_blk);
        } else {
            oputs("#define PARAM_EE_TOTAL (0u)\n\n");
        }
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
        oprintf("param_layout_%u_t g_param_ram_%u;\n", i, i);
    }
    oputs("\n");
    oputs("const uint32_t PARAM_EEPROM_ORIGIN = (uint32_t)PARAM_EEPROM_BASE;\n\n");
    oputs("const ST_PARAM_BLOCK_TABLE tParamBlockTable[] = {\n");
    for (i = 0u; i < nblocks; i++) {
        oprintf("    { PARAM_LAYOUT_BLOCK_%u_EE_OFF, "
                "(uint8_t *)&g_param_ram_%u, (uint16_t)sizeof(g_param_ram_%u), "
                "0x%02Xu }",
                i, i, i, (unsigned)blocks[i].flags);
        if ((i + 1u) < nblocks) {
            oputs(",");
        }
        oputs("\n");
    }
    oputs("};\n\n");
    oputs("const uint16_t tParamBlockTableCount = (uint16_t)PARAM_LAYOUT_BLOCK_COUNT;\n\n");
    emit_param_api_table(nitems, place);
    oputs("const uint16_t tParamApiTableCount = (uint16_t)PARAM_LAYOUT_ITEM_COUNT;\n\n");
    oputs("#endif /* DC_PARAM_LAYOUT_TABLE_DEFINED */\n");
    oputs("#endif /* DC_PARAM_LAYOUT_DEFINE */\n");

    if (file_same(path, s_out, s_out_len)) {
        dump_layout(nitems, nblocks, blocks, place);
        return 0;
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
    dump_layout(nitems, nblocks, blocks, place);
    return 0;
}
