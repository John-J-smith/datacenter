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

#define PACK_ROW(tok, id, dt, n, b, fl) \
    { #tok, (uint16_t)(id), (uint8_t)(dt), (uint8_t)(n), (uint8_t)(b), (uint8_t)(fl) },

static const pack_item_t s_items[] = { PARAM_ITEM_LIST(PACK_ROW) };

#undef PACK_ROW

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

    printf("=== param layout (host dump) ===\n");
    printf("=== blocks (%u) ===\n", nblocks);
    for (bi = 0u; bi < nblocks; bi++) {
        unsigned f;

        printf("  block %u: flags=0x%02X payload=%u len=%u fields=%u\n",
               bi,
               (unsigned)blocks[bi].flags,
               (unsigned)blocks[bi].payload,
               (unsigned)PARAM_BLOCK_BYTES_MAX,
               blocks[bi].nfields);
        for (f = 0u; f < blocks[bi].nfields; f++) {
            unsigned ii = blocks[bi].item_i[f];

            printf("    %s[%u]\n", s_items[ii].name,
                   (unsigned)blocks[bi].field_len[f]);
        }
    }

    printf("=== param items (%u) ===\n", nitems);
    for (i = 0u; i < nitems; i++) {
        printf("  %-22s id=0x%04X blk=%u off=%u len=%u type=%s n=%u b=%u",
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
    oputs("#include <stddef.h>\n\n");
    oprintf("#define PARAM_LAYOUT_BLOCK_COUNT (%uu)\n", nblocks);
    oprintf("#define PARAM_LAYOUT_ITEM_COUNT (%uu)\n\n", nitems);

    for (i = 0u; i < nblocks; i++) {
        unsigned f;
        unsigned pad;

        pad = (unsigned)PARAM_BLOCK_BYTES_MAX -
              (unsigned)blocks[i].payload -
              (unsigned)PARAM_CRC_BYTES_BLOCK;
        oprintf("typedef struct {\n");
        for (f = 0u; f < blocks[i].nfields; f++) {
            unsigned ii = blocks[i].item_i[f];

            oprintf("    uint8_t %s[%uu];\n", s_items[ii].name,
                    (unsigned)blocks[i].field_len[f]);
        }
        oputs("    uint8_t crc[PARAM_CRC_BYTES_BLOCK];\n");
        if (pad != 0u) {
            oprintf("    uint8_t hold[%uu];\n", pad);
        }
        oprintf("} param_layout_%u_t;\n", i);
        oprintf("typedef char param_layout_%u_szchk[(sizeof(param_layout_%u_t) == "
                "(size_t)PARAM_BLOCK_BYTES_MAX) ? 1 : -1];\n\n",
                i, i);
    }

    oputs("#if defined(DC_PARAM_LAYOUT_DEFINE)\n\n");
    for (i = 0u; i < nblocks; i++) {
        oprintf("param_layout_%u_t g_param_ram_%u;\n", i, i);
    }
    oputs("\n");
    for (i = 0u; i < nblocks; i++) {
        unsigned f;

        oprintf("const param_layout_%u_t g_param_rom_%u = {\n", i, i);
        for (f = 0u; f < blocks[i].nfields; f++) {
            unsigned ii = blocks[i].item_i[f];

            oprintf("    .%s = {", s_items[ii].name);
            emit_bytes(blocks[i].rom + place[ii].off,
                       (unsigned)blocks[i].field_len[f]);
            oputs("\n    },\n");
        }
        oputs("};\n\n");
    }
    oputs("const STR_PARAM_BLOCK_TABLE tParamBlockTable[] = {\n");
    for (i = 0u; i < nblocks; i++) {
        oprintf("    { %uu, (uint8_t *)&g_param_ram_%u, (uint16_t)sizeof(g_param_ram_%u), "
                "0x%02Xu, (const uint8_t *)&g_param_rom_%u }",
                i, i, i, (unsigned)blocks[i].flags, i);
        if ((i + 1u) < nblocks) {
            oputs(",");
        }
        oputs("\n");
    }
    oputs("};\n\n");
    oputs("const uint16_t tParamBlockTableCount = (uint16_t)PARAM_LAYOUT_BLOCK_COUNT;\n\n");
    oputs("const STR_PARAMETER_TABLE tParamApiTable[] = {\n");
    for (i = 0u; i < nitems; i++) {
        oprintf("    { 0x%04Xu, %uu, (uint16_t)offsetof(param_layout_%u_t, %s), "
                "%uu, %uu, %uu, %uu }",
                (unsigned)s_items[i].id,
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
    oputs("#endif /* DC_PARAM_LAYOUT_DEFINE */\n\n");
    oputs("#endif\n");

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
