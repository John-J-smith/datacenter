#define DC_PARAM_PACK
#include "dc_param.h"

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
    uint8_t n;
    uint8_t b;
    uint8_t flags;
} pack_item_t;

#define PACK_ROW(tok, dt, n, b, fl) \
    { #tok, 0u, (uint8_t)(dt), (uint8_t)(n), (uint8_t)(b), (uint8_t)(fl) },

static pack_item_t s_items[] = { PARAM_ITEM_LIST(PACK_ROW) };

#undef PACK_ROW

static void assign_param_ids(unsigned nitems)
{
    unsigned i;

    for (i = 0u; i < nitems; i++) {
        s_items[i].id = (uint16_t)i;
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
    uint8_t rom[PARAM_BLOCK_BYTES_MAX];
    unsigned nfields;
    unsigned item_i[PACK_MAX_ITEMS];
    uint16_t field_len[PACK_MAX_ITEMS];
    unsigned field_def_off[PACK_MAX_ITEMS];
} pack_block_t;

typedef struct {
    uint8_t blk;
    uint16_t off;
    uint16_t len;
} pack_place_t;

static char s_out[OUT_CAP];
static size_t s_out_len;

static void die(const char *fmt, ...);

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

        sz = (uint16_t)s_items[i].n * (uint16_t)s_items[i].b;
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

            k = (unsigned)s_items[i].b;
            nrec = (unsigned)s_items[i].n;
            if (k == 0u) {
                die("%s: LINKARRAY element size 0", s_items[i].name);
            }
            per_page = (unsigned)payload_max / k;
            if (per_page == 0u) {
                die("%s: record %u exceeds payload %u", s_items[i].name,
                    k, (unsigned)payload_max);
            }
            npage = (nrec + per_page - 1u) / per_page;
            if (nblocks > (255u - npage)) {
                die("%s: too many param blocks", s_items[i].name);
            }
            place[i].blk = (uint8_t)nblocks;
            place[i].off = 0u;
            place[i].len = sz;
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
        place[i].off = used;
        place[i].len = sz;
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
            ee_off += (uint32_t)PARAM_BLOCK_BYTES_MAX;
        }

        printf("  block %u: flags=0x%02X payload=%u ram=%u ee_slot=%u ee_off=%u fields=%u\n",
               bi,
               (unsigned)blocks[bi].flags,
               (unsigned)blocks[bi].payload,
               compact,
               (unsigned)PARAM_BLOCK_BYTES_MAX,
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
                   bi, (unsigned)ee_off, (unsigned)PARAM_BLOCK_BYTES_MAX);
            ee_off += (uint32_t)PARAM_BLOCK_BYTES_MAX;
        }
    }

    printf("=== param items (%u) ===\n", nitems);
    for (i = 0u; i < nitems; i++) {
        printf("  %-22s id=%u blk=%u off=%u len=%u type=%s n=%u b=%u",
               s_items[i].name,
               (unsigned)s_items[i].id,
               (unsigned)place[i].blk,
               (unsigned)place[i].off,
               (unsigned)place[i].len,
               dtype_name(s_items[i].dtype),
               (unsigned)s_items[i].n,
               (unsigned)s_items[i].b);

        if (s_items[i].dtype == (uint8_t)DATATYPE_LINKARRAY && s_items[i].b != 0u) {
            uint8_t per_page =
                (uint8_t)(PARAM_BLOCK_PAYLOAD_MAX / (uint16_t)s_items[i].b);
            uint8_t pages;

            if (per_page != 0u) {
                pages = (uint8_t)((s_items[i].n + per_page - 1u) / per_page);
                printf("  pages=%u blk%u..%u",
                       (unsigned)pages,
                       (unsigned)place[i].blk,
                       (unsigned)(place[i].blk + pages - 1u));
            }
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

    nitems = (unsigned)(sizeof s_items / sizeof s_items[0]);
    if (nitems == 0u) {
        die("no param items");
    }
    if (nitems > PACK_MAX_ITEMS) {
        die("too many param items");
    }

    payload_max = (uint16_t)PARAM_BLOCK_PAYLOAD_MAX;
    assign_param_ids(nitems);
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

        for (i = 0u; i < nblocks; i++) {
            if ((blocks[i].flags & FLAG_EEPROM) != 0u) {
                oprintf("#define PARAM_LAYOUT_BLOCK_%u_EE_OFF (%uu)\n", i, ee_off);
                ee_off += (uint32_t)PARAM_BLOCK_BYTES_MAX;
            } else {
                oprintf("#define PARAM_LAYOUT_BLOCK_%u_EE_OFF (PARAM_BLOCK_NULL_EE_OFF)\n",
                        i);
            }
        }
        oprintf("#define PARAM_EE_TOTAL (%uu)\n\n", ee_off);
    }

    for (i = 0u; i < nblocks; i++) {
        unsigned f;
        unsigned compact;

        compact = (unsigned)blocks[i].payload + (unsigned)PARAM_CRC_BYTES_BLOCK;
        if (compact > (unsigned)PARAM_BLOCK_BYTES_MAX) {
            die("block %u: compact %u exceeds PARAM_BLOCK_BYTES_MAX", i, compact);
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
    for (i = 0u; i < nblocks; i++) {
        oprintf("const uint8_t g_param_rom_%u[PARAM_LAYOUT_BLOCK_%u_PAYLOAD] = {",
                i, i);
        emit_bytes(blocks[i].rom, (unsigned)blocks[i].payload);
        oputs("\n};\n\n");
    }
    oputs("const uint32_t PARAM_EEPROM_ORIGIN = (uint32_t)PARAM_EEPROM_BASE;\n\n");
    oputs("const ST_PARAM_BLOCK_TABLE tParamBlockTable[] = {\n");
    for (i = 0u; i < nblocks; i++) {
        oprintf("    { %uu, PARAM_LAYOUT_BLOCK_%u_EE_OFF, "
                "(uint8_t *)&g_param_ram_%u, (uint16_t)sizeof(g_param_ram_%u), "
                "0x%02Xu, g_param_rom_%u }",
                i, i, i, i, (unsigned)blocks[i].flags, i);
        if ((i + 1u) < nblocks) {
            oputs(",");
        }
        oputs("\n");
    }
    oputs("};\n\n");
    oputs("const uint16_t tParamBlockTableCount = (uint16_t)PARAM_LAYOUT_BLOCK_COUNT;\n\n");
    oputs("const ST_PARAM_TABLE tParamApiTable[] = {\n");
    for (i = 0u; i < nitems; i++) {
        oprintf("    { %s, %uu, (uint16_t)offsetof(param_layout_%u_t, %s), "
                "%uu, %uu, %uu, %uu }",
                s_items[i].name,
                (unsigned)place[i].blk,
                (unsigned)place[i].blk,
                s_items[i].name,
                (unsigned)place[i].len,
                (unsigned)s_items[i].dtype,
                (unsigned)s_items[i].n,
                (unsigned)s_items[i].b);
        if ((i + 1u) < nitems) {
            oputs(",");
        }
        oputs("\n");
    }
    oputs("};\n\n");
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
