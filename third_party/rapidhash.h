/*
 * rapidhash V3 - Very fast, high quality, platform-independent hashing algorithm.
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * Copyright (C) 2025 Nicolas De Carli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */

static inline __attribute__((always_inline)) void rapid_mum(uint64_t *A, uint64_t *B)
{
#if defined(__SIZEOF_INT128__) && !defined(__wasi__)
  __uint128_t r = *A;
  r *= *B;
  *A = (uint64_t)r;
  *B = (uint64_t)(r >> 64);
#elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64)) && !defined(__wasi__)
#if defined(_M_X64)
  *A = _umul128(*A, *B, B);
#else
  uint64_t c = __umulh(*A, *B);
  *A = *A * *B;
  *B = c;
#endif
#else
  uint64_t ha = *A >> 32, hb = *B >> 32, la = (uint32_t)*A, lb = (uint32_t)*B;
  uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb, t = rl + (rm0 << 32), c = t < rl;
  uint64_t lo = t + (rm1 << 32);
  c += lo < t;
  uint64_t hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
  *A = lo;
  *B = hi;
#endif
}

static inline __attribute__((always_inline)) uint64_t rapid_mix(uint64_t A, uint64_t B)
{
  rapid_mum(&A, &B);
  return A ^ B;
}

static inline __attribute__((always_inline)) uint64_t rapidhashNano(const uint64_t s1, const uint64_t s2, const uint64_t s3, const uint64_t s4, const uint64_t s5, uint64_t seed)
{
  seed ^= rapid_mix(seed ^ 0x4b33a62ed433d4a3ULL, 0x8bb84b93962eacc9ULL);
  seed = rapid_mix(s1 ^ 0x4b33a62ed433d4a3ULL, s2 ^ seed);
  seed = rapid_mix(s3 ^ 0x4b33a62ed433d4a3ULL, s4 ^ seed);

  uint64_t a = s4 ^ 40;
  uint64_t b = s5;
  
  a ^= 0x8bb84b93962eacc9ULL;
  b ^= seed;
  rapid_mum(&a, &b);
  return rapid_mix(a ^ 0xaaaaaaaaaaaaaaaaULL, b ^ 0x8bb84b93962eacc9ULL ^ 40);
}
