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

inline __attribute__((__always_inline__)) uint64_t rapidhashNano(const void *__restrict__ key, uint64_t seed)
{
  __uint128_t r;
  r = (__uint128_t)(seed ^ 0x4b33a62ed433d4a3ull) * (__uint128_t)0x8bb84b93962eacc9ull;
  r = (__uint128_t)(*(uint64_t *)((uint8_t *)key + 0) ^ 0x4b33a62ed433d4a3ull) * (__uint128_t)(*(uint64_t *)((uint8_t *)key + 8) ^ (seed ^ ((uint64_t)r ^ (uint64_t)(r >> 64))));
  r = (__uint128_t)(*(uint64_t *)((uint8_t *)key + 16) ^ 0x4b33a62ed433d4a3ull) * (__uint128_t)(*(uint64_t *)((uint8_t *)key + 24) ^ ((uint64_t)r ^ (uint64_t)(r >> 64)));
  r = (__uint128_t)(*(uint64_t *)((uint8_t *)key + 24) ^ 0x8bb84b93962eacc9ull) * (__uint128_t)(*(uint64_t *)((uint8_t *)key + 32) ^ ((uint64_t)r ^ (uint64_t)(r >> 64)));
  r = (__uint128_t)(((uint64_t)r) ^ 0xaaaaaaaaaaaaaaaaull) * (__uint128_t)((uint64_t)(r >> 64) ^ 0x8bb84b93962eacc9ull ^ 40ULL);
  return (uint64_t)r ^ (uint64_t)(r >> 64);
}
