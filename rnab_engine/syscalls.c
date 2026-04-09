#if defined(__APPLE__)
#define STATIC_BSS static __attribute__((section("__DATA,__bss")))
#else
#define STATIC_BSS static __attribute__((section(".bss")))
#endif

STATIC_BSS volatile bool should_exit = false;

#if __SIZEOF_POINTER__ == 8

#if defined(__x86_64__)

#define CLEAR_TT()                            \
    __asm__ volatile(                         \
        "vpxor    %%xmm0, %%xmm0, %%xmm0\n\t" \
                                              \
        "movq     %0, %%rdi\n\t"              \
        "movq     %1, %%rcx\n\t"              \
        "1:\n\t"                              \
        "movdqu   %%xmm0,   0(%%rdi)\n\t"     \
        "movdqu   %%xmm0,  16(%%rdi)\n\t"     \
        "movdqu   %%xmm0,  32(%%rdi)\n\t"     \
        "movdqu   %%xmm0,  48(%%rdi)\n\t"     \
        "movdqu   %%xmm0,  64(%%rdi)\n\t"     \
        "movdqu   %%xmm0,  80(%%rdi)\n\t"     \
        "movdqu   %%xmm0,  96(%%rdi)\n\t"     \
        "movdqu   %%xmm0, 112(%%rdi)\n\t"     \
        "addq     $128,   %%rdi\n\t"          \
        "decq     %%rcx\n\t"                  \
        "jnz      1b\n\t"                     \
        :                                     \
        : "r"((void *)table),                 \
          "i"(sizeof(table) / 128)            \
        : "rdi", "rcx", "xmm0", "memory");
#elif defined(__aarch64__)

#define CLEAR_TT()                   \
    __asm__ volatile(                \
        "movi v0.16b, #0\n\t"        \
                                     \
        "mov  x0, %0\n\t"            \
        "mov  x1, %1\n\t"            \
        "1:\n\t"                     \
        "stp  q0, q0, [x0,  #0]\n\t" \
        "stp  q0, q0, [x0, #32]\n\t" \
        "stp  q0, q0, [x0, #64]\n\t" \
        "stp  q0, q0, [x0, #96]\n\t" \
        "add  x0, x0, #128\n\t"      \
        "subs x1, x1, #1\n\t"        \
        "bne  1b\n\t"                \
        :                            \
        : "r"((void *)table),        \
          "i"(sizeof(table) / 128)   \
        : "x0", "x1", "v0", "memory")

#elif defined(__wasm__)

#undef RNAB_MT // not supported

#define CLEAR_TT() \
    __builtin_memset(table, 0, sizeof(table));

#else
#error "Unknown 64-bit arch"
#endif

#else

#undef RNAB_MT // cant perform atomic 8-byte reads/writes

#if defined(__i386__)

#define CLEAR_TT()                     \
    __asm__ volatile(                  \
        "xorl    %%eax, %%eax\n\t"     \
                                       \
        "movl    %0, %%edi\n\t"        \
        "movl    %1, %%ecx\n\t"        \
        "1:\n\t"                       \
        "movl    %%eax,  0(%%edi)\n\t" \
        "movl    %%eax,  4(%%edi)\n\t" \
        "movl    %%eax,  8(%%edi)\n\t" \
        "movl    %%eax, 12(%%edi)\n\t" \
        "movl    %%eax, 16(%%edi)\n\t" \
        "movl    %%eax, 20(%%edi)\n\t" \
        "movl    %%eax, 24(%%edi)\n\t" \
        "movl    %%eax, 28(%%edi)\n\t" \
        "addl    $32,   %%edi\n\t"     \
        "decl    %%ecx\n\t"            \
        "jnz     1b\n\t"               \
        :                              \
        : "r"((void *)table),          \
          "i"(sizeof(table) / 32)      \
        : "eax", "edi", "ecx", "memory");

#elif defined(__arm__)

#define CLEAR_TT()                \
    __asm__ volatile(             \
        "mov  r2, #0\n\t"         \
                                  \
        "mov  r0, %0\n\t"         \
        "mov  r1, %1\n\t"         \
        "1:\n\t"                  \
        "str  r2, [r0, #0]\n\t"   \
        "str  r2, [r0, #4]\n\t"   \
        "str  r2, [r0, #8]\n\t"   \
        "str  r2, [r0, #12]\n\t"  \
        "str  r2, [r0, #16]\n\t"  \
        "str  r2, [r0, #20]\n\t"  \
        "str  r2, [r0, #24]\n\t"  \
        "str  r2, [r0, #28]\n\t"  \
        "add  r0, r0, #32\n\t"    \
        "subs r1, r1, #1\n\t"     \
        "bne  1b\n\t"             \
        :                         \
        : "r"((void *)table),     \
          "i"(sizeof(table) / 32) \
        : "r0", "r1", "r2", "memory")

#elif defined(__wasm__)

#define CLEAR_TT() \
    __builtin_memset(table, 0, sizeof(table));

#else
#error "Unknown 32-bit arch"
#endif

#endif

#if defined(_WIN32) || defined(_WIN64)

static LARGE_INTEGER _qpf_freq;
static HANDLE _win_event = NULL;
static HANDLE _win_thread = NULL;
static HANDLE g_wake_event;

BOOL WINAPI DllMainCRTStartup(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;

#ifdef RNAB_DEBUG
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        QueryPerformanceFrequency(&_qpf_freq);
        g_wake_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    }
#endif
    return TRUE;
}

#define TIME_TYPE LARGE_INTEGER
#define get_time(v) QueryPerformanceCounter(&(v))

static uint64_t get_time_uint(TIME_TYPE t)
{
    return t.QuadPart * 1000000ULL / _qpf_freq.QuadPart;
}

static uint64_t get_time_diff_millis(TIME_TYPE stop, TIME_TYPE start)
{
    return ((stop).QuadPart - (start).QuadPart) * 1000ULL / _qpf_freq.QuadPart;
}

static uint64_t get_time_diff_micros(TIME_TYPE stop, TIME_TYPE start)
{
    return ((stop).QuadPart - (start).QuadPart) * 1000000ULL / _qpf_freq.QuadPart;
}

static inline void sys_write(int fd, const void *buf, int len)
{
    (void)fd;
    HANDLE h = (HANDLE)((LONG_PTR)-11); /* STD_OUTPUT_HANDLE */
    DWORD w;
    WriteFile(h, buf, (DWORD)len, &w, NULL);
}

static DWORD WINAPI _win_timer_proc(void *arg)
{
    DWORD ms = (DWORD)(ULONG_PTR)arg;
    if (WaitForSingleObject(_win_event, ms) == WAIT_TIMEOUT)
        should_exit = true;
    return 0;
}

static inline __attribute__((always_inline)) void start_search_timer(uint32_t ms)
{
    should_exit = false;

    if (ms == UINT32_MAX) /* unlimited search: no thread needed */
        return;

    _win_event = CreateEventA(NULL, 0, 0, NULL);
    if (!_win_event) /* well no timer for you I guess */
        return;

    _win_thread = CreateThread(NULL, 0, _win_timer_proc, (void *)(ULONG_PTR)ms, 0, NULL);
    if (!_win_thread) /* almost got it but still no timer */
    {
        CloseHandle(_win_event);
        _win_event = NULL;
    }
}

static inline __attribute__((always_inline)) void stop_search_timer(void)
{
    if (!_win_event) /* unlimited search or error: nothing to tear down */
        return;

    SetEvent(_win_event);                       /* wake thread; it exits cleanly */
    WaitForSingleObject(_win_thread, INFINITE); /* wait for it to fully exit before freeing */
    CloseHandle(_win_thread);
    CloseHandle(_win_event);
    _win_thread = _win_event = NULL;
    should_exit = false;
}

/* We dont directly support apple but these ones work on the jailbroken aarch64 devices */
#elif defined(__APPLE__)

#undef RNAB_MT // not supported

#if defined(__aarch64__)

#define sys_write(fd, buf, len)                                                   \
    do                                                                            \
    {                                                                             \
        register long _x0 __asm__("x0") = (long)(fd);                             \
        register const void *_x1 __asm__("x1") = (buf);                           \
        register long _x2 __asm__("x2") = (long)(len);                            \
        register long _x16 __asm__("x16") = 4;                                    \
        __asm__ volatile("svc #0x80\n"                                            \
                         : "+r"(_x0) : "r"(_x1), "r"(_x2), "r"(_x16) : "memory"); \
    } while (0)

#define TIME_TYPE uint64_t

typedef long time_t;
typedef int suseconds_t;
struct timeval
{
    time_t tv_sec;
    suseconds_t tv_usec;
};

#define get_time(t)                                                    \
    do                                                                 \
    {                                                                  \
        struct timeval tv;                                             \
        register struct timeval *x0 __asm__("x0") = &tv;               \
        register int x1 __asm__("x1") = 0;     /* tzp = NULL */        \
        register int x16 __asm__("x16") = 116; /* SYS_gettimeofday */  \
        __asm__ volatile(                                              \
            "svc #0x80"                                                \
            : "+r"(x0)                                                 \
            : "r"(x1), "r"(x16)                                        \
            : "memory", "cc");                                         \
        (t) = (unsigned long long)tv.tv_sec * 1000000ULL + tv.tv_usec; \
    } while (0)

#define get_time_uint(t) \
    (t)

#define get_time_diff_millis(stop, start) \
    ((uint64_t)(((stop) - (start)) / 1000ULL))

#define get_time_diff_micros(stop, start) \
    ((uint64_t)((stop) - (start)))

#else
#error "Apple: unsupported architecture (need aarch64)"
#endif

/* broken */
#define start_search_timer(...)
#define stop_search_timer(...)

#elif defined(__linux__) || defined(__ANDROID__)

#define _SIGALRM 14
#define _SIGEV_THREAD_ID 4
#define _CLOCK_MONOTONIC 1

struct _kern_sigevent
{
    long sigev_value; /* sigval_t: pointer-sized          */
    int sigev_signo;
    int sigev_notify;           /* SIGEV_THREAD_ID = 4              */
    int sigev_notify_thread_id; /* first field of the trailing union */
#if __SIZEOF_POINTER__ == 8
    int _pad[11];
#else
    int _pad[12];
#endif
};

struct _kern_timespec
{
    long tv_sec;
    long tv_nsec;
};

struct _kern_itimerspec
{
    struct _kern_timespec it_interval;
    struct _kern_timespec it_value;
};

typedef struct _kern_timespec TIME_TYPE;

#define _TV_SEC(t) ((t).tv_sec)
#define _TV_NSEC(t) ((t).tv_nsec)

/* Avoid 64-bit divisions */

#define get_time_uint(t) \
    (_TV_SEC(t) * (int64_t)1000000 + (int32_t)(_TV_NSEC(t)) / 1000)

#define get_time_diff_millis(stop, start) \
    (uint64_t)((_TV_SEC(stop) - _TV_SEC(start)) * (int64_t)1000 + (int32_t)(_TV_NSEC(stop) - _TV_NSEC(start)) / 1000000)

#define get_time_diff_micros(stop, start) \
    (uint64_t)((_TV_SEC(stop) - _TV_SEC(start)) * (int64_t)1000000 + (int32_t)(_TV_NSEC(stop) - _TV_NSEC(start)) / 1000)

#if defined(__x86_64__)

#define _SYSCALL4(nr, a0, a1, a2, a3)                    \
    do                                                   \
    {                                                    \
        register long _rax __asm__("rax") = (long)(nr);  \
        register long _rdi __asm__("rdi") = (long)(a0);  \
        register long _rsi __asm__("rsi") = (long)(a1);  \
        register long _rdx __asm__("rdx") = (long)(a2);  \
        register long _r10 __asm__("r10") = (long)(a3);  \
        __asm__ volatile(                                \
            "syscall\n"                                  \
            : "+r"(_rax)                                 \
            : "r"(_rdi), "r"(_rsi), "r"(_rdx), "r"(_r10) \
            : "rcx", "r11", "memory");                   \
    } while (0)

static void __attribute__((naked)) _signal_restorer(void)
{
    __asm__ volatile("mov $15, %rax\nsyscall\n");
}

#define GET_TIME_SYSCALL(out_var)                                                       \
    do                                                                                  \
    {                                                                                   \
        register long _rax __asm__("rax") = 228;                                        \
        register long _rdi __asm__("rdi") = _CLOCK_MONOTONIC;                           \
        register struct _kern_timespec *_rsi __asm__("rsi") = &(out_var);               \
        __asm__ volatile("syscall\n"                                                    \
                         : "+r"(_rax) : "r"(_rdi), "r"(_rsi) : "rcx", "r11", "memory"); \
    } while (0)

#define sys_write(fd, buf, len)                                                                    \
    do                                                                                             \
    {                                                                                              \
        register long _rax __asm__("rax") = 1;                                                     \
        register long _rdi __asm__("rdi") = (long)(fd);                                            \
        register const void *_rsi __asm__("rsi") = (buf);                                          \
        register long _rdx __asm__("rdx") = (long)(len);                                           \
        __asm__ volatile("syscall\n"                                                               \
                         : "+r"(_rax) : "r"(_rdi), "r"(_rsi), "r"(_rdx) : "rcx", "r11", "memory"); \
    } while (0)

static inline int _sys_gettid(void)
{
    int tid;
    __asm__ volatile("mov $186, %%eax\nsyscall\n"
                     : "=a"(tid)::"rcx", "r11", "memory");
    return tid;
}

#elif defined(__aarch64__)

static inline uint64_t rbit64(uint64_t x)
{
#if defined(__clang__)
    return __builtin_bitreverse64(x);
#elif defined(__GNUC__)
    uint64_t result;
    __asm__("rbit %0, %1" : "=r"(result) : "r"(x));
    return result;
#else
#error "rbit64: unsupported compiler/arch"
#endif
}

#define _SYSCALL4(nr, a0, a1, a2, a3)                 \
    do                                                \
    {                                                 \
        register long _x8 __asm__("x8") = (long)(nr); \
        register long _x0 __asm__("x0") = (long)(a0); \
        register long _x1 __asm__("x1") = (long)(a1); \
        register long _x2 __asm__("x2") = (long)(a2); \
        register long _x3 __asm__("x3") = (long)(a3); \
        __asm__ volatile(                             \
            "svc #0\n"                                \
            : "+r"(_x0)                               \
            : "r"(_x8), "r"(_x1), "r"(_x2), "r"(_x3)  \
            : "memory", "x4", "x5", "x6", "x7");      \
    } while (0)

static void __attribute__((naked)) _signal_restorer(void)
{
    __asm__ volatile(
        "mov x8, #139\n"
        "svc #0\n");
}

#define GET_TIME_SYSCALL(out_var)                                       \
    do                                                                  \
    {                                                                   \
        register long _r0 __asm__("x0") = _CLOCK_MONOTONIC;             \
        register struct _kern_timespec *_r1 __asm__("x1") = &(out_var); \
        register long _r8 __asm__("x8") = 113;                          \
        __asm__ volatile("svc #0\n"                                     \
                         : "+r"(_r0) : "r"(_r1), "r"(_r8) : "memory");  \
    } while (0)

#define sys_write(fd, buf, len)                                                  \
    do                                                                           \
    {                                                                            \
        register long _r0 __asm__("x0") = (long)(fd);                            \
        register const void *_r1 __asm__("x1") = (buf);                          \
        register long _r2 __asm__("x2") = (long)(len);                           \
        register long _r8 __asm__("x8") = 64;                                    \
        __asm__ volatile("svc #0\n"                                              \
                         : "+r"(_r0) : "r"(_r1), "r"(_r2), "r"(_r8) : "memory"); \
    } while (0)

static inline int _sys_gettid(void)
{
    register long _x8 __asm__("x8") = 178;
    register long _x0 __asm__("x0");
    __asm__ volatile("svc #0\n" : "=r"(_x0) : "r"(_x8) : "memory");
    return (int)_x0;
}

#elif defined(__arm__)

#define _SYSCALL4(nr, a0, a1, a2, a3)                              \
    do                                                             \
    {                                                              \
        register int _r7 __asm__("r7") = (int)(nr);                \
        register int _r0 __asm__("r0") = (int)(unsigned long)(a0); \
        register int _r1 __asm__("r1") = (int)(unsigned long)(a1); \
        register int _r2 __asm__("r2") = (int)(unsigned long)(a2); \
        register int _r3 __asm__("r3") = (int)(unsigned long)(a3); \
        __asm__ volatile(                                          \
            "swi #0\n"                                             \
            : "+r"(_r0)                                            \
            : "r"(_r7), "r"(_r1), "r"(_r2), "r"(_r3)               \
            : "memory");                                           \
    } while (0)

static void __attribute__((naked)) _signal_restorer(void)
{
    __asm__ volatile(
        "mov r7, #173\n\t" /* NR_rt_sigreturn */
        "swi #0\n\t");
}

#define GET_TIME_SYSCALL(out_var)                                                 \
    do                                                                            \
    {                                                                             \
        struct _kern_timespec _t;                                                 \
        register int _r0 __asm__("r0") = _CLOCK_MONOTONIC;                        \
        register struct _kern_timespec *_r1 __asm__("r1") = &_t;                  \
        register int _r7 __asm__("r7") = 263;                                     \
        __asm__ volatile("swi #0\n" : "+r"(_r0) : "r"(_r1), "r"(_r7) : "memory"); \
        (out_var).tv_sec = (long long)_t.tv_sec;                                  \
        (out_var).tv_nsec = (long long)_t.tv_nsec;                                \
    } while (0)

#define sys_write(fd, buf, len)                                              \
    do                                                                       \
    {                                                                        \
        register int r0 __asm__("r0") = (int)(fd);                           \
        register const void *r1 __asm__("r1") = (buf);                       \
        register int r2 __asm__("r2") = (int)(len);                          \
        register int r7 __asm__("r7") = 4;                                   \
        __asm__ volatile("swi #0\n"                                          \
                         : "+r"(r0) : "r"(r1), "r"(r2), "r"(r7) : "memory"); \
        (void)r0;                                                            \
    } while (0)

static inline int _sys_gettid(void)
{
    register int _r7 __asm__("r7") = 224;
    register int _r0 __asm__("r0");
    __asm__ volatile("swi #0\n" : "=r"(_r0) : "r"(_r7) : "memory");
    return _r0;
}

#elif defined(__i386__)

#define _SYSCALL4(nr, a0, a1, a2, a3)                                  \
    do                                                                 \
    {                                                                  \
        register long _eax __asm__("eax") = (long)(nr);                \
        register long _ebx __asm__("ebx") = (long)(unsigned long)(a0); \
        register long _ecx __asm__("ecx") = (long)(unsigned long)(a1); \
        register long _edx __asm__("edx") = (long)(unsigned long)(a2); \
        register long _esi __asm__("esi") = (long)(unsigned long)(a3); \
        __asm__ volatile(                                              \
            "int $0x80\n"                                              \
            : "+r"(_eax)                                               \
            : "r"(_ebx), "r"(_ecx), "r"(_edx), "r"(_esi)               \
            : "memory");                                               \
    } while (0)

static void __attribute__((naked)) _signal_restorer(void)
{
    __asm__ volatile("mov $173, %eax\nint $0x80\n");
}

#define GET_TIME_SYSCALL(out_var)                                                       \
    do                                                                                  \
    {                                                                                   \
        struct _kern_timespec _t;                                                       \
        register long _eax __asm__("eax") = 265;                                        \
        register long _ebx __asm__("ebx") = _CLOCK_MONOTONIC;                           \
        register struct _kern_timespec *_ecx __asm__("ecx") = &_t;                      \
        __asm__ volatile("int $0x80\n" : "+r"(_eax) : "r"(_ebx), "r"(_ecx) : "memory"); \
        (out_var).tv_sec = (long long)_t.tv_sec;                                        \
        (out_var).tv_nsec = (long long)_t.tv_nsec;                                      \
    } while (0)

#define sys_write(fd, buf, len)                                                      \
    do                                                                               \
    {                                                                                \
        register long _eax __asm__("eax") = 4;                                       \
        register long _ebx __asm__("ebx") = (long)(fd);                              \
        register const void *_ecx __asm__("ecx") = (buf);                            \
        register long _edx __asm__("edx") = (long)(len);                             \
        __asm__ volatile("int $0x80\n"                                               \
                         : "+r"(_eax) : "r"(_ebx), "r"(_ecx), "r"(_edx) : "memory"); \
    } while (0)

static inline int _sys_gettid(void)
{
    int tid;
    __asm__ volatile("mov $224, %%eax\nint $0x80\n"
                     : "=a"(tid)::"memory");
    return tid;
}

#else
#error "Linux: unsupported architecture (need x86_64/aarch64/arm/i386)"
#endif

#define get_time(time_var) GET_TIME_SYSCALL(time_var)

#if defined(__x86_64__)
#define _NR_rt_sigaction 13
#define _NR_rt_sigprocmask 14
#define _NR_timer_create 222
#define _NR_timer_settime 223
#define _NR_timer_delete 226
#elif defined(__aarch64__)
#define _NR_rt_sigaction 134
#define _NR_rt_sigprocmask 135
#define _NR_timer_create 107
#define _NR_timer_settime 110
#define _NR_timer_delete 111
#elif defined(__arm__)
#define _NR_rt_sigaction 174
#define _NR_rt_sigprocmask 175
#define _NR_timer_create 257
#define _NR_timer_settime 258
#define _NR_timer_delete 261
#elif defined(__i386__)
#define _NR_rt_sigaction 174
#define _NR_rt_sigprocmask 175
#define _NR_timer_create 259
#define _NR_timer_settime 260
#define _NR_timer_delete 263
#endif

#define _SIG_BLOCK 0
#define _SIG_UNBLOCK 1

#if defined(__arm__) || defined(__i386__)
struct _kern_sigaction
{
    union
    {
        void (*_sa_handler)(int);
        void (*_sa_sigaction)(int, void *, void *);
    } _u;                      /* offset  0 */
    unsigned long sa_mask[2];  /* offset  4 */
    unsigned long sa_flags;    /* offset 12 */
    void (*sa_restorer)(void); /* offset 16 */
};
#elif defined(__x86_64__) || defined(__aarch64__)
struct _kern_sigaction
{
    union
    {
        void (*_sa_handler)(int);
        void (*_sa_sigaction)(int, void *, void *);
    } _u;                      /* offset  0 */
    unsigned long sa_flags;    /* offset  8 */
    void (*sa_restorer)(void); /* offset 16 */
    unsigned long sa_mask[1];  /* offset 24 */
};
#endif

#define _SA_RESTORER 0x04000000UL
#define _SA_SIGINFO 0x00000004UL

static inline void _sys_rt_sigaction(const struct _kern_sigaction *sa)
{
    _SYSCALL4(_NR_rt_sigaction, _SIGALRM, sa, 0, 8);
}

static inline void _sys_rt_sigprocmask(int how, const unsigned long mask[2], unsigned long old_mask[2])
{
    _SYSCALL4(_NR_rt_sigprocmask, how, mask, old_mask, 8);
}

static inline void _sys_timer_create(const struct _kern_sigevent *sev, int *timer_id)
{
    _SYSCALL4(_NR_timer_create, _CLOCK_MONOTONIC, sev, timer_id, 0);
}

static inline void _sys_timer_settime(int timer_id, const struct _kern_itimerspec *its)
{
    _SYSCALL4(_NR_timer_settime, timer_id, 0, its, 0);
}

static inline void _sys_timer_delete(int timer_id)
{
    _SYSCALL4(_NR_timer_delete, timer_id, 0, 0, 0);
}

static void _handle_alarm(int sig, void *si, void *uctx)
{
    (void)sig;
    (void)si;
    (void)uctx;
    should_exit = true;
}

static int _linux_timer_id = -1;
static const unsigned long _sigalrm_mask[2] = {1UL << (_SIGALRM - 1), 0UL};

static inline void start_search_timer(uint32_t ms)
{
    if (ms == UINT32_MAX) /* unlimited search */
        return;

    should_exit = false;

    struct _kern_sigaction sa = {
        ._u._sa_sigaction = _handle_alarm,
        .sa_flags = _SA_RESTORER | _SA_SIGINFO,
        .sa_restorer = _signal_restorer,
        .sa_mask = {0},
    };

    _sys_rt_sigaction(&sa);

    struct _kern_sigevent sev = {
        .sigev_value = 0,
        .sigev_signo = _SIGALRM,
        .sigev_notify = _SIGEV_THREAD_ID,
        .sigev_notify_thread_id = _sys_gettid(),
    };
    _sys_timer_create(&sev, &_linux_timer_id);

    struct _kern_itimerspec its = {
        {0, 0},
        {(long)(ms / 1000), (long)((ms % 1000) * 1000000)},
    };
    _sys_timer_settime(_linux_timer_id, &its);
}

static inline void stop_search_timer(void)
{
    if (_linux_timer_id == -1)
        return;

    _sys_rt_sigprocmask(_SIG_BLOCK, _sigalrm_mask, 0);

    struct _kern_sigaction ign = {
        ._u._sa_handler = (void (*)(int))1, /* SIG_IGN */
    };
    _sys_rt_sigaction(&ign);

    _sys_timer_delete(_linux_timer_id);
    _linux_timer_id = -1;

    _sys_rt_sigprocmask(_SIG_UNBLOCK, _sigalrm_mask, 0);
}

#elif defined(__wasi__)

#define TIME_TYPE uint64_t

typedef struct
{
    const char *buf;
    unsigned int buf_len;
} __wasi_ciovec_t;

__attribute__((__import_module__("wasi_snapshot_preview1"), __import_name__("fd_write"))) extern int __wasi_fd_write(int, const __wasi_ciovec_t *, int, int *);

__attribute__((__import_module__("wasi_snapshot_preview1"), __import_name__("clock_time_get"))) extern int __wasi_clock_time_get(unsigned int, unsigned long long, unsigned long long *);

#define sys_write(fd, buf, len)                              \
    do                                                       \
    {                                                        \
        int _nw;                                             \
        __wasi_ciovec_t _iov = {(buf), (unsigned int)(len)}; \
        __wasi_fd_write((fd), &_iov, 1, &_nw);               \
    } while (0)

#define get_time(t)                        \
    do                                     \
    {                                      \
        unsigned long long _ns;            \
        __wasi_clock_time_get(1, 1, &_ns); \
        (t) = _ns;                         \
    } while (0)

#define get_time_uint(t) \
    ((uint64_t)(t) / 1000ULL)

#define get_time_diff_millis(stop, start) \
    ((uint64_t)(((stop) - (start)) / 1000000ULL))

#define get_time_diff_micros(stop, start) \
    ((uint64_t)(((stop) - (start)) / 1000ULL))

/* not supported yet */
#define start_search_timer(...)
#define stop_search_timer(...)

#else
#error "Unsupported platform"
#endif

#if defined(__linux__)

STATIC_BSS uint8_t stack_buffer[MAX_THREADS * STACK_SIZE] __attribute__((aligned(4096)));

#define CLONE_VM 0x00000100
#define CLONE_FS 0x00000200
#define CLONE_FILES 0x00000400
#define CLONE_SIGHAND 0x00000800
#define CLONE_THREAD 0x00010000
#define CLONE_SYSVSEM 0x00040000
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_PRIVATE_FLAG 128

#define THREAD_FLAGS \
    (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM)

#define get_thread_count() 4 // to be implemented

#if defined(__x86_64__)

#define SYS_CLONE 56
#define SYS_EXIT 60
#define SYS_FUTEX 202

static inline void sys_exit(void)
{
    __asm__ volatile("syscall" ::"a"(SYS_EXIT), "D"(0));
    __builtin_unreachable();
}

#define thread_create(stack_top, fn)                         \
    long tid;                                                \
    __asm__ volatile(                                        \
        "syscall"                                            \
        : "=a"(tid)                                          \
        : "0"(SYS_CLONE), "D"(THREAD_FLAGS), "S"(stack_top), \
          "d"(0), "r"((long)0), "r"((long)0)                 \
        : "rcx", "r11", "memory");                           \
                                                             \
    if (tid == 0)                                            \
    {                                                        \
        fn();                                                \
        sys_exit();                                          \
    }                                                        \
    if (tid < 0) /* failure */                               \
    {                                                        \
        break;                                               \
    }

#define resume_main_thread()                                                       \
    do                                                                             \
    {                                                                              \
        long ret;                                                                  \
        register long r10 __asm__("r10") = 0; /* timeout  = NULL */                \
        register long r8 __asm__("r8") = 0;   /* uaddr2   = NULL */                \
        register long r9 __asm__("r9") = 0;   /* val3     = 0    */                \
                                                                                   \
        __asm__ volatile(                                                          \
            "syscall"                                                              \
            : "=a"(ret)                                                            \
            : "a"(SYS_FUTEX),                                                      \
              "D"(&queue_head),                           /* uaddr  */             \
              "S"((long)FUTEX_WAKE | FUTEX_PRIVATE_FLAG), /* FUTEX_WAKE */         \
              "d"((long)1),                               /* val: wake 1 waiter */ \
              "r"(r10), "r"(r8), "r"(r9)                                           \
            : "rcx", "r11", "memory");                                             \
    } while (0);

#define wait_for_queue()                                                           \
    do                                                                             \
    {                                                                              \
        int cur_val = atomic_load(&queue_head);                                    \
        while (cur_val < queue_size + thread_num)                                  \
        {                                                                          \
            long ret;                                                              \
            register long r10 __asm__("r10") = 0; /* timeout = NULL */             \
            register long r8 __asm__("r8") = 0;   /* uaddr2  = NULL */             \
            register long r9 __asm__("r9") = 0;   /* val3    = 0    */             \
                                                                                   \
            __asm__ volatile(                                                      \
                "syscall"                                                          \
                : "=a"(ret)                                                        \
                : "a"(SYS_FUTEX),                                                  \
                  "D"(&queue_head),                           /* uaddr          */ \
                  "S"((long)FUTEX_WAIT | FUTEX_PRIVATE_FLAG), /* FUTEX_WAIT     */ \
                  "d"((long)cur_val),                         /* expected value */ \
                  "r"(r10), "r"(r8), "r"(r9)                                       \
                : "rcx", "r11", "memory");                                         \
                                                                                   \
            cur_val = atomic_load(&queue_head);                                    \
        }                                                                          \
    } while (0);

#elif defined(__i386__)

#define SYS_CLONE 120
#define SYS_EXIT 1
#define SYS_FUTEX 240

static inline void sys_exit(void)
{
    __asm__ volatile(
        "int $0x80" ::"a"(SYS_EXIT), "b"(0)
        : "memory");
    __builtin_unreachable();
}

#define thread_create(stack_top, fn)      \
    long tid;                             \
                                          \
    __asm__ volatile(                     \
        "int $0x80"                       \
        : "=a"(tid)                       \
        : "0"(SYS_CLONE),                 \
          "b"(THREAD_FLAGS), /* flags  */ \
          "c"(stack_top),    /* stack  */ \
          "d"(0),            /* ptid   */ \
          "S"(0),            /* ctid   */ \
          "D"(0)             /* tls    */ \
        : "memory");                      \
                                          \
    if (tid == 0)                         \
    {                                     \
        fn();                             \
        sys_exit();                       \
    }                                     \
    if (tid < 0) /* failure */            \
    {                                     \
        break;                            \
    }

#define resume_main_thread()                                             \
    do                                                                   \
    {                                                                    \
        long ret;                                                        \
                                                                         \
        __asm__ volatile(                                                \
            "pushl %%ebp        \n\t"                                    \
            "movl  $0, %%ebp   \n\t" /* val3 = 0 */                      \
            "int   $0x80        \n\t"                                    \
            "popl  %%ebp        \n\t"                                    \
            : "=a"(ret)                                                  \
            : "a"(SYS_FUTEX),                                            \
              "b"(&queue_head),                     /* uaddr      */     \
              "c"(FUTEX_WAKE | FUTEX_PRIVATE_FLAG), /* FUTEX_WAKE */     \
              "d"(1),                               /* val: wake 1 */    \
              "S"(0),                               /* timeout = NULL */ \
              "D"(0)                                /* uaddr2  = NULL */ \
            : "memory");                                                 \
    } while (0);

#define wait_for_queue()                                                     \
    do                                                                       \
    {                                                                        \
        int cur_val = atomic_load(&queue_head);                              \
        while (cur_val < queue_size + thread_num)                            \
        {                                                                    \
            long ret;                                                        \
            __asm__ volatile(                                                \
                "pushl %%ebp        \n\t"                                    \
                "movl  $0, %%ebp   \n\t" /* val3 = 0 */                      \
                "int   $0x80        \n\t"                                    \
                "popl  %%ebp        \n\t"                                    \
                : "=a"(ret)                                                  \
                : "a"(SYS_FUTEX),                                            \
                  "b"(&queue_head),                     /* uaddr          */ \
                  "c"(FUTEX_WAIT | FUTEX_PRIVATE_FLAG), /* FUTEX_WAIT     */ \
                  "d"(cur_val),                         /* expected value */ \
                  "S"(0),                               /* timeout = NULL */ \
                  "D"(0)                                /* uaddr2  = NULL */ \
                : "memory");                                                 \
                                                                             \
            cur_val = atomic_load(&queue_head);                              \
        }                                                                    \
    } while (0);

#elif defined(__aarch64__)

#define SYS_CLONE 220
#define SYS_EXIT 93
#define SYS_FUTEX 98

static inline void sys_exit(void)
{
    register long x8 __asm__("x8") = SYS_EXIT;
    register long x0 __asm__("x0") = 0;
    __asm__ volatile(
        "svc #0" ::"r"(x8), "r"(x0)
        : "memory");
    __builtin_unreachable();
}

#define thread_create(stack_top, fn)                  \
    register long x8 __asm__("x8") = SYS_CLONE;       \
    register long x0 __asm__("x0") = THREAD_FLAGS;    \
    register long x1 __asm__("x1") = (long)stack_top; \
    register long x2 __asm__("x2") = 0; /* ptid */    \
    register long x3 __asm__("x3") = 0; /* ctid */    \
    register long x4 __asm__("x4") = 0; /* tls  */    \
                                                      \
    __asm__ volatile(                                 \
        "svc #0"                                      \
        : "+r"(x0)                                    \
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4) \
        : "memory", "cc");                            \
                                                      \
    if (x0 == 0)                                      \
    {                                                 \
        fn();                                         \
        sys_exit();                                   \
    }                                                 \
    if (x0 < 0) /* failure */                         \
    {                                                 \
        break;                                        \
    }

#define resume_main_thread()                                                                \
    do                                                                                      \
    {                                                                                       \
        register long x8 __asm__("x8") = SYS_FUTEX;                                         \
        register long x0 __asm__("x0") = (long)&queue_head;                                 \
        register long x1 __asm__("x1") = FUTEX_WAKE | FUTEX_PRIVATE_FLAG; /* FUTEX_WAKE  */ \
        register long x2 __asm__("x2") = 1;                               /* val: wake 1 */ \
        register long x3 __asm__("x3") = 0;                               /* timeout     */ \
        register long x4 __asm__("x4") = 0;                               /* uaddr2      */ \
        register long x5 __asm__("x5") = 0;                               /* val3        */ \
                                                                                            \
        __asm__ volatile(                                                                   \
            "svc #0"                                                                        \
            : "+r"(x0)                                                                      \
            : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)                          \
            : "memory", "cc");                                                              \
    } while (0);

#define wait_for_queue()                                                                           \
    do                                                                                             \
    {                                                                                              \
        int cur_val = atomic_load(&queue_head);                                                    \
        while (cur_val < queue_size + thread_num)                                                  \
        {                                                                                          \
            register long x8 __asm__("x8") = SYS_FUTEX;                                            \
            register long x0 __asm__("x0") = (long)&queue_head;                                    \
            register long x1 __asm__("x1") = FUTEX_WAIT | FUTEX_PRIVATE_FLAG; /* FUTEX_WAIT     */ \
            register long x2 __asm__("x2") = (long)cur_val;                   /* expected value */ \
            register long x3 __asm__("x3") = 0;                               /* timeout        */ \
            register long x4 __asm__("x4") = 0;                               /* uaddr2         */ \
            register long x5 __asm__("x5") = 0;                               /* val3           */ \
                                                                                                   \
            __asm__ volatile(                                                                      \
                "svc #0"                                                                           \
                : "+r"(x0)                                                                         \
                : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)                             \
                : "memory", "cc");                                                                 \
                                                                                                   \
            cur_val = atomic_load(&queue_head);                                                    \
        }                                                                                          \
    } while (0);

#elif defined(__arm__)

#define SYS_CLONE 120
#define SYS_EXIT 1
#define SYS_FUTEX 240

static inline void sys_exit(void)
{
    register long r7 __asm__("r7") = SYS_EXIT;
    register long r0 __asm__("r0") = 0;
    __asm__ volatile(
        "swi #0" ::"r"(r7), "r"(r0)
        : "memory");
    __builtin_unreachable();
}

#define thread_create(stack_top, fn)                  \
    register long r7 __asm__("r7") = SYS_CLONE;       \
    register long r0 __asm__("r0") = THREAD_FLAGS;    \
    register long r1 __asm__("r1") = (long)stack_top; \
    register long r2 __asm__("r2") = 0; /* ptid */    \
    register long r3 __asm__("r3") = 0; /* ctid */    \
    register long r4 __asm__("r4") = 0; /* tls  */    \
                                                      \
    __asm__ volatile(                                 \
        "swi #0"                                      \
        : "+r"(r0)                                    \
        : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4) \
        : "memory", "cc");                            \
                                                      \
    if (r0 == 0)                                      \
    {                                                 \
        fn();                                         \
        sys_exit();                                   \
    }                                                 \
    if (r0 < 0) /* failure */                         \
    {                                                 \
        break;                                        \
    }

#define resume_main_thread()                                                                \
    do                                                                                      \
    {                                                                                       \
        register long r7 __asm__("r7") = SYS_FUTEX;                                         \
        register long r0 __asm__("r0") = (long)&queue_head;                                 \
        register long r1 __asm__("r1") = FUTEX_WAKE | FUTEX_PRIVATE_FLAG; /* FUTEX_WAKE  */ \
        register long r2 __asm__("r2") = 1;                               /* val: wake 1 */ \
        register long r3 __asm__("r3") = 0;                               /* timeout     */ \
        register long r4 __asm__("r4") = 0;                               /* uaddr2      */ \
        register long r5 __asm__("r5") = 0;                               /* val3        */ \
                                                                                            \
        __asm__ volatile(                                                                   \
            "swi #0"                                                                        \
            : "+r"(r0)                                                                      \
            : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)                          \
            : "memory", "cc");                                                              \
    } while (0);

#define wait_for_queue()                                                                           \
    do                                                                                             \
    {                                                                                              \
        int cur_val = atomic_load(&queue_head);                                                    \
        while (cur_val < queue_size + thread_num)                                                  \
        {                                                                                          \
            register long r7 __asm__("r7") = SYS_FUTEX;                                            \
            register long r0 __asm__("r0") = (long)&queue_head;                                    \
            register long r1 __asm__("r1") = FUTEX_WAIT | FUTEX_PRIVATE_FLAG; /* FUTEX_WAIT     */ \
            register long r2 __asm__("r2") = (long)cur_val;                   /* expected value */ \
            register long r3 __asm__("r3") = 0;                               /* timeout        */ \
            register long r4 __asm__("r4") = 0;                               /* uaddr2         */ \
            register long r5 __asm__("r5") = 0;                               /* val3           */ \
                                                                                                   \
            __asm__ volatile(                                                                      \
                "swi #0"                                                                           \
                : "+r"(r0)                                                                         \
                : "r"(r7), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5)                             \
                : "memory", "cc");                                                                 \
                                                                                                   \
            cur_val = atomic_load(&queue_head);                                                    \
        }                                                                                          \
    } while (0);

#else
#error "Unsupported platform"
#endif

#elif defined(_WIN32) || defined(_WIN64)

#define thread_create(stack_top, fn)                                             \
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fn, NULL, 0, NULL); \
    if (h)                                                                       \
        CloseHandle(h);                                                          \
    else /* failure */                                                           \
        break;

#define resume_main_thread()    \
    do                          \
    {                           \
        SetEvent(g_wake_event); \
    } while (0)

#define wait_for_queue()                                             \
    do                                                               \
    {                                                                \
        while (atomic_load(&queue_head) < queue_size + thread_num)   \
        {                                                            \
            ResetEvent(g_wake_event);                                \
            if (atomic_load(&queue_head) >= queue_size + thread_num) \
                break;                                               \
            WaitForSingleObject(g_wake_event, INFINITE);             \
        }                                                            \
    } while (0)

static inline int get_thread_count()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
}

#else
#error "Unsupported platform"
#endif
