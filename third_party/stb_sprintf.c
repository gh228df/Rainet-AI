// stb_sprintf - v1.10 - public domain snprintf() implementation
// originally by Jeff Roberts / RAD Game Tools, 2015/10/20
// http://github.com/nothings/stb
//
// allowed types:  sc uidBboXx p AaGgEef n
// lengths      :  hh h ll j z t I64 I32 I
//
// Contributors:
//    Fabian "ryg" Giesen (reformatting)
//    github:aganm (attribute format)
//
// Contributors (bugfixes):
//    github:d26435
//    github:trex78
//    github:account-login
//    Jari Komppa (SI suffixes)
//    Rohit Nirmal
//    Marcin Wojdyr
//    Leonard Ritter
//    Stefano Zanotti
//    Adam Allison
//    Arvid Gerstmann
//    Markus Kolb
//
// LICENSE:
//
//   See end of file for license information.

static struct
{
   short temp; // force next field to be 2-byte aligned
   char pair[201];
} stbsp__digitpair =
    {
        0,
        "00010203040506070809101112131415161718192021222324"
        "25262728293031323334353637383940414243444546474849"
        "50515253545556575859606162636465666768697071727374"
        "75767778798081828384858687888990919293949596979899"};

#define STBSP__LEFTJUST 1
#define STBSP__LEADINGPLUS 2
#define STBSP__LEADINGSPACE 4
#define STBSP__LEADING_0X 8
#define STBSP__LEADINGZERO 16
#define STBSP__INTMAX 32
#define STBSP__NEGATIVE 128
#define STBSP__HALFWIDTH 512

static inline __attribute__((always_inline)) void stbsp__lead_sign(uint32_t fl, char *sign)
{
   sign[0] = 0;
   if (fl & STBSP__NEGATIVE)
   {
      sign[0] = 1;
      sign[1] = '-';
   }
   else if (fl & STBSP__LEADINGSPACE)
   {
      sign[0] = 1;
      sign[1] = ' ';
   }
   else if (fl & STBSP__LEADINGPLUS)
   {
      sign[0] = 1;
      sign[1] = '+';
   }
}

static inline __attribute__((always_inline)) uint32_t stbsp__strlen_limited(char const *s, uint32_t limit)
{
   char const *sn = s;

   // get up to 4-byte alignment
   for (;;)
   {
      if (((uintptr_t)sn & 3) == 0)
         break;

      if (!limit || *sn == 0)
         return (uint32_t)(sn - s);

      ++sn;
      --limit;
   }

   // scan over 4 bytes at a time to find terminating 0
   // this will intentionally scan up to 3 bytes past the end of buffers,
   // but because it works 4B aligned, it will never cross page boundaries
   // (hence the STBSP__ASAN markup; the over-read here is intentional
   // and harmless)
   while (limit >= 4)
   {
      uint32_t v = *(uint32_t *)sn;
      // bit hack to find if there's a 0 byte in there
      if ((v - 0x01010101) & (~v) & 0x80808080UL)
         break;

      sn += 4;
      limit -= 4;
   }

   // handle the last few characters to find actual size
   while (limit && *sn)
   {
      ++sn;
      --limit;
   }

   return (uint32_t)(sn - s);
}

STATIC_BSS char buf[STB_SPRINTF_MIN];

static void _printf(const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);

   static char hex[] = "0123456789abcdefxp";
   static char hexu[] = "0123456789ABCDEFXP";
   char *bf;
   char const *f;
   int tlen = 0;

   bf = buf;
   f = fmt;
   for (;;)
   {
      int32_t fw, pr, tz;
      uint32_t fl;

// macros for the callback buffer stuff
#define stbsp__chk_cb_bufL(bytes)             \
   {                                          \
      int len = (int)(bf - buf);              \
      if ((len + (bytes)) >= STB_SPRINTF_MIN) \
      {                                       \
         tlen += len;                         \
         sys_write(1, buf, len);              \
         bf = buf;                            \
      }                                       \
   }
#define stbsp__chk_cb_buf(bytes) stbsp__chk_cb_bufL(bytes)
#define stbsp__flush_cb() stbsp__chk_cb_bufL(STB_SPRINTF_MIN - 1)
#define stbsp__cb_buf_clamp(cl, v)                \
   cl = v;                                        \
   {                                              \
      int lg = STB_SPRINTF_MIN - (int)(bf - buf); \
      if (cl > lg)                                \
         cl = lg;                                 \
   }

      // fast copy everything up to the next % (or end of string)
      for (;;)
      {
         while (((uintptr_t)f) & 3)
         {
         schk1:
            if (f[0] == '%')
               goto scandd;
         schk2:
            if (f[0] == 0)
               goto endfmt;
            stbsp__chk_cb_buf(1);
            *bf++ = f[0];
            ++f;
         }
         for (;;)
         {
            // Check if the next 4 bytes contain %(0x25) or end of string.
            // Using the 'hasless' trick:
            // https://graphics.stanford.edu/~seander/bithacks.html#HasLessInWord
            uint32_t v, c;
            v = *(uint32_t *)f;
            c = (~v) & 0x80808080;
            if (((v ^ 0x25252525) - 0x01010101) & c)
               goto schk1;
            if ((v - 0x01010101) & c)
               goto schk2;
            if ((STB_SPRINTF_MIN - (int)(bf - buf)) < 4)
               goto schk1;

            bf[0] = f[0];
            bf[1] = f[1];
            bf[2] = f[2];
            bf[3] = f[3];

            bf += 4;
            f += 4;
         }
      }
   scandd:

      ++f;

      // ok, we have a percent, read the modifiers first
      fw = 0;
      pr = -1;
      fl = 0;
      tz = 0;

      // flags
      for (;;)
      {
         switch (f[0])
         {
         // if we have left justify
         case '-':
            fl |= STBSP__LEFTJUST;
            ++f;
            continue;
         // if we have leading plus
         case '+':
            fl |= STBSP__LEADINGPLUS;
            ++f;
            continue;
         // if we have leading space
         case ' ':
            fl |= STBSP__LEADINGSPACE;
            ++f;
            continue;
         // if we have leading 0x
         case '#':
            fl |= STBSP__LEADING_0X;
            ++f;
            continue;
         // if we have leading zero
         case '0':
            fl |= STBSP__LEADINGZERO;
            ++f;
            goto flags_done;
         default:
            goto flags_done;
         }
      }
   flags_done:

      // get the field width
      if (f[0] == '*')
      {
         fw = va_arg(va, uint32_t);
         ++f;
      }
      else
      {
         while ((f[0] >= '0') && (f[0] <= '9'))
         {
            fw = fw * 10 + f[0] - '0';
            f++;
         }
      }
      // get the precision
      if (f[0] == '.')
      {
         ++f;
         if (f[0] == '*')
         {
            pr = va_arg(va, uint32_t);
            ++f;
         }
         else
         {
            pr = 0;
            while ((f[0] >= '0') && (f[0] <= '9'))
            {
               pr = pr * 10 + f[0] - '0';
               f++;
            }
         }
      }

      // handle integer size overrides
      switch (f[0])
      {
      // are we halfwidth?
      case 'h':
         fl |= STBSP__HALFWIDTH;
         ++f;
         if (f[0] == 'h')
            ++f; // QUARTERWIDTH
         break;
      // are we 64-bit (unix style)
      case 'l':
         fl |= ((sizeof(long) == 8) ? STBSP__INTMAX : 0);
         ++f;
         if (f[0] == 'l')
         {
            fl |= STBSP__INTMAX;
            ++f;
         }
         break;
      // are we 64-bit on intmax? (c99)
      case 'j':
         fl |= (sizeof(size_t) == 8) ? STBSP__INTMAX : 0;
         ++f;
         break;
      // are we 64-bit on size_t or ptrdiff_t? (c99)
      case 'z':
         fl |= (sizeof(ptrdiff_t) == 8) ? STBSP__INTMAX : 0;
         ++f;
         break;
      case 't':
         fl |= (sizeof(ptrdiff_t) == 8) ? STBSP__INTMAX : 0;
         ++f;
         break;
      // are we 64-bit (msft style)
      case 'I':
         if ((f[1] == '6') && (f[2] == '4'))
         {
            fl |= STBSP__INTMAX;
            f += 3;
         }
         else if ((f[1] == '3') && (f[2] == '2'))
         {
            f += 3;
         }
         else
         {
            fl |= ((sizeof(void *) == 8) ? STBSP__INTMAX : 0);
            ++f;
         }
         break;
      default:
         break;
      }

      // handle each replacement
      switch (f[0])
      {
#define STBSP__NUMSZ 512 // big enough for e308 (with commas) or e-307
         STATIC_BSS char num[STBSP__NUMSZ];
         char lead[8];
         char tail[8];
         char *s;
         char const *h;
         uint32_t l, n, cs;
         uint64_t n64;
         char const *sn;

      case 's':
         // get the string
         s = va_arg(va, char *);
         if (s == 0)
            s = (char *)"null";
         // get the length, limited to desired precision
         // always limit to ~0u chars since our counts are 32b
         l = stbsp__strlen_limited(s, (pr >= 0) ? pr : ~0u);
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         cs = 0;
         // copy the string in
         goto scopy;

      case 'c': // char
         // get the character
         s = num + STBSP__NUMSZ - 1;
         *s = (char)va_arg(va, int);
         l = 1;
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         cs = 0;
         goto scopy;

      case 'n': // weird write-bytes specifier
      {
         int *d = va_arg(va, int *);
         *d = tlen + (int)(bf - buf);
      }
      break;

      case 'A':              // float
      case 'a':              // hex float
      case 'G':              // float
      case 'g':              // float
      case 'E':              // float
      case 'e':              // float
      case 'f':              // float
         va_arg(va, double); // eat it
         s = (char *)"No float";
         l = 8;
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         cs = 0;
         goto scopy;

      case 'o': // octal
         h = hexu;
         lead[0] = 0;
         if (fl & STBSP__LEADING_0X)
         {
            lead[0] = 1;
            lead[1] = '0';
         }
         l = (3 << 4) | (3 << 8);
         goto radixnum;

      case 'p': // pointer
         fl |= (sizeof(void *) == 8) ? STBSP__INTMAX : 0;
         pr = sizeof(void *) * 2;
         fl &= ~STBSP__LEADINGZERO; // 'p' only prints the pointer with zeros
                                    // fall through - to X

      case 'X': // upper hex
      case 'x': // lower hex
         h = (f[0] == 'X') ? hexu : hex;
         l = (4 << 4) | (4 << 8);
         lead[0] = 0;
         if (fl & STBSP__LEADING_0X)
         {
            lead[0] = 2;
            lead[1] = '0';
            lead[2] = h[16];
         }
      radixnum:
         // get the number
         if (fl & STBSP__INTMAX)
            n64 = va_arg(va, uint64_t);
         else
            n64 = va_arg(va, uint32_t);

         s = num + STBSP__NUMSZ;
         // clear tail, and clear leading if value is zero
         tail[0] = 0;
         if (n64 == 0)
         {
            lead[0] = 0;
            if (pr == 0)
            {
               l = 0;
               cs = 0;
               goto scopy;
            }
         }
         // convert to string
         for (;;)
         {
            *--s = h[n64 & ((1 << (l >> 8)) - 1)];
            n64 >>= (l >> 8);
            if (!((n64) || ((int32_t)((num + STBSP__NUMSZ) - s) < pr)))
               break;
         };
         // get the length that we copied
         cs = (uint32_t)((num + STBSP__NUMSZ) - s);
         l = cs;
         // copy it
         goto scopy;

      case 'u': // unsigned
      case 'i':
      case 'd': // integer
         // get the integer and abs it
         if (fl & STBSP__INTMAX)
         {
            int64_t i64 = va_arg(va, int64_t);
            n64 = (uint64_t)i64;
            if ((f[0] != 'u') && (i64 < 0))
            {
               n64 = (uint64_t)-i64;
               fl |= STBSP__NEGATIVE;
            }
         }
         else
         {
            int32_t i = va_arg(va, int32_t);
            n64 = (uint32_t)i;
            if ((f[0] != 'u') && (i < 0))
            {
               n64 = (uint32_t)-i;
               fl |= STBSP__NEGATIVE;
            }
         }

         // convert to string
         s = num + STBSP__NUMSZ;

         for (;;)
         {
            // do in 32-bit chunks
            char *o = s - 8;
            if (n64 >= 100000000)
            {
#if __SIZEOF_POINTER__ == 8
               n = (uint32_t)(n64 % 100000000);
               n64 /= 100000000;
#else
               uint64_t den = 100000000ULL;
               uint64_t rem = n64;
               uint64_t quot = 0, qbit = 1;

               while ((int64_t)den >= 0)
               {
                  den <<= 1;
                  qbit <<= 1;
               }
               while (qbit)
               {
                  if (den <= rem)
                  {
                     rem -= den;
                     quot += qbit;
                  }
                  den >>= 1;
                  qbit >>= 1;
               }
               n = (uint32_t)rem;
               n64 = quot;
#endif
            }
            else
            {
               n = (uint32_t)n64;
               n64 = 0;
            }
            do
            {
               s -= 2;
               *(uint16_t *)s = *(uint16_t *)&stbsp__digitpair.pair[(n % 100) * 2];
               n /= 100;
            } while (n);
            if (n64 == 0)
            {
               if ((s[0] == '0') && (s != (num + STBSP__NUMSZ)))
                  ++s;
               break;
            }
            while (s != o)
               *--s = '0';
         }

         tail[0] = 0;
         stbsp__lead_sign(fl, lead);

         // get the length that we copied
         l = (uint32_t)((num + STBSP__NUMSZ) - s);
         if (l == 0)
         {
            *--s = '0';
            l = 1;
         }
         cs = l;
         if (pr < 0)
            pr = 0;

      scopy:
         // get fw=leading/trailing space, pr=leading zeros
         if (pr < (int32_t)l)
            pr = l;
         n = pr + lead[0] + tail[0] + tz;
         if (fw < (int32_t)n)
            fw = n;
         fw -= n;
         pr -= l;

         // handle right justify and leading zeros
         if ((fl & STBSP__LEFTJUST) == 0)
         {
            if (fl & STBSP__LEADINGZERO) // if leading zeros, everything is in pr
            {
               pr = (fw > pr) ? fw : pr;
               fw = 0;
            }
         }

         // copy the spaces and/or zeros
         if (fw + pr)
         {
            int32_t i;

            // copy leading spaces (or when doing %8.4d stuff)
            if ((fl & STBSP__LEFTJUST) == 0)
               while (fw > 0)
               {
                  stbsp__cb_buf_clamp(i, fw);
                  fw -= i;
                  while (i)
                  {
                     if ((((uintptr_t)bf) & 3) == 0)
                        break;
                     *bf++ = ' ';
                     --i;
                  }
                  while (i >= 4)
                  {
                     *(uint32_t *)bf = 0x20202020;
                     bf += 4;
                     i -= 4;
                  }
                  while (i)
                  {
                     *bf++ = ' ';
                     --i;
                  }
                  stbsp__chk_cb_buf(1);
               }

            // copy leader
            sn = lead + 1;
            while (lead[0])
            {
               stbsp__cb_buf_clamp(i, lead[0]);
               lead[0] -= (char)i;
               while (i)
               {
                  *bf++ = *sn++;
                  --i;
               }
               stbsp__chk_cb_buf(1);
            }

            // copy leading zeros
            while (pr > 0)
            {
               stbsp__cb_buf_clamp(i, pr);
               pr -= i;
               while (i)
               {
                  if ((((uintptr_t)bf) & 3) == 0)
                     break;
                  *bf++ = '0';
                  --i;
               }
               while (i >= 4)
               {
                  *(uint32_t *)bf = 0x30303030;
                  bf += 4;
                  i -= 4;
               }
               while (i)
               {
                  *bf++ = '0';
                  --i;
               }
               stbsp__chk_cb_buf(1);
            }
         }

         // copy leader if there is still one
         sn = lead + 1;
         while (lead[0])
         {
            int32_t i;
            stbsp__cb_buf_clamp(i, lead[0]);
            lead[0] -= (char)i;
            while (i)
            {
               *bf++ = *sn++;
               --i;
            }
            stbsp__chk_cb_buf(1);
         }

         // copy the string
         n = l;
         while (n)
         {
            int32_t i;
            stbsp__cb_buf_clamp(i, n);
            n -= i;

            while (i)
            {
               *bf++ = *s++;
               --i;
            }
            stbsp__chk_cb_buf(1);
         }

         // copy trailing zeros
         while (tz)
         {
            int32_t i;
            stbsp__cb_buf_clamp(i, tz);
            tz -= i;
            while (i)
            {
               if ((((uintptr_t)bf) & 3) == 0)
                  break;
               *bf++ = '0';
               --i;
            }
            while (i >= 4)
            {
               *(uint32_t *)bf = 0x30303030;
               bf += 4;
               i -= 4;
            }
            while (i)
            {
               *bf++ = '0';
               --i;
            }
            stbsp__chk_cb_buf(1);
         }

         // copy tail if there is one
         sn = tail + 1;
         while (tail[0])
         {
            int32_t i;
            stbsp__cb_buf_clamp(i, tail[0]);
            tail[0] -= (char)i;
            while (i)
            {
               *bf++ = *sn++;
               --i;
            }
            stbsp__chk_cb_buf(1);
         }

         // handle the left justify
         if (fl & STBSP__LEFTJUST)
            if (fw > 0)
            {
               while (fw)
               {
                  int32_t i;
                  stbsp__cb_buf_clamp(i, fw);
                  fw -= i;
                  while (i)
                  {
                     if ((((uintptr_t)bf) & 3) == 0)
                        break;
                     *bf++ = ' ';
                     --i;
                  }
                  while (i >= 4)
                  {
                     *(uint32_t *)bf = 0x20202020;
                     bf += 4;
                     i -= 4;
                  }
                  while (i--)
                     *bf++ = ' ';
                  stbsp__chk_cb_buf(1);
               }
            }
         break;

      default: // unknown, just copy code
         s = num + STBSP__NUMSZ - 1;
         *s = f[0];
         l = 1;
         fw = fl = 0;
         lead[0] = 0;
         tail[0] = 0;
         pr = 0;
         cs = 0;
         goto scopy;
      }
      ++f;
   }
endfmt:
   /* flush remainder */
   if (bf > buf)
      sys_write(1, buf, (int)(bf - buf));

   va_end(va);
   return;
}

// cleanup
#undef STBSP__LEFTJUST
#undef STBSP__LEADINGPLUS
#undef STBSP__LEADINGSPACE
#undef STBSP__LEADING_0X
#undef STBSP__LEADINGZERO
#undef STBSP__INTMAX
#undef STBSP__NEGATIVE
#undef STBSP__NUMSZ
#undef stbsp__chk_cb_bufL
#undef stbsp__chk_cb_buf
#undef stbsp__flush_cb
#undef stbsp__cb_buf_clamp
#undef STBSP__UNALIGNED

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
