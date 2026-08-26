#ifndef INFLATE_H
#define INFLATE_H

#include <stddef.h>

#ifdef _WIN32
# include <windows.h>
#else
# include <stdint.h>
typedef uint8_t      BYTE;
typedef unsigned int UINT;
#endif

/* Raw DEFLATE (RFC1951, no zlib header) decompressor.
 * Reconstructed from the inflater embedded in WINLINEZ.EXE. */
int inflate_raw(const BYTE *in, size_t inLen, BYTE *out, size_t outSize, size_t *outLen);

#endif
