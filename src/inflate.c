/* inflate.c - minimal raw-DEFLATE (RFC1951) decompressor.
 *
 * Reconstruction of the hand-written inflater found inside WINLINEZ.EXE
 * (functions at 0x412B5D..0x412EF3).  The original is a classic
 * "puff"-style implementation: LSB-first bit reader, fixed/dynamic/stored
 * blocks, Huffman decoding through count/symbol tables.
 *
 * The original reads its input through a bit-buffer (DX register) fed
 * byte-by-byte from DS:ESI; here the same logic is expressed with a
 * plain struct so the code is portable.
 */
#include <windows.h>
#include "inflate.h"

typedef struct {
    const BYTE *in;
    size_t      inLen;
    size_t      inPos;
    UINT        bitBuf;      /* DX in original */
    int         bitCnt;      /* CH in original */
    int         eof;         /* 0x430F6E in original */
} InflateState;

static InflateState g;

/* --- bit reader (0x412C57 / 0x412C66) ------------------------------- */

static int GetBit(void)
{
    if (g.bitCnt == 0) {
        if (g.inPos >= g.inLen) { g.eof = 1; return 0; }
        g.bitBuf = g.in[g.inPos++];
        g.bitCnt = 8;
    }
    {
        int b = g.bitBuf & 1;
        g.bitBuf >>= 1;
        g.bitCnt--;
        return b;
    }
}

static UINT GetBits(int n)              /* 0x412C1A / 0x412C66 */
{
    UINT v = 0;
    int  i;
    for (i = 0; i < n; i++)
        v |= (UINT)GetBit() << i;
    return v;
}

/* --- Huffman decoding (0x412EF3 builds, 0x412C7E/0x412CEB decode) ---- */

#define MAXBITS   15
#define MAXLCODES 286
#define MAXDCODES 30
#define FIXLCODES 288

typedef struct {
    short count[MAXBITS + 1];           /* 0x430F28 */
    short symbol[FIXLCODES];            /* 0x430F66 */
} HuffTable;

/* Build (0x412EF3): canonical Huffman; returns 0 ok, 1 error */
static int BuildTable(HuffTable *h, const BYTE *lengths, int n)
{
    int symbol, len, left;
    short offs[MAXBITS + 1];

    for (len = 0; len <= MAXBITS; len++)
        h->count[len] = 0;
    for (symbol = 0; symbol < n; symbol++)
        h->count[lengths[symbol]]++;

    if (h->count[0] == n)               /* no codes at all */
        return 0;

    left = 1;
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return 1;         /* over-subscribed */
    }

    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + h->count[len];

    for (symbol = 0; symbol < n; symbol++)
        if (lengths[symbol] != 0)
            h->symbol[offs[lengths[symbol]]++] = (short)symbol;

    /* incomplete sets are legal in DEFLATE (e.g. the fixed distance
     * code uses only 30 of 32 codes) - only over-subscription fails.
     * Decoding an unused code makes DecodeSymbol return -1. */
    return left < 0 ? 1 : 0;
}

/* Decode one symbol (0x412C7E + 0x412CEB) */
static int DecodeSymbol(const HuffTable *h)
{
    int len, code = 0, first = 0, index = 0;

    for (len = 1; len <= MAXBITS; len++) {
        code |= GetBit();
        {
            int count = h->count[len];
            if (code - count < first)
                return h->symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code  <<= 1;
        }
    }
    return -1;
}

/* --- block decoding -------------------------------------------------- */

static const BYTE kLenOrder[19] =       /* 0x4200EC in original .data */
    {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

static const short kLenBase[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
                                    35,43,51,59,67,83,99,115,131,163,195,227,258};
static const short kLenExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
                                    3,3,3,3,4,4,4,4,5,5,5,5,0};
static const short kDistBase[30]  = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
                                     257,385,513,769,1025,1537,2049,3073,4097,6145,
                                     8193,12289,16385,24577};
static const short kDistExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
                                     7,7,8,8,9,9,10,10,11,11,12,12,13,13};

/* Fixed literal/length code lengths (0x412D5D) */
static void InitFixedTables(BYTE *lit, BYTE *dist)
{
    int i;
    for (i = 0;   i < 144; i++) lit[i] = 8;
    for (;       i < 256; i++) lit[i] = 9;
    for (;       i < 280; i++) lit[i] = 7;
    for (;       i < 288; i++) lit[i] = 8;
    for (i = 0;  i < 30;  i++) dist[i] = 5;
}

static int Codes(HuffTable *lc, HuffTable *dc, BYTE *win, size_t *outPos, size_t winSize)
{
    for (;;) {
        int sym = DecodeSymbol(lc);
        if (sym < 0) return 1;
        if (sym < 256) {
            if (*outPos >= winSize) return 1;
            win[(*outPos)++] = (BYTE)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            int len, dist, bits;
            sym -= 257;
            if (sym >= 29) return 1;
            bits = kLenExtra[sym];
            len  = kLenBase[sym] + (int)GetBits(bits);
            sym  = DecodeSymbol(dc);
            if (sym < 0 || sym >= 30) return 1;
            bits = kDistExtra[sym];
            dist = kDistBase[sym] + (int)GetBits(bits);
            while (len--) {
                if (*outPos >= winSize) return 1;
                win[*outPos] = win[*outPos - dist];
                (*outPos)++;
            }
        }
        if (g.eof) return 1;
    }
}

static int StoredBlock(BYTE *win, size_t *outPos, size_t winSize)
{
    /* original reads a 16-bit count word whose complement must follow
     * (0x412B7C / 0x412D9F "copy raw" branch) */
    UINT len, nlen, i;
    g.bitCnt = 0;                        /* discard partial bits */
    len  = GetBits(16);
    nlen = GetBits(16);
    if ((len ^ 0xFFFF) != nlen) return 1;
    for (i = 0; i < len; i++) {
        if (*outPos >= winSize || g.eof) return 1;
        win[(*outPos)++] = (BYTE)GetBits(8);
    }
    return 0;
}

static int DynamicTables(HuffTable *lc, HuffTable *dc)
{
    BYTE lengths[288 + 32];
    int  nlen, ndist, ncode, index;
    HuffTable cl;

    nlen  = (int)GetBits(5) + 257;       /* HLIT  (original: +0x101) */
    ndist = (int)GetBits(5) + 1;         /* HDIST */
    ncode = (int)GetBits(4) + 4;         /* HCLEN */

    for (index = 0; index < ncode; index++)
        lengths[kLenOrder[index]] = (BYTE)GetBits(3);
    for (; index < 19; index++)
        lengths[kLenOrder[index]] = 0;

    if (BuildTable(&cl, lengths, 19)) return 1;

    index = 0;
    while (index < nlen + ndist) {
        int sym = DecodeSymbol(&cl), len;
        if (sym < 0) return 1;
        if (sym < 16) {
            lengths[index++] = (BYTE)sym;
            continue;
        }
        len = 0;
        if (sym == 16) {
            if (index == 0) return 1;
            len  = lengths[index - 1];
            sym  = 3 + (int)GetBits(2);
        } else if (sym == 17) {
            sym = 3 + (int)GetBits(3);
        } else {
            sym = 11 + (int)GetBits(7);
        }
        while (sym-- && index < nlen + ndist)
            lengths[index++] = (BYTE)len;
    }

    if (BuildTable(lc, lengths, nlen) ||
        (lengths[256] == 0)            ||
        BuildTable(dc, lengths + nlen, ndist))
        return 1;
    return 0;
}

int inflate_raw(const BYTE *in, size_t inLen, BYTE *out, size_t outSize, size_t *outLen)
{
    HuffTable lenc, distc;
    BYTE      lengths[288 + 32];
    size_t    pos = 0;
    int       last;

    g.in = in; g.inLen = inLen; g.inPos = 0;
    g.bitBuf = 0; g.bitCnt = 0; g.eof = 0;

    do {
        last = GetBit();                                  /* BFINAL */
        {
            int type = (int)GetBits(2);                   /* BTYPE  */
            switch (type) {
            case 0: if (StoredBlock(out, &pos, outSize)) return 1; break;
            case 1:
                InitFixedTables(lengths, lengths + 288);
                if (BuildTable(&lenc, lengths, FIXLCODES) ||
                    BuildTable(&distc, lengths + 288, MAXDCODES)) return 1;
                if (Codes(&lenc, &distc, out, &pos, outSize)) return 1;
                break;
            case 2:
                if (DynamicTables(&lenc, &distc)) return 1;
                if (Codes(&lenc, &distc, out, &pos, outSize)) return 1;
                break;
            default: return 1;
            }
        }
    } while (!last && !g.eof);

    if (outLen) *outLen = pos;
    return 0;
}
