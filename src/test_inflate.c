/* test_inflate.c - regression test: the embedded sprite blob must inflate
 * to the original 300,598-byte BMP (CRC-32 reference computed with zlib). */
#include <stdio.h>
#include <stdlib.h>
#include "gfxdata.h"
#include "inflate.h"

static unsigned long crc32_buf(const BYTE *p, size_t n)
{
    unsigned long crc = 0xffffffffUL;
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1)));
    }
    return crc ^ 0xffffffffUL;
}

int main(void)
{
    BYTE  *out = malloc(0x4A000);
    size_t n = 0;
    unsigned long crc;

    if (!out) return 2;
    if (inflate_raw(g_packedGfx, g_packedGfxSize, out, 0x4A000, &n)) {
        printf("inflate FAILED\n");
        return 1;
    }
    crc = crc32_buf(out, n);
    printf("inflated %lu bytes, crc32=%08lx\n", (unsigned long)n, crc);
    if (n != 300598UL || crc != 0xCD043A57UL) {
        printf("MISMATCH (expected 300598 bytes / crc32 cd043a57)\n");
        return 1;
    }
    printf("OK\n");
    return 0;
}
