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

#define WEBP_TRANSFORM_AC3_C1 20091
#define WEBP_TRANSFORM_AC3_C2 35468
#define WEBP_TRANSFORM_AC3_MUL1(a) ((((a) * WEBP_TRANSFORM_AC3_C1) >> 16) + (a))
#define WEBP_TRANSFORM_AC3_MUL2(a) (((a) * WEBP_TRANSFORM_AC3_C2) >> 16)

static void TransformOne_C(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {    // vertical pass
    const int a = in[0] + in[8];    // [-4096, 4094]
    const int b = in[0] - in[8];    // [-4095, 4095]
    const int c = WEBP_TRANSFORM_AC3_MUL2(in[4]) -
                  WEBP_TRANSFORM_AC3_MUL1(in[12]);  // [-3783, 3783]
    const int d = WEBP_TRANSFORM_AC3_MUL1(in[4]) +
                  WEBP_TRANSFORM_AC3_MUL2(in[12]);  // [-3785, 3781]
    tmp[0] = a + d;   // [-7881, 7875]
    tmp[1] = b + c;   // [-7878, 7878]
    tmp[2] = b - c;   // [-7878, 7878]
    tmp[3] = a - d;   // [-7877, 7879]
    tmp += 4;
    in++;
  }
  // Each pass is expanding the dynamic range by ~3.85 (upper bound).
  // The exact value is (2. + (20091 + 35468) / 65536).
  // After the second pass, maximum interval is [-3794, 3794], assuming
  // an input in [-2048, 2047] interval. We then need to add a dst value
  // in the [0, 255] range.
  // In the worst case scenario, the input to clip_8b() can be as large as
  // [-60713, 60968].
  tmp = C;
  for (i = 0; i < 4; ++i) {    // horizontal pass
    const int dc = tmp[0] + 4;
    const int a =  dc +  tmp[8];
    const int b =  dc -  tmp[8];
    const int c =
        WEBP_TRANSFORM_AC3_MUL2(tmp[4]) - WEBP_TRANSFORM_AC3_MUL1(tmp[12]);
    const int d =
        WEBP_TRANSFORM_AC3_MUL1(tmp[4]) + WEBP_TRANSFORM_AC3_MUL2(tmp[12]);
    STORE(0, 0, a + d);
    STORE(1, 0, b + c);
    STORE(2, 0, b - c);
    STORE(3, 0, a - d);
    tmp++;
    dst += BPS;
  }
}