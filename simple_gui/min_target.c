#define TABLE_SIZE (16777216 * 2)
#define RNAB_DEBUG
#define RNAB_BENCHMARK

#include "../rnab_engine/rnab.h"

#ifdef __wasi__

void _start(void)
{
    rnab_benchmark();
}

#elif defined(__APPLE__)

void start_c(void);

__attribute__((naked)) int main(void)
{
    __asm__(
        "mov  x9, sp\n"
        "and  x9, x9, #-16\n"
        "mov  sp, x9\n"
        "b    _start_c\n");
}

__attribute__((noreturn, used)) void start_c(void)
{
    rnab_benchmark();
    register int x0 __asm__("x0") = 0;
    register int x16 __asm__("x16") = 1;
    __asm__ volatile("svc #0x80" ::"r"(x0), "r"(x16) : "memory");
    __builtin_unreachable();
}

#elif defined(_WIN64) || defined(_WIN32)

void mainCRTStartup(void)
{
    #ifdef RNAB_DEBUG
    QueryPerformanceFrequency(&_qpf_freq);
    #endif
    rnab_benchmark();
}

#else

#if __SIZEOF_POINTER__ == 8

#if defined(__x86_64__)
__attribute__((naked, noreturn, section(".text._start"))) void _start(void)
{
    __asm__ volatile(
        "xorq  %%rbp, %%rbp\n\t"
        "andq  $-16,  %%rsp\n\t"
        "call  rnab_benchmark\n\t"
        "xorl  %%edi, %%edi\n\t"
        "movl  $60,   %%eax\n\t"
        "syscall" ::: "memory");
}

#elif defined(__aarch64__)
__attribute__((naked, noreturn, section(".text._start"))) void _start(void)
{
    __asm__ volatile(
        "mov   x29, #0\n\t"
        "mov   x30, #0\n\t"
        "mov   x9,  sp\n\t"
        "and   x9,  x9, #-16\n\t"
        "mov   sp,  x9\n\t"
        "bl    rnab_benchmark\n\t"
        "mov   x0,  #0\n\t"
        "mov   x8,  #93\n\t"
        "svc   #0" ::: "memory");
}

#else
#error "Unknown 64-bit architecture"
#endif

#else

#if defined(__i386__)
__attribute__((naked, noreturn, section(".text._start"))) void _start(void)
{
    __asm__ volatile(
        "xorl  %%ebp, %%ebp\n\t"
        "andl  $-16,  %%esp\n\t"
        "call  rnab_benchmark\n\t"
        "xorl  %%ebx, %%ebx\n\t"
        "movl  $1,    %%eax\n\t"
        "int   $0x80" ::: "memory");
}

#elif defined(__arm__)
__attribute__((naked, noreturn, section(".text._start"))) void _start(void)
{
    __asm__ volatile(
        "mov   fp, #0\n\t"
        "mov   lr, #0\n\t"
        "mov   r0, sp\n\t"
        "bic   r0, r0, #7\n\t"
        "mov   sp, r0\n\t"
        "bl    rnab_benchmark\n\t"
        "mov   r0, #0\n\t"
        "mov   r7, #1\n\t"
        "swi   #0" ::: "r0", "memory");
}

#else
#error "Unknown 32-bit architecture"
#endif

#endif

#endif
