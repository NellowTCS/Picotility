// pico_png_cart.c
// PNG-format PICO-8 cartridge loader
//
// PICO-8 PNG carts (160x205 RGBA) embed cart data steganographically:
// 2 least-significant bits per RGBA channel (ARGB order) = 1 byte per pixel.
// Data layout: sprites(0x2000) + map(0x1000) + flags(0x100) +
//              music(0x100) + sfx(0x1100) + code(compressed).
// Version byte at offset 0x8000.
//
// Contains: minimal DEFLATE inflate, PNG decoder, LSB extractor,
//           PICO-8 code decompression (old :c:\0 and pxa formats).

#include "pico_png_cart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════
 * Minimal DEFLATE inflate (RFC 1951)
 * Only decompression — no zlib/gzip framing, that's handled by caller.
 * ═══════════════════════════════════════════════════════════════════════ */

typedef struct {
    const uint8_t* src;
    size_t len;
    size_t pos;
    uint32_t bits;
    int nbits;
    int eof;
} InfState;

static uint32_t inf_bits(InfState* s, int n) {
    while (s->nbits < n) {
        if (s->pos < s->len) {
            s->bits |= (uint32_t)s->src[s->pos++] << s->nbits;
            s->nbits += 8;
        } else {
            s->eof = 1;
            break;
        }
    }
    uint32_t v = s->bits & ((1u << n) - 1);
    s->bits >>= n;
    s->nbits -= n;
    return v;
}

/* Canonical Huffman table (counts-based decode) */
typedef struct {
    uint16_t counts[16];
    uint16_t symbols[320]; /* max: 288 lit/len or 32 dist */
} HuffTbl;

static void huff_build(HuffTbl* h, const uint8_t* lens, int n) {
    memset(h->counts, 0, sizeof(h->counts));
    for (int i = 0; i < n; i++)
        if (lens[i]) h->counts[lens[i]]++;

    int offs[16];
    int total = 0;
    for (int i = 1; i <= 15; i++) {
        offs[i] = total;
        total += h->counts[i];
    }
    for (int i = 0; i < n; i++)
        if (lens[i]) h->symbols[offs[lens[i]]++] = (uint16_t)i;
}

static int huff_decode(InfState* s, const HuffTbl* h) {
    int code = 0, first = 0, idx = 0;
    for (int len = 1; len <= 15; len++) {
        code |= (int)inf_bits(s, 1);
        int cnt = h->counts[len];
        if (code - first < cnt)
            return h->symbols[idx + (code - first)];
        idx += cnt;
        first = (first + cnt) << 1;
        code <<= 1;
    }
    return -1;
}

/* DEFLATE length/distance tables */
static const uint16_t len_base[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const uint8_t len_extra[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint16_t dist_base[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,
    8193,12289,16385,24577
};
static const uint8_t dist_extra[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
    7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

/* Code length alphabet order for dynamic blocks */
static const uint8_t cl_order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int inf_block(InfState* s, uint8_t* dst, size_t cap, size_t* pos,
                     const HuffTbl* lit, const HuffTbl* dist) {
    for (;;) {
        int sym = huff_decode(s, lit);
        if (sym < 0) return -1;
        if (sym < 256) {
            if (*pos >= cap) return -1;
            dst[(*pos)++] = (uint8_t)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            int li = sym - 257;
            if (li >= 29) return -1;
            uint16_t length = len_base[li] + (uint16_t)inf_bits(s, len_extra[li]);
            int di = huff_decode(s, dist);
            if (di < 0 || di >= 30) return -1;
            uint32_t distance = dist_base[di] + inf_bits(s, dist_extra[di]);
            if (distance > *pos || *pos + length > cap) return -1;
            size_t src_off = *pos - distance;
            for (uint16_t i = 0; i < length; i++)
                dst[(*pos)++] = dst[src_off + i];
        }
    }
}

/* Main inflate: raw DEFLATE stream (no zlib header) */
static int inflate_raw(const uint8_t* src, size_t src_len,
                       uint8_t* dst, size_t dst_cap, size_t* out_len) {
    InfState s = { src, src_len, 0, 0, 0 };
    size_t dpos = 0;
    int bfinal;

    do {
        bfinal = (int)inf_bits(&s, 1);
        int btype = (int)inf_bits(&s, 2);

        if (btype == 0) {
            /* Stored block — drain bit buffer first */
            int skip = s.nbits & 7;
            if (skip) { s.bits >>= skip; s.nbits -= skip; }
            /* Read buffered bytes back, then from source */
            uint8_t hdr[4];
            for (int i = 0; i < 4; i++) {
                if (s.nbits >= 8) {
                    hdr[i] = (uint8_t)(s.bits & 0xFF);
                    s.bits >>= 8;
                    s.nbits -= 8;
                } else if (s.pos < s.len) {
                    hdr[i] = s.src[s.pos++];
                } else {
                    return -1;
                }
            }
            uint16_t len = hdr[0] | ((uint16_t)hdr[1] << 8);
            /* hdr[2..3] is NLEN — we trust it */
            /* Read len bytes from buffered bits then source */
            for (uint16_t i = 0; i < len; i++) {
                if (dpos >= dst_cap) return -1;
                if (s.nbits >= 8) {
                    dst[dpos++] = (uint8_t)(s.bits & 0xFF);
                    s.bits >>= 8;
                    s.nbits -= 8;
                } else if (s.pos < s.len) {
                    dst[dpos++] = s.src[s.pos++];
                } else {
                    return -1;
                }
            }
        } else if (btype == 1 || btype == 2) {
            HuffTbl lit_tbl, dist_tbl;

            if (btype == 1) {
                /* Fixed Huffman tables */
                uint8_t lens[320];
                int i = 0;
                for (; i <= 143; i++) lens[i] = 8;
                for (; i <= 255; i++) lens[i] = 9;
                for (; i <= 279; i++) lens[i] = 7;
                for (; i <= 287; i++) lens[i] = 8;
                huff_build(&lit_tbl, lens, 288);
                for (i = 0; i < 32; i++) lens[i] = 5;
                huff_build(&dist_tbl, lens, 32);
            } else {
                /* Dynamic Huffman tables */
                int hlit = (int)inf_bits(&s, 5) + 257;
                int hdist = (int)inf_bits(&s, 5) + 1;
                int hclen = (int)inf_bits(&s, 4) + 4;

                uint8_t cl_lens[19];
                memset(cl_lens, 0, sizeof(cl_lens));
                for (int i = 0; i < hclen; i++)
                    cl_lens[cl_order[i]] = (uint8_t)inf_bits(&s, 3);

                HuffTbl cl_tbl;
                huff_build(&cl_tbl, cl_lens, 19);

                uint8_t lens[320];
                memset(lens, 0, sizeof(lens));
                int total = hlit + hdist;
                int i = 0;
                while (i < total) {
                    int sym = huff_decode(&s, &cl_tbl);
                    if (sym < 0) return -1;
                    if (sym < 16) {
                        lens[i++] = (uint8_t)sym;
                    } else if (sym == 16) {
                        int rep = (int)inf_bits(&s, 2) + 3;
                        uint8_t prev = i > 0 ? lens[i - 1] : 0;
                        while (rep-- > 0 && i < total) lens[i++] = prev;
                    } else if (sym == 17) {
                        int rep = (int)inf_bits(&s, 3) + 3;
                        while (rep-- > 0 && i < total) lens[i++] = 0;
                    } else { /* sym == 18 */
                        int rep = (int)inf_bits(&s, 7) + 11;
                        while (rep-- > 0 && i < total) lens[i++] = 0;
                    }
                }

                huff_build(&lit_tbl, lens, hlit);
                huff_build(&dist_tbl, lens + hlit, hdist);
            }

            if (inf_block(&s, dst, dst_cap, &dpos, &lit_tbl, &dist_tbl) < 0)
                return -1;
        } else {
            return -1; /* reserved/invalid block type */
        }
    } while (!bfinal);

    *out_len = dpos;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Minimal PNG decoder (RGBA only, non-interlaced)
 * ═══════════════════════════════════════════════════════════════════════ */

static const uint8_t png_magic[8] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
};

static uint32_t rd_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* Paeth predictor for PNG filter type 4 */
static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c) {
    int p = (int)a + b - c;
    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

/* Decode PNG to raw RGBA pixel buffer.
 * Caller must free(*out_pixels) on success.
 * Returns 0 on success, -1 on failure. */
static int png_decode(const uint8_t* file, size_t flen,
                      uint8_t** out_pixels, int* out_w, int* out_h) {
    if (flen < 8 || memcmp(file, png_magic, 8) != 0) return -1;

    int width = 0, height = 0;
    uint8_t* idat = NULL;
    size_t idat_len = 0, idat_cap = 0;
    size_t pos = 8;

    while (pos + 12 <= flen) {
        uint32_t clen = rd_be32(file + pos);
        uint32_t ctype = rd_be32(file + pos + 4);
        const uint8_t* cdata = file + pos + 8;

        /* Guard against overflow: ensure clen fits in remaining space */
        if (clen > flen - pos - 12) break;

        if (ctype == 0x49484452) { /* IHDR */
            if (clen < 13) goto fail;
            width = (int)rd_be32(cdata);
            height = (int)rd_be32(cdata + 4);
            uint8_t depth = cdata[8];
            uint8_t color = cdata[9];
            uint8_t interlace = cdata[12];
            if (depth != 8 || color != 6 || interlace != 0) goto fail;
        } else if (ctype == 0x49444154) { /* IDAT */
            if (idat_len + clen > idat_cap) {
                size_t need = idat_len + clen;
                idat_cap = need + need / 2 + 256;
                uint8_t* tmp = (uint8_t*)realloc(idat, idat_cap);
                if (!tmp) goto fail;
                idat = tmp;
            }
            memcpy(idat + idat_len, cdata, clen);
            idat_len += clen;
        } else if (ctype == 0x49454E44) { /* IEND */
            break;
        }

        pos += 12 + clen; /* 4(len) + 4(type) + data + 4(crc) */
    }

    if (!idat || width <= 0 || height <= 0) goto fail;

    /* Sanity check dimensions to prevent overflow (max ~16k x 16k) */
    if (width > 16384 || height > 16384) goto fail;

    /* Skip 2-byte zlib header; ignore 4-byte Adler-32 trailer */
    if (idat_len < 6) goto fail;
    size_t raw_len = (size_t)(width * 4 + 1) * height; /* filter byte + RGBA */
    uint8_t* raw = (uint8_t*)malloc(raw_len);
    if (!raw) goto fail;

    size_t inflated = 0;
    if (inflate_raw(idat + 2, idat_len - 6, raw, raw_len, &inflated) != 0 ||
        inflated != raw_len) {
        printf("[png_cart] inflate failed: got %u, expected %u\n",
               (unsigned)inflated, (unsigned)raw_len);
        free(raw);
        goto fail;
    }

    free(idat);
    idat = NULL;

    /* Reverse PNG row filters */
    size_t stride = (size_t)width * 4;
    uint8_t* pixels = (uint8_t*)malloc(stride * height);
    if (!pixels) { free(raw); return -1; }

    for (int y = 0; y < height; y++) {
        uint8_t filt = raw[y * (stride + 1)];
        const uint8_t* src = raw + y * (stride + 1) + 1;
        uint8_t* dst = pixels + y * stride;

        for (size_t x = 0; x < stride; x++) {
            uint8_t a = (x >= 4) ? dst[x - 4] : 0;
            uint8_t b = (y > 0) ? (pixels + (y - 1) * stride)[x] : 0;
            uint8_t c = (x >= 4 && y > 0) ? (pixels + (y - 1) * stride)[x - 4] : 0;

            switch (filt) {
                case 0: dst[x] = src[x]; break;
                case 1: dst[x] = src[x] + a; break;
                case 2: dst[x] = src[x] + b; break;
                case 3: dst[x] = (uint8_t)(src[x] + ((unsigned)a + b) / 2); break;
                case 4: dst[x] = src[x] + paeth(a, b, c); break;
                default: dst[x] = src[x]; break;
            }
        }
    }

    free(raw);
    *out_pixels = pixels;
    *out_w = width;
    *out_h = height;
    return 0;

fail:
    free(idat);
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Extract cart data from RGBA pixels
 *
 * Byte from 2 LSBs per channel, ordered ARGB:
 *   A bits 1-0 → byte bits 7-6
 *   R bits 1-0 → byte bits 5-4
 *   G bits 1-0 → byte bits 3-2
 *   B bits 1-0 → byte bits 1-0
 * ═══════════════════════════════════════════════════════════════════════ */

/* Extract up to out_cap "lo" bytes from pixel LSBs */
static void extract_lsb(const uint8_t* rgba, int w, int h,
                         uint8_t* out, size_t out_cap) {
    size_t idx = 0;
    size_t total = (size_t)w * h;
    for (size_t px = 0; px < total && idx < out_cap; px++) {
        const uint8_t* p = rgba + px * 4;
        out[idx++] = ((p[3] & 3) << 6) | ((p[0] & 3) << 4) |
                     ((p[1] & 3) << 2) | (p[2] & 3);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * PICO-8 code decompression
 *
 * NEW format (\0pxa): bit-level move-to-front + LZ (v0.2.0+)
 * OLD format (:c:\0): byte-level LUT + LZ (pre-v0.2.0)
 * ═══════════════════════════════════════════════════════════════════════ */

/* --- PXA bit reader (LSB-first) --- */
typedef struct {
    const uint8_t* data;
    size_t len;
    size_t byte_pos;
    int bit_pos;
} PxaBits;

static int pxa_bit(PxaBits* r) {
    if (r->byte_pos >= r->len) return 0;
    int b = (r->data[r->byte_pos] >> r->bit_pos) & 1;
    if (++r->bit_pos >= 8) { r->bit_pos = 0; r->byte_pos++; }
    return b;
}

static int pxa_bits(PxaBits* r, int n) {
    int v = 0;
    for (int i = 0; i < n; i++)
        v |= pxa_bit(r) << i;
    return v;
}

/* NEW format: \0pxa header, bit-level move-to-front + LZ */
static int decompress_pxa(const uint8_t* data, size_t data_len,
                          char* out, size_t out_cap) {
    if (data_len < 8) return -1;
    if (out_cap == 0) return -1;
    if (data[0] != 0x00 || data[1] != 'p' || data[2] != 'x' || data[3] != 'a')
        return -1;

    size_t decomp_len = ((size_t)data[4] << 8) | data[5];

    /* Move-to-front table: identity init */
    uint8_t mtf[256];
    for (int i = 0; i < 256; i++) mtf[i] = (uint8_t)i;

    PxaBits br = { data + 8, data_len - 8, 0, 0 };
    size_t opos = 0;

    while (opos < out_cap - 1 && opos < decomp_len) {
        if (pxa_bit(&br)) {
            /* Literal: unary-coded MTF index */
            int unary = 0;
            while (pxa_bit(&br)) unary++;
            int index = pxa_bits(&br, 4 + unary) + (((1 << unary) - 1) << 4);
            if (index >= 256) break;

            uint8_t ch = mtf[index];
            out[opos++] = (char)ch;

            /* Move to front */
            for (int j = index; j > 0; j--) mtf[j] = mtf[j - 1];
            mtf[0] = ch;
        } else {
            /* Back-reference */
            int obits;
            if (pxa_bit(&br)) {
                obits = pxa_bit(&br) ? 5 : 10;
            } else {
                obits = 15;
            }
            int offset = pxa_bits(&br, obits) + 1;

            /* Special: 10-bit offset == 1 → uncompressed block */
            if (obits == 10 && offset == 1) {
                while (opos < out_cap - 1 && opos < decomp_len) {
                    uint8_t ch = (uint8_t)pxa_bits(&br, 8);
                    if (ch == 0x00) break;
                    out[opos++] = (char)ch;
                }
                continue;
            }

            /* Length: start at 3, add 3-bit chunks until chunk != 7 */
            int length = 3;
            for (;;) {
                int chunk = pxa_bits(&br, 3);
                length += chunk;
                if (chunk != 7) break;
            }

            if (offset > (int)opos) break;
            for (int j = 0; j < length && opos < out_cap - 1; j++) {
                out[opos] = out[opos - offset];
                opos++;
            }
        }
    }

    out[opos] = '\0';
    return (int)opos;
}

/* OLD format: :c:\0 header, byte-level LUT + LZ */
static const char old_lut[] =
    "\n 0123456789abcdefghijklmnopqrstuvwxyz!#%(){}[]<>+=/*:;.,~_";

static int decompress_old(const uint8_t* data, size_t data_len,
                          char* out, size_t out_cap) {
    if (data_len < 8) return -1;
    if (out_cap == 0) return -1;
    if (data[0] != ':' || data[1] != 'c' || data[2] != ':' || data[3] != '\0')
        return -1;

    size_t decomp_len = ((size_t)data[4] << 8) | data[5];
    size_t opos = 0;
    size_t i = 8;

    while (i < data_len && opos < out_cap - 1 && opos < decomp_len) {
        uint8_t b = data[i++];

        if (b == 0x00) {
            /* Raw byte escape */
            if (i >= data_len) break;
            out[opos++] = (char)data[i++];
        } else if (b <= 0x3b) {
            /* LUT character */
            out[opos++] = old_lut[b - 1];
        } else {
            /* Back-reference (0x3c-0xff) */
            if (i >= data_len) break;
            uint8_t nb = data[i++];
            int offset = ((b - 0x3c) << 4) | (nb & 0x0f);
            int length = (nb >> 4) + 2;

            if (offset == 0 || offset > (int)opos) break;
            for (int j = 0; j < length && opos < out_cap - 1; j++) {
                out[opos] = out[opos - offset];
                opos++;
            }
        }
    }

    out[opos] = '\0';
    return (int)opos;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API: load PNG-format PICO-8 cartridge
 * ═══════════════════════════════════════════════════════════════════════ */

int pico_png_cart_load_mem(const uint8_t* data, size_t len,
                           pico_ram_t* ram, char* lua_code,
                           size_t lua_code_size, pico_cart_info_t* info) {
    if (!data || !ram || !lua_code || lua_code_size == 0) return -1;

    /* Step 1: Decode PNG → RGBA pixels */
    uint8_t* pixels = NULL;
    int w, h;
    if (png_decode(data, len, &pixels, &w, &h) != 0) {
        printf("[png_cart] PNG decode failed\n");
        return -1;
    }

    /* Step 2: Extract "lo" bytes from pixel LSBs (bits 0-1).
     * We need 0x8001 bytes to include the version byte at 0x8000. */
    size_t cart_data_size = 0x8001;
    uint8_t* cart_data = (uint8_t*)malloc(cart_data_size);
    if (!cart_data) {
        free(pixels);
        return -1;
    }
    memset(cart_data, 0, cart_data_size);
    extract_lsb(pixels, w, h, cart_data, cart_data_size);
    free(pixels);

    /* Step 3: Copy data sections into RAM */
    memcpy(ram->sprites,   cart_data + 0x0000, 0x2000); /* gfx */
    memcpy(ram->map,       cart_data + 0x2000, 0x1000); /* map */
    memcpy(ram->spr_flags, cart_data + 0x3000, 0x0100); /* flags */
    memcpy(ram->songs,     cart_data + 0x3100, 0x0100); /* music */
    memcpy(ram->sfx,       cart_data + 0x3200, 0x1100); /* sfx */

    /* Step 4: Decompress code section (0x4300-0x7FFF) */
    const uint8_t* code = cart_data + 0x4300;
    size_t code_len = 0x8000 - 0x4300;
    int lua_len = -1;

    /* Try new pxa format first (\0pxa header) */
    if (code_len >= 8 && code[0] == 0x00 &&
        code[1] == 'p' && code[2] == 'x' && code[3] == 'a') {
        lua_len = decompress_pxa(code, code_len, lua_code, lua_code_size);
    }
    /* Try old :c:\0 format */
    else if (code_len >= 8 && code[0] == ':' &&
             code[1] == 'c' && code[2] == ':' && code[3] == '\0') {
        lua_len = decompress_old(code, code_len, lua_code, lua_code_size);
    }
    /* Uncompressed: version 0 carts or raw ASCII */
    else if (code_len > 0 && code[0] >= 0x20 && code[0] < 0x7F) {
        size_t slen = 0;
        while (slen < code_len && code[slen] != 0x00) slen++;
        if (slen < lua_code_size) {
            memcpy(lua_code, code, slen);
            lua_code[slen] = '\0';
            lua_len = (int)slen;
        }
    } else {
        printf("[png_cart] Unknown code format, first byte: 0x%02x\n", code[0]);
    }

    free(cart_data);

    if (lua_len <= 0) {
        printf("[png_cart] Decompression failed\n");
    }

    if (info) {
        info->valid = (lua_len >= 0);
    }

    return lua_len;
}
