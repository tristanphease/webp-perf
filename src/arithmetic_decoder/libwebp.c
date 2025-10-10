//! A copy of the decoder as in libwebp
//! Won't bother getting it to compile and getting ffi setup since
//! that's kind of a pain but nice to have it here as a reference
//! all code in this file is owned by Google Inc. not by me. here's 
// the license in libwebp:
/*
Copyright (c) 2010, Google Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * Neither the name of Google nor the names of its contributors may
    be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// bit_reader_utils.h

// The Boolean decoder needs to maintain infinite precision on the 'value'
// field. However, since 'range' is only 8bit, we only need an active window of
// 8 bits for 'value". Left bits (MSB) gets zeroed and shifted away when
// 'value' falls below 128, 'range' is updated, and fresh bits read from the
// bitstream are brought in as LSB. To avoid reading the fresh bits one by one
// (slow), we cache BITS of them ahead. The total of (BITS + 8) bits must fit
// into a natural register (with type bit_t). To fetch BITS bits from bitstream
// we use a type lbit_t.
//
// BITS can be any multiple of 8 from 8 to 56 (inclusive).
// Pick values that fit natural register size.

#if defined(__i386__) || defined(_M_IX86)      // x86 32bit
#define BITS 24
#elif defined(__x86_64__) || defined(_M_X64)   // x86 64bit
#define BITS 56
#elif defined(__arm__) || defined(_M_ARM)      // ARM
#define BITS 24
#elif WEBP_AARCH64                             // ARM 64bit
#define BITS 56
#elif defined(__mips__)                        // MIPS
#define BITS 24
#elif defined(__wasm__)                        // WASM
#define BITS 56
#else                                          // reasonable default
#define BITS 24
#endif

//------------------------------------------------------------------------------
// Derived types and constants:
//   bit_t = natural register type for storing 'value' (which is BITS+8 bits)
//   range_t = register for 'range' (which is 8bits only)

#if (BITS > 24)
typedef uint64_t bit_t;
#else
typedef uint32_t bit_t;
#endif

typedef uint32_t range_t;

//------------------------------------------------------------------------------
// Bitreader

typedef struct VP8BitReader VP8BitReader;
struct VP8BitReader {
  // boolean decoder  (keep the field ordering as is!)
  bit_t value;               // current value
  range_t range;             // current range minus 1. In [127, 254] interval.
  int bits;                  // number of valid bits left
  // read buffer
  const uint8_t* buf;        // next byte to be read
  const uint8_t* buf_end;    // end of read buffer
  const uint8_t* buf_max;    // max packed-read position on buffer
  int eof;                   // true if input is exhausted
};

// Initialize the bit reader and the boolean decoder.
void VP8InitBitReader(VP8BitReader* const br,
                      const uint8_t* const start, size_t size);
// Sets the working read buffer.
void VP8BitReaderSetBuffer(VP8BitReader* const br,
                           const uint8_t* const start, size_t size);

// Update internal pointers to displace the byte buffer by the
// relative offset 'offset'.
void VP8RemapBitReader(VP8BitReader* const br, ptrdiff_t offset);

// return the next value made of 'num_bits' bits
uint32_t VP8GetValue(VP8BitReader* const br, int num_bits, const char label[]);

// return the next value with sign-extension.
int32_t VP8GetSignedValue(VP8BitReader* const br, int num_bits,
                          const char label[]);


static WEBP_INLINE int VP8GetBit(VP8BitReader* WEBP_RESTRICT const br,
                                 int prob, const char label[]) {
  // Don't move this declaration! It makes a big speed difference to store
  // 'range' *before* calling VP8LoadNewBytes(), even if this function doesn't
  // alter br->range value.
  range_t range = br->range;
  if (br->bits < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits;
    const range_t split = (range * prob) >> 8;
    const range_t value = (range_t)(br->value >> pos);
    const int bit = (value > split);
    if (bit) {
      range -= split;
      br->value -= (bit_t)(split + 1) << pos;
    } else {
      range = split + 1;
    }
    {
      const int shift = 7 ^ BitsLog2Floor(range);
      range <<= shift;
      br->bits -= shift;
    }
    br->range = range - 1;
    BT_TRACK(br);
    return bit;
  }
}

// bit_reader_inl_utils.h


// special case for the tail byte-reading
void VP8LoadFinalBytes(VP8BitReader* const br);

//------------------------------------------------------------------------------
// Inlined critical functions

// makes sure br->value has at least BITS bits worth of data
static WEBP_UBSAN_IGNORE_UNDEF WEBP_INLINE
void VP8LoadNewBytes(VP8BitReader* WEBP_RESTRICT const br) {
  assert(br != NULL && br->buf != NULL);
  // Read 'BITS' bits at a time if possible.
  if (br->buf < br->buf_max) {
    // convert memory type to register type (with some zero'ing!)
    bit_t bits;
#if defined(WEBP_USE_MIPS32)
    // This is needed because of un-aligned read.
    lbit_t in_bits;
    lbit_t* p_buf = (lbit_t*)br->buf;
    __asm__ volatile(
      ".set   push                             \n\t"
      ".set   at                               \n\t"
      ".set   macro                            \n\t"
      "ulw    %[in_bits], 0(%[p_buf])          \n\t"
      ".set   pop                              \n\t"
      : [in_bits]"=r"(in_bits)
      : [p_buf]"r"(p_buf)
      : "memory", "at"
    );
#else
    lbit_t in_bits;
    memcpy(&in_bits, br->buf, sizeof(in_bits));
#endif
    br->buf += BITS >> 3;
#if !defined(WORDS_BIGENDIAN)
#if (BITS > 32)
    bits = BSwap64(in_bits);
    bits >>= 64 - BITS;
#elif (BITS >= 24)
    bits = BSwap32(in_bits);
    bits >>= (32 - BITS);
#elif (BITS == 16)
    bits = BSwap16(in_bits);
#else   // BITS == 8
    bits = (bit_t)in_bits;
#endif  // BITS > 32
#else    // WORDS_BIGENDIAN
    bits = (bit_t)in_bits;
    if (BITS != 8 * sizeof(bit_t)) bits >>= (8 * sizeof(bit_t) - BITS);
#endif
    br->value = bits | (br->value << BITS);
    br->bits += BITS;
  } else {
    VP8LoadFinalBytes(br);    // no need to be inlined
  }
}

// Read a bit with proba 'prob'. Speed-critical function!
static WEBP_INLINE int VP8GetBit(VP8BitReader* WEBP_RESTRICT const br,
                                 int prob, const char label[]) {
  // Don't move this declaration! It makes a big speed difference to store
  // 'range' *before* calling VP8LoadNewBytes(), even if this function doesn't
  // alter br->range value.
  range_t range = br->range;
  if (br->bits < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits;
    const range_t split = (range * prob) >> 8;
    const range_t value = (range_t)(br->value >> pos);
    const int bit = (value > split);
    if (bit) {
      range -= split;
      br->value -= (bit_t)(split + 1) << pos;
    } else {
      range = split + 1;
    }
    {
      const int shift = 7 ^ BitsLog2Floor(range);
      range <<= shift;
      br->bits -= shift;
    }
    br->range = range - 1;
    BT_TRACK(br);
    return bit;
  }
}

// simplified version of VP8GetBit() for prob=0x80 (note shift is always 1 here)
static WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW WEBP_INLINE
int VP8GetSigned(VP8BitReader* WEBP_RESTRICT const br, int v,
                 const char label[]) {
  if (br->bits < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits;
    const range_t split = br->range >> 1;
    const range_t value = (range_t)(br->value >> pos);
    const int32_t mask = (int32_t)(split - value) >> 31;  // -1 or 0
    br->bits -= 1;
    br->range += (range_t)mask;
    br->range |= 1;
    br->value -= (bit_t)((split + 1) & (uint32_t)mask) << pos;
    BT_TRACK(br);
    return (v ^ mask) - mask;
  }
}

static WEBP_INLINE int VP8GetBitAlt(VP8BitReader* WEBP_RESTRICT const br,
                                    int prob, const char label[]) {
  // Don't move this declaration! It makes a big speed difference to store
  // 'range' *before* calling VP8LoadNewBytes(), even if this function doesn't
  // alter br->range value.
  range_t range = br->range;
  if (br->bits < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits;
    const range_t split = (range * prob) >> 8;
    const range_t value = (range_t)(br->value >> pos);
    int bit;  // Don't use 'const int bit = (value > split);", it's slower.
    if (value > split) {
      range -= split + 1;
      br->value -= (bit_t)(split + 1) << pos;
      bit = 1;
    } else {
      range = split;
      bit = 0;
    }
    if (range <= (range_t)0x7e) {
      const int shift = kVP8Log2Range[range];
      range = kVP8NewRange[range];
      br->bits -= shift;
    }
    br->range = range;
    BT_TRACK(br);
    return bit;
  }
}


//------------------------------------------------------------------------------
// VP8BitReader

void VP8BitReaderSetBuffer(VP8BitReader* const br,
                           const uint8_t* const start,
                           size_t size) {
  if (start != NULL) {
    br->buf = start;
    br->buf_end = start + size;
    br->buf_max =
        (size >= sizeof(lbit_t)) ? start + size - sizeof(lbit_t) + 1 : start;
  }
}

void VP8InitBitReader(VP8BitReader* const br,
                      const uint8_t* const start, size_t size) {
  assert(br != NULL);
  assert(start != NULL);
  assert(size < (1u << 31));   // limit ensured by format and upstream checks
  br->range   = 255 - 1;
  br->value   = 0;
  br->bits    = -8;   // to load the very first 8bits
  br->eof     = 0;
  VP8BitReaderSetBuffer(br, start, size);
  VP8LoadNewBytes(br);
}

void VP8RemapBitReader(VP8BitReader* const br, ptrdiff_t offset) {
  if (br->buf != NULL) {
    br->buf += offset;
    br->buf_end += offset;
    br->buf_max += offset;
  }
}

const uint8_t kVP8Log2Range[128] = {
     7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0
};

// range = ((range - 1) << kVP8Log2Range[range]) + 1
const uint8_t kVP8NewRange[128] = {
  127, 127, 191, 127, 159, 191, 223, 127,
  143, 159, 175, 191, 207, 223, 239, 127,
  135, 143, 151, 159, 167, 175, 183, 191,
  199, 207, 215, 223, 231, 239, 247, 127,
  131, 135, 139, 143, 147, 151, 155, 159,
  163, 167, 171, 175, 179, 183, 187, 191,
  195, 199, 203, 207, 211, 215, 219, 223,
  227, 231, 235, 239, 243, 247, 251, 127,
  129, 131, 133, 135, 137, 139, 141, 143,
  145, 147, 149, 151, 153, 155, 157, 159,
  161, 163, 165, 167, 169, 171, 173, 175,
  177, 179, 181, 183, 185, 187, 189, 191,
  193, 195, 197, 199, 201, 203, 205, 207,
  209, 211, 213, 215, 217, 219, 221, 223,
  225, 227, 229, 231, 233, 235, 237, 239,
  241, 243, 245, 247, 249, 251, 253, 127
};

void VP8LoadFinalBytes(VP8BitReader* const br) {
  assert(br != NULL && br->buf != NULL);
  // Only read 8bits at a time
  if (br->buf < br->buf_end) {
    br->bits += 8;
    br->value = (bit_t)(*br->buf++) | (br->value << 8);
  } else if (!br->eof) {
    br->value <<= 8;
    br->bits += 8;
    br->eof = 1;
  } else {
    br->bits = 0;  // This is to avoid undefined behaviour with shifts.
  }
}

//------------------------------------------------------------------------------
// Higher-level calls

uint32_t VP8GetValue(VP8BitReader* const br, int bits, const char label[]) {
  uint32_t v = 0;
  while (bits-- > 0) {
    v |= VP8GetBit(br, 0x80, label) << bits;
  }
  return v;
}

int32_t VP8GetSignedValue(VP8BitReader* const br, int bits,
                          const char label[]) {
  const int value = VP8GetValue(br, bits, label);
  return VP8Get(br, label) ? -value : value;
}

