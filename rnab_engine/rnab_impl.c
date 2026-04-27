#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include "../third_party/rapidhash.h"
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#define RNAB_MT 1
// #define RNAB_REVCACHE 1
// #define TRACK_CUTOFF_STATS 1

#include <stdarg.h>
#include <stddef.h>

#ifndef STACK_SIZE
#define STACK_SIZE (128 * 1024)
#endif

#ifndef MAX_THREADS
#define MAX_THREADS 128
#endif

#ifndef FUNC_ALIGN
#define FUNC_ALIGN 256
#endif

#include "syscalls.c"

#ifdef RNAB_MT
#include <stdatomic.h>
#endif

#ifndef _printf

#define STB_SPRINTF_MIN 512
#include "../third_party/stb_sprintf.c"

#endif

#ifdef RNAB_DEBUG
#define debug_printf(...) _printf(__VA_ARGS__)
#define debug_get_time(time_var) get_time(time_var)
#else
#define debug_printf(...) ((void)0)
#define debug_get_time(time_var) ((void)0)
#endif

#ifndef RNAB_HOT_FUNCTION
#define RNAB_HOT_FUNCTION
#endif

#ifdef __AVX2__
#include <immintrin.h>
#endif

/*
 * Firewall              :         60 combinations
 * Boosted Card Movement :         12 combinations
 * Card Movement         : 7 * 4 = 28 combinations
 * Swap                  : 4 * 4 = 16 combinations
 *
 * Total                 :         116 moves
 */

#define MAX_MOVES 128 // 12 extra ones just in case

#define MIN_MT_DEPTH 8

#define MIN -2047
#define MAX 2047

#define MIN_CACHE_DEPTH 1

#define ITERATION_CURRENT_IS_FIRST_UNKNOWN 0
#define ITERATION_CURRENT_IS_FIRST_LINK 1
#define ITERATION_CURRENT_IS_FIRST_VIRUS 2
#define ITERATION_CURRENT_IS_SECOND_UNKNOWN 3
#define ITERATION_CURRENT_IS_SECOND_LINK 4
#define ITERATION_CURRENT_IS_SECOND_VIRUS 5

#define TT_EXACT 0
#define TT_LOWERBOUND 1
#define TT_UPPERBOUND 2

#define CAN_MOVE_DOUBLE_RIGHT 18229723555195321596ULL
#define CAN_MOVE_RIGHT 18374403900871474942ULL
#define CAN_MOVE_LEFT 9187201950435737471ULL
#define CAN_MOVE_DOUBLE_LEFT 4557430888798830399ULL

#define RESET "\033[0m"
#define FG_RED "\033[31m"
#define FG_GREEN "\033[32m"
#define FG_BLUE "\033[34m"
#define FG_WHITE "\033[37m"
#define BG_RED "\033[41m"
#define BG_GREEN "\033[42m"

#if defined(__clang__)
#define RNAB_ASSUME(expr) __builtin_assume(!!(expr))
#elif defined(__GNUC__) || defined(__GNUG__)
#define RNAB_ASSUME(expr)            \
    do                               \
    {                                \
        if (!(expr))                 \
            __builtin_unreachable(); \
    } while (0)
#elif defined(_MSC_VER)
#define RNAB_ASSUME(expr) __assume(!!(expr))
#else
#define RNAB_ASSUME(expr)       \
    do                          \
    {                           \
        (void)sizeof(!!(expr)); \
    } while (0)
#endif

typedef union
{
    uint64_t raw;
    struct
    {
        uint8_t firewall_fir;
        uint8_t firewall_sec;

        uint8_t fir_link;
        uint8_t sec_link;
        uint8_t fir_virus;
        uint8_t sec_virus;

        bool is_swap_available_fir;
        bool is_swap_available_sec;
    } fields;
} extra_args_t;

typedef struct
{
    uint64_t is_fir_mask;
    uint64_t is_sec_mask;
    uint64_t is_link_mask;
    uint64_t is_boosted_mask;

    extra_args_t args;
} field_t;

typedef struct
{
    uint64_t depth_preferred;
    uint64_t scratch;
} tt_bucket_t;

typedef struct
{
    field_t best_field;
    int32_t evaluation;
} minimax_main_result_t;

#ifdef BRANCH_DEBUG
typedef struct
{
    int64_t total_entries;
    int64_t cutoff_entries;
    int64_t improved_score;
    int64_t recursion_cost;
    const char *msg;
    int32_t temp_score;
    int32_t pad;
} cutoff_tracker_t;
#endif

#ifndef TABLE_SIZE // should never be undefined, supress warnings
#define TABLE_SIZE 1024
#endif

#define ENTRY_HASH_SIZE 41
#define ENTRY_HASH_MASK ((1ULL << (ENTRY_HASH_SIZE)) - 1)
#define TABLE_SIZE_BITS __builtin_ctzll(TABLE_SIZE)

// we need just 23 bits of tt to fully verify the initial hash

#if defined(__BMI__)
#define clear_lowest_set_bit(x, var) (x) = _blsr_u64(x)
#define clear_highest_set_bit(x, var) ((x) ^= (var))
#else
#define clear_lowest_set_bit(x, var) ((x) &= (x - 1))
#define clear_highest_set_bit(x, var) ((x) ^= (var))
#endif

#define extract_lsb(x) (x & -x)

static const int32_t indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

static uint64_t rec_counter = 0;

STATIC_BSS tt_bucket_t table[TABLE_SIZE * 2] __attribute__((aligned(4096)));

static tt_bucket_t *tt_fir = table;
static tt_bucket_t *tt_sec = table + TABLE_SIZE;

#ifdef BRANCH_DEBUG

#define MAX_BRANCHES 1000

static cutoff_tracker_t cutoff_tracker[MAX_BRANCHES] = {0};

static void clear_branch_tracker()
{
    for (int32_t i = 0; i < MAX_BRANCHES; ++i)
    {
        cutoff_tracker[i].total_entries = 0;
        cutoff_tracker[i].cutoff_entries = 0;
        cutoff_tracker[i].improved_score = 0;
        cutoff_tracker[i].recursion_cost = 0;
    }
}
#endif

static uint64_t reverse_mask(uint64_t mask)
{
    uint64_t res = 0;

    while (mask)
    {
        const int cur_pos = __builtin_clzll(mask);
        const uint64_t cur_mask = 1ULL << (63 - cur_pos);

        res |= (1ULL << cur_pos);

        mask ^= cur_mask;
    }

    return res;
}

static inline __attribute__((always_inline)) void init_field(field_t *__restrict__ f, uint8_t pos_fir, uint8_t pos_sec)
{
    f->is_sec_mask = 0;
    f->is_link_mask = 0;
    f->is_fir_mask = 0;
    f->is_boosted_mask = 0;

    f->args.fields.fir_link = 0;
    f->args.fields.fir_virus = 0;
    f->args.fields.sec_link = 0;
    f->args.fields.sec_virus = 0;

    f->args.fields.firewall_fir = 0;
    f->args.fields.firewall_sec = 0;

    f->args.fields.is_swap_available_fir = true;
    f->args.fields.is_swap_available_sec = true;

    static const int init_pos_fir[8] = {63, 62, 61, 52, 51, 58, 57, 56};
    static const int init_pos_sec[8] = {0, 1, 2, 11, 12, 5, 6, 7};

    for (int i = 0; i < 8; ++i)
    {
        f->is_sec_mask |= (1ULL << init_pos_sec[i]);
        f->is_fir_mask |= (1ULL << init_pos_fir[i]);
        f->is_link_mask |= ((uint64_t)(((~pos_fir) >> i) & 1) << init_pos_fir[i]) | ((uint64_t)(((~pos_sec) >> i) & 1) << init_pos_sec[i]);
    }
}

static void print_field(const field_t *__restrict__ f)
{
    _printf("Virus: %d         Link: %d\n", f->args.fields.fir_virus, f->args.fields.fir_link);
    _printf("   " BG_GREEN "[C]" RESET);
    if (f->args.fields.is_swap_available_fir)
        _printf(BG_GREEN "[S]" RESET "      ");
    else
        _printf(BG_RED "[S]" RESET "      ");
    if (f->args.fields.firewall_fir == 0)
        _printf(BG_GREEN "[F]" RESET);
    else
        _printf(BG_RED "[F]" RESET);
    if (f->is_boosted_mask & f->is_fir_mask)
        _printf(BG_RED "[B]" RESET "\n");
    else
        _printf(BG_GREEN "[B]" RESET "\n");
    for (int i = 63; i >= 0; --i)
    {
        if (f->args.fields.firewall_fir == ((i << 1) | 1))
            _printf(BG_GREEN);
        if (f->args.fields.firewall_sec == ((i << 1) | 1))
            _printf(BG_RED);
        if (((f->is_fir_mask & f->is_link_mask) >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                _printf("" FG_GREEN "[" FG_BLUE "L" FG_GREEN "]");
            else
                _printf("[" FG_GREEN "L" FG_WHITE "]");
        }
        else if ((f->is_fir_mask >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                _printf("" FG_GREEN "[" FG_BLUE "V" FG_GREEN "]");
            else
                _printf("[" FG_GREEN "V" FG_WHITE "]");
        }
        else if (((f->is_sec_mask & f->is_link_mask) >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                _printf(FG_RED "[" FG_BLUE "L" FG_RED "]");
            else
                _printf("[" FG_RED "L" FG_WHITE "]");
        }
        else if ((f->is_sec_mask >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                _printf(FG_RED "[" FG_BLUE "V" FG_RED "]");
            else
                _printf("[" FG_RED "V" FG_WHITE "]");
        }
        else
            _printf("[ ]");

        if (i % 8 == 0)
            _printf("\n");
        _printf(RESET);
    }
    _printf("   " BG_GREEN "[C]" RESET);
    if (f->args.fields.is_swap_available_sec)
        _printf(BG_GREEN "[S]" RESET "      ");
    else
        _printf(BG_RED "[S]" RESET "      ");
    if (f->args.fields.firewall_sec == 0)
        _printf(BG_GREEN "[F]" RESET);
    else
        _printf(BG_RED "[F]" RESET);
    if (f->is_boosted_mask & f->is_sec_mask)
        _printf(BG_RED "[B]" RESET "\n");
    else
        _printf(BG_GREEN "[B]" RESET "\n");

    _printf("Virus: %d         Link: %d\n", f->args.fields.sec_virus, f->args.fields.sec_link);
}

static int32_t __attribute__((noinline)) field_evaluate_slow(const field_t *__restrict__ f)
{
    int32_t forward_adv_fir = 0;
    int32_t forward_adv_sec = 0;

    uint64_t fir_mask = f->is_fir_mask;
    uint64_t sec_mask = f->is_sec_mask;

    while (fir_mask)
    {
        const int pos = __builtin_ctzll(fir_mask);
        uint64_t bit = 1ULL << pos;

        forward_adv_fir += 7 - (pos >> 3);

        clear_lowest_set_bit(fir_mask, bit);
    }

    while (sec_mask)
    {
        const int pos = __builtin_ctzll(sec_mask);
        uint64_t bit = 1ULL << pos;

        forward_adv_sec += (pos >> 3);

        clear_lowest_set_bit(sec_mask, bit);
    }

    /*

    Max Evaluation:
        (128 << 3) - (64 << 0) - (128 << 0) + (64 << 3) = 1344
        (7 * 8) - (0)                                   = 56
        256 - 0 + 8 - 0                                 = 264
        Sum                                             = 1664

        We squeeze everything in 12 bits: [-2047 ~ 2047], leaving almost 400 slots for the terminal states per player
    */

    return (128 << f->args.fields.fir_link) - (64 << f->args.fields.fir_virus) - (128 << f->args.fields.sec_link) + (64 << f->args.fields.sec_virus) + forward_adv_fir - forward_adv_sec + 256 * (int32_t)f->args.fields.is_swap_available_fir - 256 * (int32_t)f->args.fields.is_swap_available_sec - 8 * (f->args.fields.firewall_fir & 1) + 8 * (f->args.fields.firewall_sec & 1);
}

// don't forget to change these

#define SCORE_REGULAR_MAX 1664
#define SCORE_TERMINAL_BASE (SCORE_REGULAR_MAX + 1)

static field_t reverse_field(const field_t *__restrict__ f) // should only be used for debugging
{
    field_t new_field;

    new_field.is_fir_mask = reverse_mask(f->is_sec_mask);
    new_field.is_sec_mask = reverse_mask(f->is_fir_mask);
    new_field.is_link_mask = reverse_mask(f->is_link_mask);
    new_field.is_boosted_mask = reverse_mask(f->is_boosted_mask);

    new_field.args.fields.firewall_fir = (f->args.fields.firewall_sec) ? ((126 - (f->args.fields.firewall_sec & 126)) | (f->args.fields.firewall_sec & 1)) : 0;
    new_field.args.fields.firewall_sec = (f->args.fields.firewall_fir) ? ((126 - (f->args.fields.firewall_fir & 126)) | (f->args.fields.firewall_fir & 1)) : 0;

    new_field.args.fields.fir_link = f->args.fields.sec_link;
    new_field.args.fields.fir_virus = f->args.fields.sec_virus;

    new_field.args.fields.sec_link = f->args.fields.fir_link;
    new_field.args.fields.sec_virus = f->args.fields.fir_virus;

    new_field.args.fields.is_swap_available_fir = f->args.fields.is_swap_available_sec;
    new_field.args.fields.is_swap_available_sec = f->args.fields.is_swap_available_fir;

    return new_field;
}

static void print_tt_stats()
{
    static uint64_t perf_array[16];
    uint64_t total_entries = 0;

    for (int i = 0; i < 16; ++i)
        perf_array[i] = 0;
    for (int i = 0; i < TABLE_SIZE * 2; ++i)
    {
        do
        {
            uint64_t entry = table[i].depth_preferred;

            const uint32_t entry_depth = (entry >> (ENTRY_HASH_SIZE + 2)) & 31;
            const uint32_t entry_best_section = (entry >> (ENTRY_HASH_SIZE + 2 + 5)) & 15;

            if (entry_depth != 0)
                ++total_entries;

            if (entry_depth != 0 && entry_best_section != 15u)
                perf_array[entry_best_section]++;
        } while (0);

        do
        {
            uint64_t entry = table[i].scratch;

            const uint32_t entry_depth = (entry >> (ENTRY_HASH_SIZE + 2)) & 31;
            const uint32_t entry_best_section = (entry >> (ENTRY_HASH_SIZE + 2 + 5)) & 15;

            if (entry_depth != 0)
                ++total_entries;

            if (entry_depth != 0 && entry_best_section != 15u)
                perf_array[entry_best_section]++;
        } while (0);
    }

    _printf("Total entries=%llu / %llu\nbest sections: \n", total_entries, TABLE_SIZE * 4);
    for (int i = 0; i < 16; ++i)
    {
        _printf("%d -> %llu\n", i, perf_array[i]);
    }
    _printf("\n");
}

#define PERFORM_ITERATION_FIR(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                                                                                                                                                           \
    if (is_sec_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                 \
    {                                                                                                                                                                                                                                                                                                   \
        if (sec_link_mask & new_pos_bitboard)                                                                                                                                                                                                                                                           \
        {                                                                                                                                                                                                                                                                                               \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                              \
            WRITE_MOVE(is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask ^ new_pos_bitboard,                                                                                                                                                                                             \
                       ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ cur_pos_bitboard) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask ^ new_pos_bitboard) : ((is_link_mask ^ new_pos_bitboard ^ unknown_mask) | (unknown_mask shift_func shift_count)))), \
                       ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                              \
                       (const extra_args_t){.raw = args.raw + 65536ULL});                                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                                                                               \
        else if (args.fields.fir_virus < 3)                                                                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                                                                                               \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                              \
            WRITE_MOVE(is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask ^ new_pos_bitboard,                                                                                                                                                                                             \
                       ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                  \
                       ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                              \
                       (const extra_args_t){.raw = args.raw + 4294967296ULL});                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                                                                               \
    }                                                                                                                                                                                                                                                                                                   \
    else if ((is_fir_mask & new_pos_bitboard) == 0)                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                   \
        const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                  \
        WRITE_MOVE(is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask,                                                                                                                                                                                                                    \
                   ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                      \
                   ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                      \
                   args);                                                                                                                                                                                                                                                                               \
    }

#define PERFORM_ITERATION_SEC(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                                                                                                                                                             \
    if (is_fir_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                   \
    {                                                                                                                                                                                                                                                                                                     \
        if (fir_link_mask & new_pos_bitboard)                                                                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                                                                                                 \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                \
            WRITE_MOVE(is_fir_mask ^ new_pos_bitboard, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                               \
                       ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ cur_pos_bitboard) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask ^ new_pos_bitboard) : ((is_link_mask ^ new_pos_bitboard ^ unknown_mask) | (unknown_mask shift_func shift_count)))), \
                       ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                \
                       (const extra_args_t){.raw = args.raw + 16777216ULL});                                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                                                                                                 \
        else if (args.fields.sec_virus < 3)                                                                                                                                                                                                                                                               \
        {                                                                                                                                                                                                                                                                                                 \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                \
            WRITE_MOVE(is_fir_mask ^ new_pos_bitboard, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                               \
                       ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                  \
                       ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                \
                       (const extra_args_t){.raw = args.raw + 1099511627776ULL});                                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                                                                                                 \
    }                                                                                                                                                                                                                                                                                                     \
    else if ((is_sec_mask & new_pos_bitboard) == 0)                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                     \
        const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                    \
        WRITE_MOVE(is_fir_mask, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                                                      \
                   ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                      \
                   ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                        \
                   args);                                                                                                                                                                                                                                                                                 \
    }

#define WRITE_MOVE(fm, sm, lm, bm, as)                                  \
    do                                                                  \
    {                                                                   \
        *out_field_ptr = (const field_t){(fm), (sm), (lm), (bm), (as)}; \
        ++out_field_ptr;                                                \
    } while (0)

#if defined(__clang__)
static __attribute__((cold)) void possible_moves_max(const uint64_t is_fir_mask, const uint64_t is_sec_mask, const uint64_t is_link_mask, const uint64_t is_boosted_mask,
                                                     const extra_args_t args, const field_t *__restrict__ possible_moves_buf, int32_t *__restrict__ possible_moves_buf_moves_count)
#elif defined(__GNUC__)
static __attribute__((optimize("Os"), cold)) void possible_moves_max(const uint64_t is_fir_mask, const uint64_t is_sec_mask, const uint64_t is_link_mask, const uint64_t is_boosted_mask,
                                                                     const extra_args_t args, const field_t *__restrict__ possible_moves_buf, int32_t *__restrict__ possible_moves_buf_moves_count)
#else
static void possible_moves_max(const uint64_t is_fir_mask, const uint64_t is_sec_mask, const uint64_t is_link_mask, const uint64_t is_boosted_mask,
                               const extra_args_t args, const field_t *__restrict__ possible_moves_buf, int32_t *__restrict__ possible_moves_buf_moves_count)
#endif
{
    field_t *__restrict__ out_field_ptr = (field_t *__restrict__)possible_moves_buf;

    uint64_t m;
    uint64_t fir_link_m[4];
    uint64_t fir_virus_m[4];
    const uint64_t fir_link_mask = is_link_mask & is_fir_mask;
    const uint64_t fir_virus_mask = is_fir_mask ^ fir_link_mask;
    const uint64_t sec_link_mask = is_link_mask ^ fir_link_mask;
    const uint64_t cur_boosted_mask = is_boosted_mask & is_fir_mask;
    const uint64_t boosted_link = cur_boosted_mask & fir_link_mask;
    const uint64_t enemy_firewall_mask = (uint64_t)(args.fields.firewall_sec & 1) << (args.fields.firewall_sec >> 1);
    const uint64_t free_mask = ~(is_fir_mask | is_sec_mask | enemy_firewall_mask);

    // Lets be nice and use less ctz/clz calls when only bitboard is desired
    m = fir_link_mask & (~cur_boosted_mask);
    fir_link_m[0] = (m) & (-m);
    m &= (m - 1);
    fir_link_m[1] = (m) & (-m);
    m &= (m - 1);
    fir_link_m[2] = (m) & (-m);
    m &= (m - 1);
    fir_link_m[3] = m;

    m = fir_virus_mask & (~cur_boosted_mask);
    fir_virus_m[0] = (m) & (-m);
    m &= (m - 1);
    fir_virus_m[1] = (m) & (-m);
    m &= (m - 1);
    fir_virus_m[2] = (m) & (-m);
    m &= (m - 1);
    fir_virus_m[3] = m;

    if (__builtin_expect((fir_link_mask & 8ULL) != 0, 0))
    {
        WRITE_MOVE(is_fir_mask ^ 8ULL, is_sec_mask, is_link_mask ^ 8ULL, is_boosted_mask & ~8ULL, (const extra_args_t){.raw = args.raw + 65536ULL});
    }

    if (__builtin_expect((fir_link_mask & 16ULL) != 0, 0))
    {
        WRITE_MOVE(is_fir_mask ^ 16ULL, is_sec_mask, is_link_mask ^ 16ULL, is_boosted_mask & ~16ULL, (const extra_args_t){.raw = args.raw + 65536ULL});
    }

    if (__builtin_expect((((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 24ULL) != 0, 0))
    {
        WRITE_MOVE(is_fir_mask ^ cur_boosted_mask, is_sec_mask, is_link_mask ^ cur_boosted_mask, is_boosted_mask ^ cur_boosted_mask, (const extra_args_t){.raw = args.raw + 65536ULL});
    }

    if (cur_boosted_mask)
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const uint64_t links_masked_out = sec_link_mask & ~enemy_firewall_mask;

        if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }
    }

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = fir_virus_mask;

        while (temp)
        {
            const uint64_t pos = extract_lsb(temp); // back -> front

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);

            clear_lowest_set_bit(temp, pos);
        }
    }
    else
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const uint64_t links_masked_out = sec_link_mask & ~enemy_firewall_mask;
        const uint64_t legal_mask = (~enemy_firewall_mask) & (~links_masked_out) & (~is_fir_mask);

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = fir_virus_m[i];
        const uint64_t legal_mask = (is_sec_mask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = fir_link_m[i];
        const uint64_t legal_mask = (is_sec_mask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = fir_link_m[i];

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = fir_virus_m[i];

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }
    }

    if (args.fields.firewall_fir == 0)
    {
        uint64_t temp = fir_link_mask & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = __builtin_ctzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});

            clear_lowest_set_bit(temp, pos);
        }

        const uint64_t enemy_boosted_mask = is_sec_mask & is_boosted_mask;

        temp = ((fir_virus_mask & cur_boosted_mask) | (((cur_boosted_mask >> 8) | (cur_boosted_mask >> 16) | ((cur_boosted_mask & CAN_MOVE_LEFT) >> 9) | ((cur_boosted_mask & CAN_MOVE_RIGHT) >> 7) | (enemy_boosted_mask << 8)) & free_mask)) & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = __builtin_ctzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});

            clear_lowest_set_bit(temp, pos);
        }
    }
    else
    {
        WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, (const extra_args_t){.raw = args.raw & 18446744073709551360ULL});
    }

    if (args.fields.is_swap_available_fir)
    {
        uint64_t link_mask = fir_link_mask;

        while (link_mask)
        {
            const uint64_t link_pos = extract_lsb(link_mask); // front -> back

            uint64_t virus_mask = fir_virus_mask;

            while (virus_mask)
            {
                const uint64_t virus_pos = extract_lsb(virus_mask); // front -> back

                WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask, (const extra_args_t){.raw = args.raw ^ 281474976710656ULL});

                clear_lowest_set_bit(virus_mask, virus_pos);
            }

            clear_lowest_set_bit(link_mask, link_pos);
        }
    }

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = fir_link_mask;

        while (temp)
        {
            const uint64_t pos = extract_lsb(temp); // back -> front

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);

            clear_lowest_set_bit(temp, pos);
        }
    }
    else
    {
        WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
    }

    RNAB_ASSUME((int)(((uintptr_t)out_field_ptr - (uintptr_t)possible_moves_buf) / 40) <= 128);

    *possible_moves_buf_moves_count = (int)(((uintptr_t)out_field_ptr - (uintptr_t)possible_moves_buf) / 40);
}

#if defined(__clang__)
static __attribute__((cold)) void possible_moves_min(const uint64_t is_fir_mask, const uint64_t is_sec_mask, const uint64_t is_link_mask, const uint64_t is_boosted_mask,
                                                     const extra_args_t args, const field_t *__restrict__ possible_moves_buf, int32_t *__restrict__ possible_moves_buf_moves_count)
#elif defined(__GNUC__)
static __attribute__((optimize("Os"), cold)) void possible_moves_min(const uint64_t is_fir_mask, const uint64_t is_sec_mask, const uint64_t is_link_mask, const uint64_t is_boosted_mask,
                                                                     const extra_args_t args, const field_t *__restrict__ possible_moves_buf, int32_t *__restrict__ possible_moves_buf_moves_count)
#else
static void possible_moves_min(const uint64_t is_fir_mask, const uint64_t is_sec_mask, const uint64_t is_link_mask, const uint64_t is_boosted_mask,
                               const extra_args_t args, const field_t *__restrict__ possible_moves_buf, int32_t *__restrict__ possible_moves_buf_moves_count)
#endif
{
    field_t *__restrict__ out_field_ptr = (field_t *__restrict__)possible_moves_buf;

    uint64_t m;
    uint64_t sec_link_m[4];
    uint64_t sec_virus_m[4];
    const uint64_t fir_link_mask = is_link_mask & is_fir_mask;
    const uint64_t sec_link_mask = is_link_mask ^ fir_link_mask;
    const uint64_t sec_virus_mask = is_sec_mask ^ sec_link_mask;
    const uint64_t cur_boosted_mask = is_boosted_mask & is_sec_mask;
    const uint64_t boosted_link = cur_boosted_mask & sec_link_mask;
    const uint64_t enemy_firewall_mask = (uint64_t)(args.fields.firewall_fir & 1) << (args.fields.firewall_fir >> 1);
    const uint64_t free_mask = ~(is_fir_mask | is_sec_mask | enemy_firewall_mask);

    // Lets be nice and use less ctz/clz calls when only bitboard is desired
    m = sec_link_mask & (~cur_boosted_mask);
    sec_link_m[3] = (m) & (-m);
    m &= (m - 1);
    sec_link_m[2] = (m) & (-m);
    m &= (m - 1);
    sec_link_m[1] = (m) & (-m);
    m &= (m - 1);
    sec_link_m[0] = m;

    m = sec_virus_mask & (~cur_boosted_mask);
    sec_virus_m[3] = (m) & (-m);
    m &= (m - 1);
    sec_virus_m[2] = (m) & (-m);
    m &= (m - 1);
    sec_virus_m[1] = (m) & (-m);
    m &= (m - 1);
    sec_virus_m[0] = m;

    if (__builtin_expect((sec_link_mask & 1152921504606846976ULL) != 0, 0))
    {
        WRITE_MOVE(is_fir_mask, is_sec_mask ^ 1152921504606846976ULL, is_link_mask ^ 1152921504606846976ULL, is_boosted_mask & ~1152921504606846976ULL, (const extra_args_t){.raw = args.raw + 16777216ULL});
    }

    if (__builtin_expect((sec_link_mask & 576460752303423488ULL) != 0, 0))
    {
        WRITE_MOVE(is_fir_mask, is_sec_mask ^ 576460752303423488ULL, is_link_mask ^ 576460752303423488ULL, is_boosted_mask & ~576460752303423488ULL, (const extra_args_t){.raw = args.raw + 16777216ULL});
    }

    if (__builtin_expect((((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 1729382256910270464ULL) != 0, 0))
    {
        WRITE_MOVE(is_fir_mask, is_sec_mask ^ cur_boosted_mask, is_link_mask ^ cur_boosted_mask, is_boosted_mask ^ cur_boosted_mask, (const extra_args_t){.raw = args.raw + 16777216ULL});
    }

    if (cur_boosted_mask)
    {
        const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask;
        const uint64_t cur_pos_bitboard = cur_boosted_mask;

        if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }
    }

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = sec_virus_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);

            clear_highest_set_bit(temp, pos);
        }
    }
    else
    {
        const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask;
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const uint64_t legal_mask = (~enemy_firewall_mask) & (~links_masked_out) & (~is_sec_mask);

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = sec_virus_m[i];
        const uint64_t legal_mask = (is_fir_mask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = sec_link_m[i];
        const uint64_t legal_mask = (is_fir_mask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = sec_link_m[i];

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        const uint64_t cur_pos_bitboard = sec_virus_m[i];

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }
    }

    if (args.fields.firewall_sec == 0)
    {
        uint64_t temp = sec_link_mask & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = 63 - __builtin_clzll(temp);
            const uint64_t pos = (1ULL << bit_pos);

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, (const extra_args_t){.raw = args.raw | 256 | (bit_pos << 9)});

            clear_highest_set_bit(temp, pos);
        }

        const uint64_t enemy_boosted_mask = is_fir_mask & is_boosted_mask;

        temp = ((sec_virus_mask & cur_boosted_mask) | (((cur_boosted_mask << 8) | (cur_boosted_mask << 16) | ((cur_boosted_mask & CAN_MOVE_LEFT) << 7) | ((cur_boosted_mask & CAN_MOVE_RIGHT) << 9) | (enemy_boosted_mask >> 8)) & free_mask)) & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = 63 - __builtin_clzll(temp);
            const uint64_t pos = (1ULL << bit_pos);

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, (const extra_args_t){.raw = args.raw | 256 | (bit_pos << 9)});

            clear_highest_set_bit(temp, pos);
        }
    }
    else
    {
        WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, (const extra_args_t){.raw = args.raw & 18446744073709486335ULL});
    }

    if (args.fields.is_swap_available_sec)
    {
        uint64_t link_mask = sec_link_mask;

        while (link_mask)
        {
            const uint64_t link_pos = (1ULL << (63 - __builtin_clzll(link_mask))); // front -> back

            uint64_t virus_mask = sec_virus_mask;

            while (virus_mask)
            {
                const uint64_t virus_pos = (1ULL << (63 - __builtin_clzll(virus_mask))); // front -> back

                WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask, (const extra_args_t){.raw = args.raw ^ 72057594037927936ULL});

                clear_highest_set_bit(virus_mask, virus_pos);
            }

            clear_highest_set_bit(link_mask, link_pos);
        }
    }

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = sec_link_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);

            clear_highest_set_bit(temp, pos);
        }
    }
    else
    {
        WRITE_MOVE(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
    }

    RNAB_ASSUME((int)(((uintptr_t)out_field_ptr - (uintptr_t)possible_moves_buf) / 40) <= 128);

    *possible_moves_buf_moves_count = (int)(((uintptr_t)out_field_ptr - (uintptr_t)possible_moves_buf) / 40);
}

#undef WRITE_MOVE

#ifdef BRANCH_DEBUG

#define BEGIN_BRANCH_TRACKING() \
    static const int32_t _branch_counter_base = __COUNTER__

#define BRANCH_ENTER_MAX(MSG)                                                             \
    static const int32_t _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    const int64_t cur_rec_count_##__LINE__ = rec_counter;                                 \
    cutoff_tracker[_branch_idx_##__LINE__].total_entries++;                               \
    cutoff_tracker[_branch_idx_##__LINE__].msg = MSG;                                     \
    cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries++;                              \
    cutoff_tracker[_branch_idx_##__LINE__].temp_score = alpha

#define BRANCH_ENTER_MIN(MSG)                                                             \
    static const int32_t _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    const int64_t cur_rec_count_##__LINE__ = rec_counter;                                 \
    cutoff_tracker[_branch_idx_##__LINE__].total_entries++;                               \
    cutoff_tracker[_branch_idx_##__LINE__].msg = MSG;                                     \
    cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries++;                              \
    cutoff_tracker[_branch_idx_##__LINE__].temp_score = beta

#define BRANCH_EXIT_MAX()                                                                            \
    cutoff_tracker[_branch_idx_##__LINE__].recursion_cost += rec_counter - cur_rec_count_##__LINE__; \
    if (beta > reschild)                                                                             \
        cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries--;                                     \
    if (reschild > cutoff_tracker[_branch_idx_##__LINE__].temp_score)                                \
    cutoff_tracker[_branch_idx_##__LINE__].improved_score++

#define BRANCH_EXIT_MIN()                                                                            \
    cutoff_tracker[_branch_idx_##__LINE__].recursion_cost += rec_counter - cur_rec_count_##__LINE__; \
    if (reschild > alpha)                                                                            \
        cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries--;                                     \
    if (reschild < cutoff_tracker[_branch_idx_##__LINE__].temp_score)                                \
    cutoff_tracker[_branch_idx_##__LINE__].improved_score++

#define END_BRANCH_TRACKING() \
    const int32_t _total_branch_count = (__COUNTER__ - _branch_counter_base - 1)

#else

#define BEGIN_BRANCH_TRACKING()
#define BRANCH_ENTER_MAX(MSG)
#define BRANCH_ENTER_MIN(MSG)
#define BRANCH_EXIT_MAX()
#define BRANCH_EXIT_MIN()
#define END_BRANCH_TRACKING()

#endif

#define TRACK_ENTRY_MAX()        \
    best_section = MOVE_SECTION; \
    goto track_max;

#define TRACK_ENTRY_MIN()        \
    best_section = MOVE_SECTION; \
    goto track_min;

// Make the code looks prettier
#define MOVE_SECTION_BEGIN(n)           \
    {                                   \
        enum                            \
        {                               \
            MOVE_SECTION = (n)          \
        };                              \
        if (loaded_best_section != (n)) \
        {                               \
            __movesect_##n:

#define MOVE_SECTION_END(jump) \
    if (has_jumped)            \
        goto jump;             \
    }                          \
    }

#define GET_CACHE_COUNT() 0

BEGIN_BRANCH_TRACKING();

// Call signatures in case those would change any time
#define MINIMAX_FUNC(func)                                                                                                    \
    func(int32_t depth, int32_t alpha, int32_t beta, const int32_t evaluation,                                                \
         const uint64_t is_fir_mask, const uint64_t is_sec_mask, const uint64_t is_link_mask, const uint64_t is_boosted_mask, \
         const extra_args_t args)

#define MINIMAX_CALL(func, depth, alpha, beta, eval, fm, sm, lm, bm, as) \
    func(depth, alpha, beta, eval, fm, sm, lm, bm, as)

#define BRANCHED_MINIMAX_CALL(cond, func_true, func_false, depth, alpha, beta, eval, fm, sm, lm, bm, as) \
    ((cond) ? func_true : func_false)(depth, alpha, beta, eval, fm, sm, lm, bm, as)

#define MINIMAX_UNPACK()

// Way faster path for the last iteration

RNAB_HOT_FUNCTION __attribute__((aligned(FUNC_ALIGN))) __attribute__((hot)) static int32_t MINIMAX_FUNC(minimax_max);
RNAB_HOT_FUNCTION __attribute__((aligned(FUNC_ALIGN))) __attribute__((hot)) static int32_t MINIMAX_FUNC(minimax_min);

RNAB_HOT_FUNCTION __attribute__((aligned(FUNC_ALIGN))) __attribute__((hot)) static int32_t MINIMAX_FUNC(minimax_min_term)
{
#ifdef BRANCH_DEBUG
    ++rec_counter;
#endif
    MINIMAX_UNPACK();

    RNAB_ASSUME(depth == 1);
    RNAB_ASSUME(args.fields.fir_link <= 3);
    RNAB_ASSUME(args.fields.fir_virus <= 3);
    RNAB_ASSUME(args.fields.sec_link <= 3);
    RNAB_ASSUME(args.fields.sec_virus <= 3);

    const uint64_t fir_link_mask = is_link_mask & is_fir_mask;
    const uint64_t sec_link_mask = is_link_mask ^ fir_link_mask;
    const uint64_t sec_virus_mask = is_sec_mask ^ sec_link_mask;

    const uint64_t cur_boosted_mask = is_boosted_mask & is_sec_mask;
    const uint64_t enemy_firewall_mask = (uint64_t)(args.fields.firewall_fir & 1) << (args.fields.firewall_fir >> 1);
    const uint64_t free_mask = ~(is_fir_mask | is_sec_mask | enemy_firewall_mask);
    const uint64_t boosted_link = cur_boosted_mask & sec_link_mask;
    const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask;

    int32_t score = MAX;

    if (args.fields.sec_link == 3) // fast path if we are about to win
    {
        // there is either a link at an exit square or a boosted link which can reach it
        if ((sec_link_mask | (((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask)) & 1729382256910270464ULL)
            return -SCORE_TERMINAL_BASE - depth;
        if (links_masked_out)
        {
            // check if any of the cards can simply reach unprotected enemy link in one move
#ifdef __AVX2__
            __m256i tgt = _mm256_set1_epi64x(links_masked_out);

            __m256i shuffled = _mm256_and_si256(_mm256_blend_epi32(_mm256_or_si256(_mm256_srli_si256(tgt, 9), _mm256_srli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, CAN_MOVE_RIGHT, 0, 0)), 1)), _mm256_or_si256(_mm256_slli_si256(tgt, 9), _mm256_slli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, 0, CAN_MOVE_LEFT, 0)), 1)), 0b11110000), _mm256_set1_epi64x(is_sec_mask));

            if (!_mm256_testz_si256(shuffled, shuffled))
#else
            if (((links_masked_out >> 8) | (links_masked_out << 8) | ((links_masked_out & CAN_MOVE_RIGHT) >> 1) | ((links_masked_out & CAN_MOVE_LEFT) << 1)) & is_sec_mask)
#endif
                return -SCORE_TERMINAL_BASE - depth;

            // now let's check if we can reach any of the unprotected links with a boosted card if there is one
            if (cur_boosted_mask)
            {
                // it looks like SIMD check is almost 3 times faster than the scalar one
#ifdef __AVX2__
                /*
                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask << 7)  & ((free_mask >> 1) | (free_mask << 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask << 9)  & ((free_mask << 1) | (free_mask << 8))
                links_masked_out & -1                      & (cur_boosted_mask << 16) & (0                | (free_mask << 8))
                links_masked_out & CAN_MOVE_DOUBLE_RIGHT   & (cur_boosted_mask << 2)  & ((free_mask << 1) | 0               )


                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask >> 9)  & ((free_mask >> 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask >> 7)  & ((free_mask << 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_DOUBLE_LEFT    & (cur_boosted_mask >> 2)  & ((free_mask >> 1) | 0               )
                links_masked_out & -1                      & (cur_boosted_mask >> 16) & (0                | (free_mask >> 8))
                */

                const __m256i zero_mask = _mm256_setzero_si256();

                __m256i fm = _mm256_set1_epi64x(free_mask);
                __m256i cm = _mm256_set1_epi64x(cur_boosted_mask);
                __m256i shift_comb = _mm256_blend_epi32(_mm256_slli_epi64(fm, 1), _mm256_srli_epi64(fm, 1), 0b00110011);

                if (!_mm256_testz_si256(_mm256_or_si256(_mm256_and_si256(_mm256_and_si256(_mm256_sllv_epi64(cm, _mm256_setr_epi64x(7, 9, 16, 2)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(-1, 0, 1, 2, 3, 4, 5, 6, -1, 8, 9, 10, 11, 12, 13, 14, -1, 16, 17, 18, 19, 20, 21, 22, -1, -1, -1, -1, -1, -1, -1, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b11001111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, -1, CAN_MOVE_DOUBLE_RIGHT)), _mm256_and_si256(_mm256_and_si256(_mm256_srlv_epi64(cm, _mm256_setr_epi64x(9, 7, 2, 16)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(1, 2, 3, 4, 5, 6, 7, -1, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, 26, 27, 28, 29, 30, 31, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b00111111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, CAN_MOVE_DOUBLE_LEFT, -1))), tgt))
#else
                if (links_masked_out & (((free_mask << 8) & (cur_boosted_mask << 16)) |                                      // down and not blocked
                                        ((free_mask >> 8) & (cur_boosted_mask >> 16)) |                                      // up and not blocked
                                        (CAN_MOVE_DOUBLE_RIGHT & (cur_boosted_mask << 2) & (free_mask << 1)) |               // right and not blocked
                                        (CAN_MOVE_DOUBLE_LEFT & (cur_boosted_mask >> 2) & (free_mask >> 1)) |                // left and not blocked
                                        (CAN_MOVE_LEFT & (cur_boosted_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) |  // down left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask << 9) & ((free_mask << 1) | (free_mask << 8))) | // down right
                                        (CAN_MOVE_LEFT & (cur_boosted_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) |  // up left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))))) // up right
#endif
                    return -SCORE_TERMINAL_BASE - depth;
            }
        }
    }

    do
    {
        const int32_t new_eval = evaluation + ((args.fields.firewall_sec) ? -8 : 8);

        score = (new_eval < score) ? new_eval : score;
        beta = (new_eval < beta) ? new_eval : beta;

        if (beta <= alpha)
        {
            return score;
        }
    } while (0);

    if ((sec_link_mask & 1152921504606846976ULL) != 0 || (sec_link_mask & 576460752303423488ULL) != 0)
    {
        const int32_t new_eval = evaluation - (128 << args.fields.sec_link) + 7;

        score = (new_eval < score) ? new_eval : score;
        beta = (new_eval < beta) ? new_eval : beta;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if (__builtin_expect((((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 1729382256910270464ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation - (128 << args.fields.sec_link) + (__builtin_ctzll(cur_boosted_mask) >> 3);

        score = (new_eval < score) ? new_eval : score;
        beta = (new_eval < beta) ? new_eval : beta;

        if (beta <= alpha)
        {
            return score;
        }
    }

    const uint64_t cards_attack_mask = is_sec_mask & ((links_masked_out >> 8) | ((links_masked_out << 1) & CAN_MOVE_RIGHT) | ((links_masked_out >> 1) & CAN_MOVE_LEFT) | (links_masked_out << 8));

    if (cards_attack_mask != 0)
    {
        const int32_t new_eval = evaluation - ((63 - __builtin_ctzll(cards_attack_mask)) >> 3) - (128 << args.fields.sec_link);

        score = (new_eval < score) ? new_eval : score;
        beta = (new_eval < beta) ? new_eval : beta;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if (cur_boosted_mask && (((cur_boosted_mask & (free_mask << 8) & (links_masked_out << 16)) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0) ||
                             ((cur_boosted_mask & (free_mask >> 8) & (links_masked_out >> 16)) != 0)))
    {
        const int32_t new_eval = evaluation - (__builtin_clzll(cur_boosted_mask) >> 3) - (128 << args.fields.sec_link);

        score = (new_eval < score) ? new_eval : score;
        beta = (new_eval < beta) ? new_eval : beta;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if ((cur_boosted_mask == 0 && is_sec_mask) != 0 || (cur_boosted_mask != 0))
    {
        score = (evaluation < score) ? evaluation : score;
        beta = (evaluation < beta) ? evaluation : beta;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if (cur_boosted_mask)
    {
        const uint64_t move_mask = free_mask & (((cur_boosted_mask >> 16) & (free_mask >> 8)) |
                                                ((cur_boosted_mask >> 7) & CAN_MOVE_RIGHT & ((free_mask << 1) | (free_mask >> 8))) |
                                                ((cur_boosted_mask >> 9) & CAN_MOVE_LEFT & ((free_mask >> 1) | (free_mask >> 8))) |

                                                ((cur_boosted_mask << 2) & CAN_MOVE_DOUBLE_RIGHT & (free_mask << 1)) |
                                                ((cur_boosted_mask >> 2) & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1)) |

                                                ((cur_boosted_mask << 9) & CAN_MOVE_RIGHT & ((free_mask << 1) | (free_mask << 8))) |
                                                ((cur_boosted_mask << 7) & CAN_MOVE_LEFT & ((free_mask >> 1) | (free_mask << 8))) |
                                                ((cur_boosted_mask << 16) & (free_mask << 8)));

        if (move_mask)
        {
            const int32_t new_eval = evaluation + (__builtin_ctzll(cur_boosted_mask) >> 3) - ((63 - __builtin_clzll(move_mask)) >> 3);

            score = (new_eval < score) ? new_eval : score;
        }
    }

    if ((is_sec_mask << 8) & free_mask)
    {
        const int32_t new_eval = evaluation - 1;

        score = (new_eval < score) ? new_eval : score;
    }

    if ((((is_sec_mask << 1) & CAN_MOVE_RIGHT) | ((is_sec_mask >> 1) & CAN_MOVE_LEFT)))
    {
        const int32_t new_eval = evaluation;

        score = (new_eval < score) ? new_eval : score;
    }

    if ((is_sec_mask >> 8) & free_mask)
    {
        const int32_t new_eval = evaluation + 1;

        score = (new_eval < score) ? new_eval : score;
    }

    return score;
}

RNAB_HOT_FUNCTION __attribute__((aligned(FUNC_ALIGN))) __attribute__((hot)) static int32_t MINIMAX_FUNC(minimax_max_term)
{
#ifdef BRANCH_DEBUG
    ++rec_counter;
#endif
    MINIMAX_UNPACK();

    RNAB_ASSUME(depth == 1);
    RNAB_ASSUME(args.fields.fir_link <= 3);
    RNAB_ASSUME(args.fields.fir_virus <= 3);
    RNAB_ASSUME(args.fields.sec_link <= 3);
    RNAB_ASSUME(args.fields.sec_virus <= 3);

    const uint64_t fir_link_mask = is_link_mask & is_fir_mask;
    const uint64_t fir_virus_mask = is_fir_mask ^ fir_link_mask;
    const uint64_t sec_link_mask = is_link_mask ^ fir_link_mask;

    const uint64_t cur_boosted_mask = is_boosted_mask & is_fir_mask;
    const uint64_t enemy_firewall_mask = (uint64_t)(args.fields.firewall_sec & 1) << (args.fields.firewall_sec >> 1);
    const uint64_t free_mask = ~(is_fir_mask | is_sec_mask | enemy_firewall_mask);
    const uint64_t boosted_link = cur_boosted_mask & fir_link_mask;
    const uint64_t links_masked_out = sec_link_mask & ~enemy_firewall_mask;

    int32_t score = MIN;

    if (args.fields.fir_link == 3) // fast path if we are about to win
    {
        // there is either a link at an exit square or a boosted link which can reach it
        if ((fir_link_mask | (((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask)) & 24ULL)
            return SCORE_TERMINAL_BASE + depth;

        if (links_masked_out)
        {
            // check if any of the cards can simply reach unprotected enemy link in one move
#ifdef __AVX2__
            __m256i tgt = _mm256_set1_epi64x(links_masked_out);

            __m256i shuffled = _mm256_and_si256(_mm256_blend_epi32(_mm256_or_si256(_mm256_srli_si256(tgt, 9), _mm256_srli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, CAN_MOVE_RIGHT, 0, 0)), 1)), _mm256_or_si256(_mm256_slli_si256(tgt, 9), _mm256_slli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, 0, CAN_MOVE_LEFT, 0)), 1)), 0b11110000), _mm256_set1_epi64x(is_fir_mask));

            if (!_mm256_testz_si256(shuffled, shuffled))
#else
            if (((links_masked_out >> 8) | (links_masked_out << 8) | ((links_masked_out & CAN_MOVE_RIGHT) >> 1) | ((links_masked_out & CAN_MOVE_LEFT) << 1)) & is_fir_mask)
#endif
                return SCORE_TERMINAL_BASE + depth;

            // now let's check if we can reach any of the unprotected links with a boosted card if there is one
            if (cur_boosted_mask)
            {
                // it looks like SIMD check is almost 3 times faster than the scalar one
#ifdef __AVX2__
                /*
                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask << 7)  & ((free_mask >> 1) | (free_mask << 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask << 9)  & ((free_mask << 1) | (free_mask << 8))
                links_masked_out & -1                      & (cur_boosted_mask << 16) & (0                | (free_mask << 8))
                links_masked_out & CAN_MOVE_DOUBLE_RIGHT   & (cur_boosted_mask << 2)  & ((free_mask << 1) | 0               )


                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask >> 9)  & ((free_mask >> 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask >> 7)  & ((free_mask << 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_DOUBLE_LEFT    & (cur_boosted_mask >> 2)  & ((free_mask >> 1) | 0               )
                links_masked_out & -1                      & (cur_boosted_mask >> 16) & (0                | (free_mask >> 8))
                */

                const __m256i zero_mask = _mm256_setzero_si256();

                __m256i fm = _mm256_set1_epi64x(free_mask);
                __m256i cm = _mm256_set1_epi64x(cur_boosted_mask);
                __m256i shift_comb = _mm256_blend_epi32(_mm256_slli_epi64(fm, 1), _mm256_srli_epi64(fm, 1), 0b00110011);

                if (!_mm256_testz_si256(_mm256_or_si256(_mm256_and_si256(_mm256_and_si256(_mm256_sllv_epi64(cm, _mm256_setr_epi64x(7, 9, 16, 2)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(-1, 0, 1, 2, 3, 4, 5, 6, -1, 8, 9, 10, 11, 12, 13, 14, -1, 16, 17, 18, 19, 20, 21, 22, -1, -1, -1, -1, -1, -1, -1, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b11001111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, -1, CAN_MOVE_DOUBLE_RIGHT)), _mm256_and_si256(_mm256_and_si256(_mm256_srlv_epi64(cm, _mm256_setr_epi64x(9, 7, 2, 16)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(1, 2, 3, 4, 5, 6, 7, -1, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, 26, 27, 28, 29, 30, 31, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b00111111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, CAN_MOVE_DOUBLE_LEFT, -1))), tgt))
#else
                if (links_masked_out & (((free_mask << 8) & (cur_boosted_mask << 16)) |                                      // down and not blocked
                                        ((free_mask >> 8) & (cur_boosted_mask >> 16)) |                                      // up and not blocked
                                        (CAN_MOVE_DOUBLE_RIGHT & (cur_boosted_mask << 2) & (free_mask << 1)) |               // right and not blocked
                                        (CAN_MOVE_DOUBLE_LEFT & (cur_boosted_mask >> 2) & (free_mask >> 1)) |                // left and not blocked
                                        (CAN_MOVE_LEFT & (cur_boosted_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) |  // down left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask << 9) & ((free_mask << 1) | (free_mask << 8))) | // down right
                                        (CAN_MOVE_LEFT & (cur_boosted_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) |  // up left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))))) // up right
#endif
                    return SCORE_TERMINAL_BASE + depth;
            }
        }
    }

    do
    {
        const int32_t new_eval = evaluation + ((args.fields.firewall_fir) ? 8 : -8);

        score = (new_eval > score) ? new_eval : score;
        alpha = (new_eval > alpha) ? new_eval : alpha;

        if (beta <= alpha)
        {
            return score;
        }
    } while (0);

    if ((fir_link_mask & 16ULL) || (fir_link_mask & 8ULL))
    {
        const int32_t new_eval = evaluation + (128 << args.fields.fir_link) - 7;

        score = (new_eval > score) ? new_eval : score;
        alpha = (new_eval > alpha) ? new_eval : alpha;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if (__builtin_expect((((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 24ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation + (128 << args.fields.fir_link) - (__builtin_clzll(cur_boosted_mask) >> 3);

        score = (new_eval > score) ? new_eval : score;
        alpha = (new_eval > alpha) ? new_eval : alpha;

        if (beta <= alpha)
        {
            return score;
        }
    }

    const uint64_t cards_attack_mask = is_fir_mask & ((links_masked_out >> 8) | ((links_masked_out << 1) & CAN_MOVE_RIGHT) | ((links_masked_out >> 1) & CAN_MOVE_LEFT) | (links_masked_out << 8));

    if (cards_attack_mask != 0)
    {
        const int32_t new_eval = evaluation + ((63 - __builtin_clzll(cards_attack_mask)) >> 3) + (128 << args.fields.fir_link);

        score = (new_eval > score) ? new_eval : score;
        alpha = (new_eval > alpha) ? new_eval : alpha;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if (cur_boosted_mask && (((cur_boosted_mask & (free_mask << 8) & (links_masked_out << 16)) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0) ||
                             ((cur_boosted_mask & CAN_MOVE_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0) ||
                             ((cur_boosted_mask & (free_mask >> 8) & (links_masked_out >> 16)) != 0)))
    {
        const int32_t new_eval = evaluation + (__builtin_ctzll(cur_boosted_mask) >> 3) + (128 << args.fields.fir_link);

        score = (new_eval > score) ? new_eval : score;
        alpha = (new_eval > alpha) ? new_eval : alpha;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if ((cur_boosted_mask == 0 && is_fir_mask) != 0 || (cur_boosted_mask != 0))
    {
        score = (evaluation > score) ? evaluation : score;
        alpha = (evaluation > alpha) ? evaluation : alpha;

        if (beta <= alpha)
        {
            return score;
        }
    }

    if (cur_boosted_mask)
    {
        const uint64_t move_mask = free_mask & (((cur_boosted_mask >> 16) & (free_mask >> 8)) |
                                                ((cur_boosted_mask >> 7) & CAN_MOVE_RIGHT & ((free_mask << 1) | (free_mask >> 8))) |
                                                ((cur_boosted_mask >> 9) & CAN_MOVE_LEFT & ((free_mask >> 1) | (free_mask >> 8))) |

                                                ((cur_boosted_mask << 2) & CAN_MOVE_DOUBLE_RIGHT & (free_mask << 1)) |
                                                ((cur_boosted_mask >> 2) & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1)) |

                                                ((cur_boosted_mask << 9) & CAN_MOVE_RIGHT & ((free_mask << 1) | (free_mask << 8))) |
                                                ((cur_boosted_mask << 7) & CAN_MOVE_LEFT & ((free_mask >> 1) | (free_mask << 8))) |
                                                ((cur_boosted_mask << 16) & (free_mask << 8)));

        if (move_mask)
        {
            const int32_t new_eval = evaluation + ((63 - __builtin_ctzll(move_mask)) >> 3) - (__builtin_clzll(cur_boosted_mask) >> 3);

            score = (new_eval > score) ? new_eval : score;
        }
    }

    if ((is_fir_mask >> 8) & free_mask)
    {
        const int32_t new_eval = evaluation + 1;

        score = (new_eval > score) ? new_eval : score;
    }

    if ((((is_fir_mask << 1) & CAN_MOVE_RIGHT) | ((is_fir_mask >> 1) & CAN_MOVE_LEFT)))
    {
        const int32_t new_eval = evaluation;

        score = (new_eval > score) ? new_eval : score;
    }

    if ((is_fir_mask << 8) & free_mask)
    {
        const int32_t new_eval = evaluation - 1;

        score = (new_eval > score) ? new_eval : score;
    }

    return score;
}

#undef PERFORM_ITERATION_FIR
#undef PERFORM_ITERATION_SEC

#define PERFORM_ITERATION_FIR(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                                                                                                                                                                                               \
    if (is_sec_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                                                                                                                                       \
        if (sec_link_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                                               \
        {                                                                                                                                                                                                                                                                                                                                   \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                  \
            const int32_t new_eval = evaluation + cur_pos_bit + (128 << args.fields.fir_link);                                                                                                                                                                                                                                              \
                                                                                                                                                                                                                                                                                                                                            \
            BRANCH_ENTER_MAX("capture link");                                                                                                                                                                                                                                                                                               \
            const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_min, minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask ^ new_pos_bitboard,                                                                                                                     \
                                                           ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ cur_pos_bitboard) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask ^ new_pos_bitboard) : ((is_link_mask ^ new_pos_bitboard ^ unknown_mask) | (unknown_mask shift_func shift_count)))), \
                                                           ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                              \
                                                           (const extra_args_t){.raw = args.raw + 65536ULL});                                                                                                                                                                                                                               \
            BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                              \
                                                                                                                                                                                                                                                                                                                                            \
            score = (reschild > score) ? reschild : score;                                                                                                                                                                                                                                                                                  \
            alpha = (reschild > alpha) ? reschild : alpha;                                                                                                                                                                                                                                                                                  \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                              \
            {                                                                                                                                                                                                                                                                                                                               \
                TRACK_ENTRY_MAX();                                                                                                                                                                                                                                                                                                          \
            }                                                                                                                                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                                                                                                                   \
        else if (virus_can_be_captured)                                                                                                                                                                                                                                                                                                     \
        {                                                                                                                                                                                                                                                                                                                                   \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                  \
            const int32_t new_eval = evaluation + cur_pos_bit - (64 << args.fields.fir_virus);                                                                                                                                                                                                                                              \
                                                                                                                                                                                                                                                                                                                                            \
            int32_t reschild;                                                                                                                                                                                                                                                                                                               \
            if (is_boosted || current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                                                                                                                                                                                              \
            {                                                                                                                                                                                                                                                                                                                               \
                BRANCH_ENTER_MAX("capture virus boosted || link");                                                                                                                                                                                                                                                                          \
                reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_min, minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask ^ new_pos_bitboard,                                                                                                                               \
                                                 ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                            \
                                                 ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                        \
                                                 (const extra_args_t){.raw = args.raw + 4294967296ULL});                                                                                                                                                                                                                                    \
                BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                          \
            }                                                                                                                                                                                                                                                                                                                               \
            else                                                                                                                                                                                                                                                                                                                            \
            {                                                                                                                                                                                                                                                                                                                               \
                BRANCH_ENTER_MAX("capture virus not boosted");                                                                                                                                                                                                                                                                              \
                if (depth > 1)                                                                                                                                                                                                                                                                                                              \
                {                                                                                                                                                                                                                                                                                                                           \
                    reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask ^ new_pos_bitboard,                                                                                                                                                            \
                                            ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                 \
                                            ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                             \
                                            (const extra_args_t){.raw = args.raw + 4294967296ULL});                                                                                                                                                                                                                                         \
                    if (reschild > alpha && beta > alpha + 1)                                                                                                                                                                                                                                                                               \
                        reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask ^ new_pos_bitboard,                                                                                                                                                             \
                                                ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                             \
                                                ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                         \
                                                (const extra_args_t){.raw = args.raw + 4294967296ULL});                                                                                                                                                                                                                                     \
                }                                                                                                                                                                                                                                                                                                                           \
                else                                                                                                                                                                                                                                                                                                                        \
                    reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask ^ new_pos_bitboard,                                                                                                                                                            \
                                            ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                 \
                                            ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                             \
                                            (const extra_args_t){.raw = args.raw + 4294967296ULL});                                                                                                                                                                                                                                         \
                                                                                                                                                                                                                                                                                                                                            \
                BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                          \
            }                                                                                                                                                                                                                                                                                                                               \
                                                                                                                                                                                                                                                                                                                                            \
            score = (reschild > score) ? reschild : score;                                                                                                                                                                                                                                                                                  \
            alpha = (reschild > alpha) ? reschild : alpha;                                                                                                                                                                                                                                                                                  \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                              \
            {                                                                                                                                                                                                                                                                                                                               \
                TRACK_ENTRY_MAX();                                                                                                                                                                                                                                                                                                          \
            }                                                                                                                                                                                                                                                                                                                               \
        }                                                                                                                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                                                                                                                       \
    else if ((is_fir_mask & new_pos_bitboard) == 0)                                                                                                                                                                                                                                                                                         \
    {                                                                                                                                                                                                                                                                                                                                       \
        const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                      \
        const int32_t new_eval = evaluation + (forward_adv);                                                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                            \
        int32_t reschild;                                                                                                                                                                                                                                                                                                                   \
        if (is_boosted)                                                                                                                                                                                                                                                                                                                     \
        {                                                                                                                                                                                                                                                                                                                                   \
            BRANCH_ENTER_MAX("move boosted");                                                                                                                                                                                                                                                                                               \
            reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_min, minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask,                                                                                                                                                      \
                                             ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                \
                                             ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                \
                                             args);                                                                                                                                                                                                                                                                                         \
            BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                                                                                                                                   \
        else                                                                                                                                                                                                                                                                                                                                \
        {                                                                                                                                                                                                                                                                                                                                   \
            BRANCH_ENTER_MAX("move not boosted");                                                                                                                                                                                                                                                                                           \
            if (depth > 1)                                                                                                                                                                                                                                                                                                                  \
            {                                                                                                                                                                                                                                                                                                                               \
                reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask,                                                                                                                                                                                   \
                                        ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                     \
                                        ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                     \
                                        args);                                                                                                                                                                                                                                                                                              \
                if (reschild > alpha && beta > alpha + 1)                                                                                                                                                                                                                                                                                   \
                    reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask,                                                                                                                                                                                    \
                                            ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                 \
                                            ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                 \
                                            args);                                                                                                                                                                                                                                                                                          \
            }                                                                                                                                                                                                                                                                                                                               \
            else                                                                                                                                                                                                                                                                                                                            \
                reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), is_sec_mask,                                                                                                                                                                                   \
                                        ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                     \
                                        ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                     \
                                        args);                                                                                                                                                                                                                                                                                              \
                                                                                                                                                                                                                                                                                                                                            \
            BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                                                                                                                                   \
                                                                                                                                                                                                                                                                                                                                            \
        score = (reschild > score) ? reschild : score;                                                                                                                                                                                                                                                                                      \
        alpha = (reschild > alpha) ? reschild : alpha;                                                                                                                                                                                                                                                                                      \
        if (beta <= alpha)                                                                                                                                                                                                                                                                                                                  \
        {                                                                                                                                                                                                                                                                                                                                   \
            TRACK_ENTRY_MAX();                                                                                                                                                                                                                                                                                                              \
        }                                                                                                                                                                                                                                                                                                                                   \
    }

#define PERFORM_ITERATION_SEC(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                                                                                                                                                                                                 \
    if (is_fir_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                                                         \
        if (fir_link_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                                                 \
        {                                                                                                                                                                                                                                                                                                                                     \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                    \
            const int32_t new_eval = evaluation - cur_pos_bit - (128 << args.fields.sec_link);                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                              \
            BRANCH_ENTER_MIN("capture link");                                                                                                                                                                                                                                                                                                 \
            const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_max, minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask ^ new_pos_bitboard, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                       \
                                                           ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ cur_pos_bitboard) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask ^ new_pos_bitboard) : ((is_link_mask ^ new_pos_bitboard ^ unknown_mask) | (unknown_mask shift_func shift_count)))), \
                                                           ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                \
                                                           (const extra_args_t){.raw = args.raw + 16777216ULL});                                                                                                                                                                                                                              \
            BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                              \
            score = (reschild < score) ? reschild : score;                                                                                                                                                                                                                                                                                    \
            beta = (reschild < beta) ? reschild : beta;                                                                                                                                                                                                                                                                                       \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                                \
            {                                                                                                                                                                                                                                                                                                                                 \
                TRACK_ENTRY_MIN();                                                                                                                                                                                                                                                                                                            \
            }                                                                                                                                                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                                                                                                                     \
        else if (virus_can_be_captured)                                                                                                                                                                                                                                                                                                       \
        {                                                                                                                                                                                                                                                                                                                                     \
            const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                    \
            const int32_t new_eval = evaluation - cur_pos_bit + (64 << args.fields.sec_virus);                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                              \
            int32_t reschild;                                                                                                                                                                                                                                                                                                                 \
            if (is_boosted || current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                                                                                                                                                                                               \
            {                                                                                                                                                                                                                                                                                                                                 \
                BRANCH_ENTER_MIN("capture virus boosted || link");                                                                                                                                                                                                                                                                            \
                reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_max, minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask ^ new_pos_bitboard, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                 \
                                                 ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                            \
                                                 ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                          \
                                                 (const extra_args_t){.raw = args.raw + 1099511627776ULL});                                                                                                                                                                                                                                   \
                BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                            \
            }                                                                                                                                                                                                                                                                                                                                 \
            else                                                                                                                                                                                                                                                                                                                              \
            {                                                                                                                                                                                                                                                                                                                                 \
                BRANCH_ENTER_MIN("capture virus not boosted");                                                                                                                                                                                                                                                                                \
                if (depth > 1)                                                                                                                                                                                                                                                                                                                \
                {                                                                                                                                                                                                                                                                                                                             \
                    reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, new_eval, is_fir_mask ^ new_pos_bitboard, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                \
                                            ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                 \
                                            ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                               \
                                            (const extra_args_t){.raw = args.raw + 1099511627776ULL});                                                                                                                                                                                                                                        \
                    if (reschild < beta && alpha < beta - 1)                                                                                                                                                                                                                                                                                  \
                        reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, new_eval, is_fir_mask ^ new_pos_bitboard, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                               \
                                                ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                             \
                                                ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                           \
                                                (const extra_args_t){.raw = args.raw + 1099511627776ULL});                                                                                                                                                                                                                                    \
                }                                                                                                                                                                                                                                                                                                                             \
                else                                                                                                                                                                                                                                                                                                                          \
                    reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask ^ new_pos_bitboard, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                              \
                                            ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                 \
                                            ((is_boosted) ? ((is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                               \
                                            (const extra_args_t){.raw = args.raw + 1099511627776ULL});                                                                                                                                                                                                                                        \
                                                                                                                                                                                                                                                                                                                                              \
                BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                            \
            }                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                              \
            score = (reschild < score) ? reschild : score;                                                                                                                                                                                                                                                                                    \
            beta = (reschild < beta) ? reschild : beta;                                                                                                                                                                                                                                                                                       \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                                \
            {                                                                                                                                                                                                                                                                                                                                 \
                TRACK_ENTRY_MIN();                                                                                                                                                                                                                                                                                                            \
            }                                                                                                                                                                                                                                                                                                                                 \
        }                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                         \
    else if ((is_sec_mask & new_pos_bitboard) == 0)                                                                                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                                                                                                         \
        const uint64_t unknown_mask = is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                        \
        const int32_t new_eval = evaluation - (forward_adv);                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                              \
        int32_t reschild;                                                                                                                                                                                                                                                                                                                     \
        if (is_boosted)                                                                                                                                                                                                                                                                                                                       \
        {                                                                                                                                                                                                                                                                                                                                     \
            BRANCH_ENTER_MIN("move boosted");                                                                                                                                                                                                                                                                                                 \
            reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_max, minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                        \
                                             ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                \
                                             ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                  \
                                             args);                                                                                                                                                                                                                                                                                           \
            BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                                                                                                                     \
        else                                                                                                                                                                                                                                                                                                                                  \
        {                                                                                                                                                                                                                                                                                                                                     \
            BRANCH_ENTER_MIN("move not boosted");                                                                                                                                                                                                                                                                                             \
            if (depth > 1)                                                                                                                                                                                                                                                                                                                    \
            {                                                                                                                                                                                                                                                                                                                                 \
                reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, new_eval, is_fir_mask, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                       \
                                        ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                     \
                                        ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                       \
                                        args);                                                                                                                                                                                                                                                                                                \
                if (reschild < beta && alpha < beta - 1)                                                                                                                                                                                                                                                                                      \
                    reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                      \
                                            ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                 \
                                            ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                   \
                                            args);                                                                                                                                                                                                                                                                                            \
            }                                                                                                                                                                                                                                                                                                                                 \
            else                                                                                                                                                                                                                                                                                                                              \
                reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                     \
                                        ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (is_link_mask) : ((is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                                     \
                                        ((is_boosted) ? (is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (is_boosted_mask)),                                                                                                                                                                                                       \
                                        args);                                                                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                              \
            BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                                                                                                                     \
                                                                                                                                                                                                                                                                                                                                              \
        score = (reschild < score) ? reschild : score;                                                                                                                                                                                                                                                                                        \
        beta = (reschild < beta) ? reschild : beta;                                                                                                                                                                                                                                                                                           \
        if (beta <= alpha)                                                                                                                                                                                                                                                                                                                    \
        {                                                                                                                                                                                                                                                                                                                                     \
            TRACK_ENTRY_MIN();                                                                                                                                                                                                                                                                                                                \
        }                                                                                                                                                                                                                                                                                                                                     \
    }

#ifdef TRACK_CUTOFF_STATS
typedef struct
{
    uint64_t nodes;
    uint64_t tt_hits;
    uint64_t tt_cutoffs;
} ply_stats_t;

static ply_stats_t ply_stats[32];
static uint64_t best_section_cutoff_tracker[32][16];
static uint64_t best_section_cutoff_tracker_all[32][16];
#endif

#ifdef RNAB_REVCACHE
static bool debug_revhash = false;

#define RNAB_SET_REVCACHE()   \
    do                        \
    {                         \
        debug_revhash = true; \
    } while (0);
#define RNAB_RESET_REVCACHE()  \
    do                         \
    {                          \
        debug_revhash = false; \
    } while (0);

#else

#define RNAB_SET_REVCACHE() \
    do                      \
    {                       \
    } while (0);
#define RNAB_RESET_REVCACHE() \
    do                        \
    {                         \
    } while (0);

#endif

RNAB_HOT_FUNCTION __attribute__((aligned(FUNC_ALIGN))) __attribute__((hot)) static int32_t MINIMAX_FUNC(minimax_max)
{
    if (__builtin_expect(should_exit != 0, 0))
        return 0;
#ifdef TRACK_CUTOFF_STATS
    ply_stats[depth].nodes++;
#endif
#ifdef BRANCH_DEBUG
    ++rec_counter;
#endif
    MINIMAX_UNPACK();

    RNAB_ASSUME(depth > 0);
    RNAB_ASSUME(args.fields.fir_link <= 3);
    RNAB_ASSUME(args.fields.fir_virus <= 3);
    RNAB_ASSUME(args.fields.sec_link <= 3);
    RNAB_ASSUME(args.fields.sec_virus <= 3);

    const uint64_t fir_link_mask = is_link_mask & is_fir_mask;
    const uint64_t fir_virus_mask = is_fir_mask ^ fir_link_mask;
    const uint64_t sec_link_mask = is_link_mask ^ fir_link_mask;
    const uint64_t sec_virus_mask = is_sec_mask ^ sec_link_mask;

    const uint64_t cur_boosted_mask = is_boosted_mask & is_fir_mask;
    const uint64_t enemy_firewall_mask = (uint64_t)(args.fields.firewall_sec & 1) << (args.fields.firewall_sec >> 1);
    const uint64_t free_mask = ~(is_fir_mask | is_sec_mask | enemy_firewall_mask);
    const uint64_t boosted_link = cur_boosted_mask & fir_link_mask;
    const uint64_t links_masked_out = sec_link_mask & ~enemy_firewall_mask;
    const bool virus_can_be_captured = args.fields.fir_virus < 3;

    const tt_bucket_t *__restrict__ bucket;
    uint64_t hash;
    int32_t alphabeg;
    uint32_t loaded_best_section = 15u;
    uint32_t best_section = 15u;
    bool has_jumped = false;

    int32_t score = MIN;

    if (args.fields.fir_link == 3) // fast path if we are about to win
    {
        // there is either a link at an exit square or a boosted link which can reach it
        if ((fir_link_mask | (((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask)) & 24ULL)
            return SCORE_TERMINAL_BASE + depth;

        if (links_masked_out)
        {
            // check if any of the cards can simply reach unprotected enemy link in one move
#ifdef __AVX2__
            __m256i tgt = _mm256_set1_epi64x(links_masked_out);

            __m256i shuffled = _mm256_and_si256(_mm256_blend_epi32(_mm256_or_si256(_mm256_srli_si256(tgt, 9), _mm256_srli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, CAN_MOVE_RIGHT, 0, 0)), 1)), _mm256_or_si256(_mm256_slli_si256(tgt, 9), _mm256_slli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, 0, CAN_MOVE_LEFT, 0)), 1)), 0b11110000), _mm256_set1_epi64x(is_fir_mask));

            if (!_mm256_testz_si256(shuffled, shuffled))
#else
            if (((links_masked_out >> 8) | (links_masked_out << 8) | ((links_masked_out & CAN_MOVE_RIGHT) >> 1) | ((links_masked_out & CAN_MOVE_LEFT) << 1)) & is_fir_mask)
#endif
                return SCORE_TERMINAL_BASE + depth;

            // now let's check if we can reach any of the unprotected links with a boosted card if there is one
            if (cur_boosted_mask)
            {
                // it looks like SIMD check is almost 3 times faster than the scalar one
#ifdef __AVX2__
                /*
                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask << 7)  & ((free_mask >> 1) | (free_mask << 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask << 9)  & ((free_mask << 1) | (free_mask << 8))
                links_masked_out & -1                      & (cur_boosted_mask << 16) & (0                | (free_mask << 8))
                links_masked_out & CAN_MOVE_DOUBLE_RIGHT   & (cur_boosted_mask << 2)  & ((free_mask << 1) | 0               )


                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask >> 9)  & ((free_mask >> 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask >> 7)  & ((free_mask << 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_DOUBLE_LEFT    & (cur_boosted_mask >> 2)  & ((free_mask >> 1) | 0               )
                links_masked_out & -1                      & (cur_boosted_mask >> 16) & (0                | (free_mask >> 8))
                */

                const __m256i zero_mask = _mm256_setzero_si256();

                __m256i fm = _mm256_set1_epi64x(free_mask);
                __m256i cm = _mm256_set1_epi64x(cur_boosted_mask);
                __m256i shift_comb = _mm256_blend_epi32(_mm256_slli_epi64(fm, 1), _mm256_srli_epi64(fm, 1), 0b00110011);

                if (!_mm256_testz_si256(_mm256_or_si256(_mm256_and_si256(_mm256_and_si256(_mm256_sllv_epi64(cm, _mm256_setr_epi64x(7, 9, 16, 2)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(-1, 0, 1, 2, 3, 4, 5, 6, -1, 8, 9, 10, 11, 12, 13, 14, -1, 16, 17, 18, 19, 20, 21, 22, -1, -1, -1, -1, -1, -1, -1, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b11001111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, -1, CAN_MOVE_DOUBLE_RIGHT)), _mm256_and_si256(_mm256_and_si256(_mm256_srlv_epi64(cm, _mm256_setr_epi64x(9, 7, 2, 16)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(1, 2, 3, 4, 5, 6, 7, -1, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, 26, 27, 28, 29, 30, 31, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b00111111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, CAN_MOVE_DOUBLE_LEFT, -1))), tgt))
#else
                if (links_masked_out & (((free_mask << 8) & (cur_boosted_mask << 16)) |                                      // down and not blocked
                                        ((free_mask >> 8) & (cur_boosted_mask >> 16)) |                                      // up and not blocked
                                        (CAN_MOVE_DOUBLE_RIGHT & (cur_boosted_mask << 2) & (free_mask << 1)) |               // right and not blocked
                                        (CAN_MOVE_DOUBLE_LEFT & (cur_boosted_mask >> 2) & (free_mask >> 1)) |                // left and not blocked
                                        (CAN_MOVE_LEFT & (cur_boosted_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) |  // down left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask << 9) & ((free_mask << 1) | (free_mask << 8))) | // down right
                                        (CAN_MOVE_LEFT & (cur_boosted_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) |  // up left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))))) // up right
#endif
                    return SCORE_TERMINAL_BASE + depth;
            }
        }
    }

    --depth;

    static const void *movesect_jmp_buf[] = {
        &&__movesect_0, &&__movesect_1, &&__movesect_2, &&__movesect_3, &&__movesect_4,
        &&__movesect_5, &&__movesect_6, &&__movesect_7, &&__movesect_8, &&__movesect_9};

    // check if the move is cached
    if (depth > MIN_CACHE_DEPTH)
    {
#ifdef RNAB_REVCACHE
        if (debug_revhash)
        {
            const field_t cur = (field_t){is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, args};
            const field_t rev = reverse_field(&cur);
            hash = rapidhashNano(rev.is_fir_mask, rev.is_sec_mask, rev.is_link_mask, rev.is_boosted_mask, rev.args.raw, 0);
        }
        else
        {
            hash = rapidhashNano(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, args.raw, 0);
        }
#else
        hash = rapidhashNano(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, args.raw, 0);
#endif

        bucket = &tt_fir[hash & (TABLE_SIZE - 1)];
        hash = (hash >> TABLE_SIZE_BITS) & ENTRY_HASH_MASK;

        const uint64_t depth_preferred = bucket->depth_preferred;
        const uint64_t scratch = bucket->scratch;

        const uint64_t entry = ((depth_preferred & ENTRY_HASH_MASK) == hash) ? depth_preferred : (((scratch & ENTRY_HASH_MASK) == hash) ? scratch : 0ULL);

        const uint32_t entry_flag = (entry >> (ENTRY_HASH_SIZE + 0)) & 3;
        const uint32_t entry_depth = (entry >> (ENTRY_HASH_SIZE + 2)) & 31;
        const uint32_t entry_best_section = (entry >> (ENTRY_HASH_SIZE + 2 + 5)) & 15;
        const int32_t entry_eval = (int32_t)((int64_t)entry >> 52);
#ifdef TRACK_CUTOFF_STATS
        ply_stats[depth].tt_hits++;
        ply_stats[depth].tt_cutoffs++;
#endif

        if (entry_depth >= depth) // inherently wrong but it works
        {
            if (entry_flag == TT_EXACT)
            {
                return entry_eval;
            }
            else if (entry_flag == TT_LOWERBOUND)
            {
                if (entry_eval >= beta)
                    return entry_eval;
                alpha = (entry_eval > alpha) ? entry_eval : alpha;
            }
            else if (entry_flag == TT_UPPERBOUND)
            {
                if (entry_eval <= alpha)
                    return entry_eval;
                beta = (entry_eval < beta) ? entry_eval : beta;
            }
        }
#ifdef TRACK_CUTOFF_STATS
        ply_stats[depth].tt_cutoffs--;
#endif

        alphabeg = alpha;

        if (entry)
        {
            loaded_best_section = entry_best_section;

            if (loaded_best_section != 15u)
            {
#ifdef TRACK_CUTOFF_STATS
                best_section_cutoff_tracker[depth][loaded_best_section]++;
                best_section_cutoff_tracker_all[depth][loaded_best_section]++;
#endif

                has_jumped = true;
                goto *movesect_jmp_buf[loaded_best_section];
            }
        }
    }

__maximize_begin:
#ifdef TRACK_CUTOFF_STATS
    if (has_jumped)
        best_section_cutoff_tracker[depth][loaded_best_section]--;
#endif
    has_jumped = false;

#define MOVE_SECTION 15u
    if (__builtin_expect((fir_link_mask & 8ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation + (128 << args.fields.fir_link) - 7;

        RNAB_ASSUME((is_fir_mask & 8ULL) && (is_link_mask & 8ULL));

        BRANCH_ENTER_MAX("deposit close 1");
        const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_min, minimax_min_term, depth, alpha, beta, new_eval,
                                                       is_fir_mask & ~8ULL, is_sec_mask, is_link_mask & ~8ULL, is_boosted_mask & ~8ULL,
                                                       (const extra_args_t){.raw = args.raw + 65536ULL});
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }

    if (__builtin_expect((fir_link_mask & 16ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation + (128 << args.fields.fir_link) - 7;

        RNAB_ASSUME((is_fir_mask & 16ULL) && (is_link_mask & 16ULL));

        BRANCH_ENTER_MAX("deposit close 2");
        const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_min, minimax_min_term, depth, alpha, beta, new_eval,
                                                       is_fir_mask & ~16ULL, is_sec_mask, is_link_mask & ~16ULL, is_boosted_mask & ~16ULL,
                                                       (const extra_args_t){.raw = args.raw + 65536ULL});
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }

    if (__builtin_expect((((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 24ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation + (128 << args.fields.fir_link) - (__builtin_clzll(cur_boosted_mask) >> 3);

        RNAB_ASSUME((is_fir_mask & cur_boosted_mask) && (is_link_mask & cur_boosted_mask) && (is_boosted_mask & cur_boosted_mask));

        BRANCH_ENTER_MAX("deposit far");
        const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_min, minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask ^ cur_boosted_mask, is_sec_mask, is_link_mask ^ cur_boosted_mask, is_boosted_mask ^ cur_boosted_mask,
                                                       (const extra_args_t){.raw = args.raw + 65536ULL});
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }

    if (cur_boosted_mask)
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const int cur_pos_bit = __builtin_ctzll(cur_pos_bitboard) >> 3;

        if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }
    }
#undef MOVE_SECTION

    MOVE_SECTION_BEGIN(0);
    if (cur_boosted_mask && virus_can_be_captured)
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const int cur_pos_bit = __builtin_ctzll(cur_pos_bitboard) >> 3;
        const uint64_t legal_mask = (~enemy_firewall_mask) & sec_virus_mask;

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) != 0 && (sec_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(1);
    if (cur_boosted_mask == 0)
    {
        uint64_t temp = fir_virus_mask;

        while (temp)
        {
            const uint64_t pos = extract_lsb(temp); // back -> front

            BRANCH_ENTER_MAX("boost virus");
            const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_min, minimax_min_term, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
            BRANCH_EXIT_MAX();
            best_section = (reschild > score) ? MOVE_SECTION : best_section;
            score = (reschild > score) ? reschild : score;
            alpha = (reschild > alpha) ? reschild : alpha;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MAX();
            }

            clear_lowest_set_bit(temp, pos);
        }
    }
    else
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const int cur_pos_bit = __builtin_ctzll(cur_pos_bitboard) >> 3;
        const uint64_t legal_mask = free_mask;

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_sec_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_fir_mask) == 0);

            PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(2);
    uint64_t unboosted_cards_mask = fir_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = extract_lsb(unboosted_cards_mask);
        const uint64_t legal_mask = (is_sec_mask & (~enemy_firewall_mask));
        const int cur_pos_bit = __builtin_ctzll(cur_pos_bitboard) >> 3;

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(3);
    uint64_t unboosted_cards_mask = fir_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = extract_lsb(unboosted_cards_mask);
        const uint64_t legal_mask = (is_sec_mask & (~enemy_firewall_mask));
        const int cur_pos_bit = __builtin_ctzll(cur_pos_bitboard) >> 3;

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_sec_mask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(4);
    uint64_t unboosted_cards_mask = fir_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = extract_lsb(unboosted_cards_mask);
        const int cur_pos_bit = __builtin_ctzll(cur_pos_bitboard) >> 3;

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(5);
    uint64_t unboosted_cards_mask = fir_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = extract_lsb(unboosted_cards_mask);
        const int cur_pos_bit = __builtin_ctzll(cur_pos_bitboard) >> 3;

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(6);
    if (args.fields.firewall_fir)
    {
        const int32_t new_eval = evaluation + 8;

        BRANCH_ENTER_MAX("un-firewall");
        int32_t reschild;
        if (depth > 1)
        {
            reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                    (const extra_args_t){.raw = (args.raw & 18446744073709551360ULL)});
            if (reschild > alpha && beta > alpha + 1)
                reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = (args.raw & 18446744073709551360ULL)});
        }
        else
            reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                    (const extra_args_t){.raw = (args.raw & 18446744073709551360ULL)});
        BRANCH_EXIT_MAX();

        best_section = (reschild > score) ? MOVE_SECTION : best_section;
        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }
    else
    {
        uint64_t temp = fir_link_mask & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = __builtin_ctzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            const int32_t new_eval = evaluation - 8;

            BRANCH_ENTER_MAX("firewall link");
            int32_t reschild;
            if (depth > 1)
            {
                reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});
                if (reschild > alpha && beta > alpha + 1)
                    reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});
            }
            else
                reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});
            BRANCH_EXIT_MAX();

            best_section = (reschild > score) ? MOVE_SECTION : best_section;
            score = (reschild > score) ? reschild : score;
            alpha = (reschild > alpha) ? reschild : alpha;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MAX();
            }

            clear_lowest_set_bit(temp, pos);
        }
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(7);
    if (args.fields.firewall_fir == 0)
    {
        const uint64_t enemy_boosted_mask = is_sec_mask & is_boosted_mask;

        uint64_t temp = ((fir_virus_mask & cur_boosted_mask) | (((cur_boosted_mask >> 8) | (cur_boosted_mask >> 16) | ((cur_boosted_mask & CAN_MOVE_LEFT) >> 9) | ((cur_boosted_mask & CAN_MOVE_RIGHT) >> 7) | (enemy_boosted_mask << 8)) & free_mask)) & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = __builtin_ctzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            const int32_t new_eval = evaluation - 8;

            BRANCH_ENTER_MAX("firewall compl");
            int32_t reschild;
            if (depth > 1)
            {
                reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});
                if (reschild > alpha && beta > alpha + 1)
                    reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});
            }
            else
                reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw | 1 | (bit_pos << 1)});
            BRANCH_EXIT_MAX();

            best_section = (reschild > score) ? MOVE_SECTION : best_section;
            score = (reschild > score) ? reschild : score;
            alpha = (reschild > alpha) ? reschild : alpha;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MAX();
            }

            clear_lowest_set_bit(temp, pos);
        }
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(8);
    if (args.fields.is_swap_available_fir)
    {
        uint64_t link_mask = fir_link_mask;

        while (link_mask)
        {
            const uint64_t link_pos = extract_lsb(link_mask); // front -> back

            uint64_t virus_mask = fir_virus_mask;

            while (virus_mask)
            {
                const uint64_t virus_pos = extract_lsb(virus_mask); // front -> back

                const int32_t new_eval = evaluation - 256;

                BRANCH_ENTER_MAX("swap");
                int32_t reschild;
                if (depth > 1)
                {
                    reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, new_eval, is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw ^ 281474976710656ULL});
                    if (reschild > alpha && beta > alpha + 1)
                        reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask,
                                                (const extra_args_t){.raw = args.raw ^ 281474976710656ULL});
                }
                else
                    reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw ^ 281474976710656ULL});
                BRANCH_EXIT_MAX();

                best_section = (reschild > score) ? MOVE_SECTION : best_section;
                score = (reschild > score) ? reschild : score;
                alpha = (reschild > alpha) ? reschild : alpha;

                if (beta <= alpha)
                {
                    TRACK_ENTRY_MAX();
                }

                clear_lowest_set_bit(virus_mask, virus_pos);
            }

            clear_lowest_set_bit(link_mask, link_pos);
        }
    }
    MOVE_SECTION_END(__maximize_begin);

    MOVE_SECTION_BEGIN(9);
    if (cur_boosted_mask == 0)
    {
        uint64_t temp = fir_link_mask;

        while (temp)
        {
            const uint64_t pos = extract_lsb(temp); // back -> front

            BRANCH_ENTER_MAX("boost link");
            int32_t reschild;
            if (depth > 1)
            {
                reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
                if (reschild > alpha && beta > alpha + 1)
                    reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
            }
            else
                reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
            BRANCH_EXIT_MAX();

            best_section = (reschild > score) ? MOVE_SECTION : best_section;
            score = (reschild > score) ? reschild : score;
            alpha = (reschild > alpha) ? reschild : alpha;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MAX();
            }

            clear_lowest_set_bit(temp, pos);
        }
    }
    else
    {
        BRANCH_ENTER_MAX("un-boost");
        int32_t reschild;
        if (depth > 1)
        {
            reschild = MINIMAX_CALL(minimax_min, depth, alpha, alpha + 1, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
            if (reschild > alpha && beta > alpha + 1)
                reschild = MINIMAX_CALL(minimax_min, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
        }
        else
            reschild = MINIMAX_CALL(minimax_min_term, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
        BRANCH_EXIT_MAX();

        best_section = (reschild > score) ? MOVE_SECTION : best_section;
        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }
    MOVE_SECTION_END(__maximize_begin);

    if (depth > MIN_CACHE_DEPTH)
    {
        *(uint64_t *__restrict__)((depth >= (*((uint8_t *)bucket + 5) >> 3)) ? &bucket->depth_preferred : &bucket->scratch) = (hash) | ((uint64_t)((score <= alphabeg) ? TT_UPPERBOUND : TT_EXACT) << (ENTRY_HASH_SIZE)) | ((uint64_t)((uint32_t)depth) << (ENTRY_HASH_SIZE + 2)) | ((uint64_t)best_section << (ENTRY_HASH_SIZE + 2 + 5)) | ((uint64_t)(uint32_t)score << 52);
    }

    return score;

track_max:
    if (depth > MIN_CACHE_DEPTH)
    {
        *(uint64_t *__restrict__)((depth >= (*((uint8_t *)bucket + 5) >> 3)) ? &bucket->depth_preferred : &bucket->scratch) = (hash) | ((uint64_t)TT_LOWERBOUND << (ENTRY_HASH_SIZE)) | ((uint64_t)((uint32_t)depth) << (ENTRY_HASH_SIZE + 2)) | ((uint64_t)best_section << (ENTRY_HASH_SIZE + 2 + 5)) | ((uint64_t)(uint32_t)score << 52);
    }

    return score;
}

RNAB_HOT_FUNCTION __attribute__((aligned(FUNC_ALIGN))) __attribute__((hot)) static int32_t MINIMAX_FUNC(minimax_min)
{
    // Checking flag in one of those is sufficient
#ifdef BRANCH_DEBUG
    ++rec_counter;
#endif
#ifdef TRACK_CUTOFF_STATS
    ply_stats[depth].nodes++;
#endif
    MINIMAX_UNPACK();

    RNAB_ASSUME(depth > 0);
    RNAB_ASSUME(args.fields.fir_link <= 3);
    RNAB_ASSUME(args.fields.fir_virus <= 3);
    RNAB_ASSUME(args.fields.sec_link <= 3);
    RNAB_ASSUME(args.fields.sec_virus <= 3);

    const uint64_t fir_link_mask = is_link_mask & is_fir_mask;
    const uint64_t fir_virus_mask = is_fir_mask ^ fir_link_mask;
    const uint64_t sec_link_mask = is_link_mask ^ fir_link_mask;
    const uint64_t sec_virus_mask = is_sec_mask ^ sec_link_mask;

    const uint64_t cur_boosted_mask = is_boosted_mask & is_sec_mask;
    const uint64_t enemy_firewall_mask = (uint64_t)(args.fields.firewall_fir & 1) << (args.fields.firewall_fir >> 1);
    const uint64_t free_mask = ~(is_fir_mask | is_sec_mask | enemy_firewall_mask);
    const uint64_t boosted_link = cur_boosted_mask & sec_link_mask;
    const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask;
    const bool virus_can_be_captured = args.fields.sec_virus < 3;

    const tt_bucket_t *__restrict__ bucket;
    uint64_t hash;
    int32_t betabeg;
    uint32_t loaded_best_section = 15u;
    uint32_t best_section = 15u;
    bool has_jumped = false;

    int32_t score = MAX;

    if (args.fields.sec_link == 3) // fast path if we are about to win
    {
        // there is either a link at an exit square or a boosted link which can reach it
        if ((sec_link_mask | (((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask)) & 1729382256910270464ULL)
            return -SCORE_TERMINAL_BASE - depth;
        if (links_masked_out)
        {
            // check if any of the cards can simply reach unprotected enemy link in one move
#ifdef __AVX2__
            __m256i tgt = _mm256_set1_epi64x(links_masked_out);

            __m256i shuffled = _mm256_and_si256(_mm256_blend_epi32(_mm256_or_si256(_mm256_srli_si256(tgt, 9), _mm256_srli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, CAN_MOVE_RIGHT, 0, 0)), 1)), _mm256_or_si256(_mm256_slli_si256(tgt, 9), _mm256_slli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, 0, CAN_MOVE_LEFT, 0)), 1)), 0b11110000), _mm256_set1_epi64x(is_sec_mask));

            if (!_mm256_testz_si256(shuffled, shuffled))
#else
            if (((links_masked_out >> 8) | (links_masked_out << 8) | ((links_masked_out & CAN_MOVE_RIGHT) >> 1) | ((links_masked_out & CAN_MOVE_LEFT) << 1)) & is_sec_mask)
#endif
                return -SCORE_TERMINAL_BASE - depth;

            // now let's check if we can reach any of the unprotected links with a boosted card if there is one
            if (cur_boosted_mask)
            {
                // it looks like SIMD check is almost 3 times faster than the scalar one
#ifdef __AVX2__
                /*
                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask << 7)  & ((free_mask >> 1) | (free_mask << 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask << 9)  & ((free_mask << 1) | (free_mask << 8))
                links_masked_out & -1                      & (cur_boosted_mask << 16) & (0                | (free_mask << 8))
                links_masked_out & CAN_MOVE_DOUBLE_RIGHT   & (cur_boosted_mask << 2)  & ((free_mask << 1) | 0               )


                links_masked_out & CAN_MOVE_LEFT           & (cur_boosted_mask >> 9)  & ((free_mask >> 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_RIGHT          & (cur_boosted_mask >> 7)  & ((free_mask << 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_DOUBLE_LEFT    & (cur_boosted_mask >> 2)  & ((free_mask >> 1) | 0               )
                links_masked_out & -1                      & (cur_boosted_mask >> 16) & (0                | (free_mask >> 8))
                */

                const __m256i zero_mask = _mm256_setzero_si256();

                __m256i fm = _mm256_set1_epi64x(free_mask);
                __m256i cm = _mm256_set1_epi64x(cur_boosted_mask);
                __m256i shift_comb = _mm256_blend_epi32(_mm256_slli_epi64(fm, 1), _mm256_srli_epi64(fm, 1), 0b00110011);

                if (!_mm256_testz_si256(_mm256_or_si256(_mm256_and_si256(_mm256_and_si256(_mm256_sllv_epi64(cm, _mm256_setr_epi64x(7, 9, 16, 2)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(-1, 0, 1, 2, 3, 4, 5, 6, -1, 8, 9, 10, 11, 12, 13, 14, -1, 16, 17, 18, 19, 20, 21, 22, -1, -1, -1, -1, -1, -1, -1, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b11001111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, -1, CAN_MOVE_DOUBLE_RIGHT)), _mm256_and_si256(_mm256_and_si256(_mm256_srlv_epi64(cm, _mm256_setr_epi64x(9, 7, 2, 16)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(1, 2, 3, 4, 5, 6, 7, -1, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, 26, 27, 28, 29, 30, 31, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b00111111))), _mm256_setr_epi64x(CAN_MOVE_LEFT, CAN_MOVE_RIGHT, CAN_MOVE_DOUBLE_LEFT, -1))), tgt))
#else
                if (links_masked_out & (((free_mask << 8) & (cur_boosted_mask << 16)) |                                      // down and not blocked
                                        ((free_mask >> 8) & (cur_boosted_mask >> 16)) |                                      // up and not blocked
                                        (CAN_MOVE_DOUBLE_RIGHT & (cur_boosted_mask << 2) & (free_mask << 1)) |               // right and not blocked
                                        (CAN_MOVE_DOUBLE_LEFT & (cur_boosted_mask >> 2) & (free_mask >> 1)) |                // left and not blocked
                                        (CAN_MOVE_LEFT & (cur_boosted_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) |  // down left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask << 9) & ((free_mask << 1) | (free_mask << 8))) | // down right
                                        (CAN_MOVE_LEFT & (cur_boosted_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) |  // up left
                                        (CAN_MOVE_RIGHT & (cur_boosted_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))))) // up right
#endif
                    return -SCORE_TERMINAL_BASE - depth;
            }
        }
    }

    --depth;

    static const void *movesect_jmp_buf[] = {
        &&__movesect_0, &&__movesect_1, &&__movesect_2, &&__movesect_3, &&__movesect_4,
        &&__movesect_5, &&__movesect_6, &&__movesect_7, &&__movesect_8, &&__movesect_9};

    // check if the move is cached
    if (depth > MIN_CACHE_DEPTH)
    {
#ifdef RNAB_REVCACHE
        if (debug_revhash)
        {
            const field_t cur = (field_t){is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, args};
            const field_t rev = reverse_field(&cur);
            hash = rapidhashNano(rev.is_fir_mask, rev.is_sec_mask, rev.is_link_mask, rev.is_boosted_mask, rev.args.raw, 0);
        }
        else
        {
            hash = rapidhashNano(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, args.raw, 0);
        }
#else
        hash = rapidhashNano(is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask, args.raw, 0);
#endif
        bucket = &tt_sec[hash & (TABLE_SIZE - 1)];
        hash = (hash >> TABLE_SIZE_BITS) & ENTRY_HASH_MASK;

        const uint64_t depth_preferred = bucket->depth_preferred;
        const uint64_t scratch = bucket->scratch;

        const uint64_t entry = ((depth_preferred & ENTRY_HASH_MASK) == hash) ? depth_preferred : (((scratch & ENTRY_HASH_MASK) == hash) ? scratch : 0ULL);

        // [                                                       ][][    ] [   ][              ]
        // [00000000] [00000000] [00000000] [00000000] [00000000] [00000000] [00000000] [00000000]

        const uint32_t entry_flag = (entry >> (ENTRY_HASH_SIZE + 0)) & 3;
        const uint32_t entry_depth = (entry >> (ENTRY_HASH_SIZE + 2)) & 31;
        const uint32_t entry_best_section = (entry >> (ENTRY_HASH_SIZE + 2 + 5)) & 15;
        const int32_t entry_eval = (int32_t)((int64_t)entry >> 52);

#ifdef TRACK_CUTOFF_STATS
        ply_stats[depth].tt_hits++;
        ply_stats[depth].tt_cutoffs++;
#endif

        if (entry_depth >= depth) // inherently wrong but it works
        {
            if (entry_flag == TT_EXACT)
            {
                return entry_eval;
            }
            else if (entry_flag == TT_LOWERBOUND)
            {
                if (entry_eval >= beta)
                    return entry_eval;
                alpha = (entry_eval > alpha) ? entry_eval : alpha;
            }
            else if (entry_flag == TT_UPPERBOUND)
            {
                if (entry_eval <= alpha)
                    return entry_eval;
                beta = (entry_eval < beta) ? entry_eval : beta;
            }
        }

#ifdef TRACK_CUTOFF_STATS
        ply_stats[depth].tt_cutoffs--;
#endif

        betabeg = beta;

        if (entry)
        {
            loaded_best_section = entry_best_section;

            if (loaded_best_section != 15u)
            {
#ifdef TRACK_CUTOFF_STATS
                best_section_cutoff_tracker[depth][loaded_best_section]++;
                best_section_cutoff_tracker_all[depth][loaded_best_section]++;
#endif

                has_jumped = true;
                goto *movesect_jmp_buf[loaded_best_section];
            }
        }
    }

__minimize_begin:
#ifdef TRACK_CUTOFF_STATS
    if (has_jumped)
        best_section_cutoff_tracker[depth][loaded_best_section]--;
#endif
    has_jumped = false;

#define MOVE_SECTION 15u
    if (__builtin_expect((sec_link_mask & 1152921504606846976ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation - (128 << args.fields.sec_link) + 7;

        RNAB_ASSUME((is_sec_mask & 1152921504606846976ULL) && (is_link_mask & 1152921504606846976ULL));

        BRANCH_ENTER_MIN("deposit close");
        const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_max, minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask & ~1152921504606846976ULL, is_link_mask & ~1152921504606846976ULL, is_boosted_mask & ~1152921504606846976ULL,
                                                       (const extra_args_t){.raw = args.raw + 16777216ULL});
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }

    if (__builtin_expect((sec_link_mask & 576460752303423488ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation - (128 << args.fields.sec_link) + 7;

        RNAB_ASSUME((is_sec_mask & 576460752303423488ULL) && (is_link_mask & 576460752303423488ULL));

        BRANCH_ENTER_MIN("deposit close");
        const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_max, minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask & ~576460752303423488ULL, is_link_mask & ~576460752303423488ULL, is_boosted_mask & ~576460752303423488ULL,
                                                       (const extra_args_t){.raw = args.raw + 16777216ULL});
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }

    if (__builtin_expect((((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 1729382256910270464ULL) != 0, 0))
    {
        const int32_t new_eval = evaluation - (128 << args.fields.sec_link) + (__builtin_ctzll(cur_boosted_mask) >> 3);

        RNAB_ASSUME((is_sec_mask & cur_boosted_mask) && (is_link_mask & cur_boosted_mask) && (is_boosted_mask & cur_boosted_mask));

        BRANCH_ENTER_MAX("deposit far");
        const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_max, minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask ^ cur_boosted_mask, is_link_mask ^ cur_boosted_mask, is_boosted_mask ^ cur_boosted_mask,
                                                       (const extra_args_t){.raw = args.raw + 16777216ULL});
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }

    if (cur_boosted_mask)
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const int cur_pos_bit = (__builtin_clzll(cur_pos_bitboard) >> 3);

        if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }
    }
#undef MOVE_SECTION

    MOVE_SECTION_BEGIN(0);
    if (cur_boosted_mask && virus_can_be_captured)
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const int cur_pos_bit = (__builtin_clzll(cur_pos_bitboard) >> 3);
        const uint64_t legal_mask = (~enemy_firewall_mask) & fir_virus_mask;

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) != 0 && (fir_link_mask & new_pos_bitboard) == 0 && virus_can_be_captured);

            PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(1);
    if (cur_boosted_mask == 0)
    {
        uint64_t temp = sec_virus_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            BRANCH_ENTER_MIN("boost virus");
            const int32_t reschild = BRANCHED_MINIMAX_CALL(depth > 1, minimax_max, minimax_max_term, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
            BRANCH_EXIT_MIN();

            best_section = (reschild < score) ? MOVE_SECTION : best_section;
            score = (reschild < score) ? reschild : score;
            beta = (reschild < beta) ? reschild : beta;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MIN();
            }

            clear_highest_set_bit(temp, pos);
        }
    }
    else
    {
        const uint64_t cur_pos_bitboard = cur_boosted_mask;
        const int cur_pos_bit = (__builtin_clzll(cur_pos_bitboard) >> 3);
        const uint64_t legal_mask = free_mask;

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & is_sec_mask) == 0);

            PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(2);
    uint64_t unboosted_cards_mask = sec_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
        const uint64_t legal_mask = (is_fir_mask & (~enemy_firewall_mask));
        const int cur_pos_bit = (__builtin_clzll(cur_pos_bitboard) >> 3);

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(3);
    uint64_t unboosted_cards_mask = sec_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
        const uint64_t legal_mask = (is_fir_mask & (~enemy_firewall_mask));
        const int cur_pos_bit = (__builtin_clzll(cur_pos_bitboard) >> 3);

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(is_fir_mask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(4);
    uint64_t unboosted_cards_mask = sec_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
        const int cur_pos_bit = (__builtin_clzll(cur_pos_bitboard) >> 3);

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(5);
    uint64_t unboosted_cards_mask = sec_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
        const int cur_pos_bit = (__builtin_clzll(cur_pos_bitboard) >> 3);

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((is_fir_mask & new_pos_bitboard) == 0 && (is_sec_mask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(6);
    if (args.fields.firewall_sec)
    {
        const int32_t new_eval = evaluation - 8;

        BRANCH_ENTER_MIN("un-firewall");
        int32_t reschild;
        if (depth > 1)
        {
            reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                    (const extra_args_t){.raw = (args.raw & 18446744073709486335ULL)});
            if (reschild < beta && alpha < beta - 1)
                reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = (args.raw & 18446744073709486335ULL)});
        }
        else
            reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                    (const extra_args_t){.raw = (args.raw & 18446744073709486335ULL)});
        BRANCH_EXIT_MIN();

        best_section = (reschild < score) ? MOVE_SECTION : best_section;
        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }
    else
    {
        uint64_t temp = sec_link_mask & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = 63 - __builtin_clzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            const int32_t new_eval = evaluation + 8;

            BRANCH_ENTER_MIN("firewall link");

            int32_t reschild;
            if (depth > 1)
            {
                reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw + 256 + (bit_pos << 9)});
                if (reschild < beta && alpha < beta - 1)
                    reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw + 256 + (bit_pos << 9)});
            }
            else
                reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw + 256 + (bit_pos << 9)});
            BRANCH_EXIT_MIN();

            best_section = (reschild < score) ? MOVE_SECTION : best_section;
            score = (reschild < score) ? reschild : score;
            beta = (reschild < beta) ? reschild : beta;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MIN();
            }

            clear_highest_set_bit(temp, pos);
        }
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(7);
    if (args.fields.firewall_sec == 0)
    {
        const uint64_t enemy_boosted_mask = is_fir_mask & is_boosted_mask;

        uint64_t temp = ((sec_virus_mask & cur_boosted_mask) | (((cur_boosted_mask << 8) | (cur_boosted_mask << 16) | ((cur_boosted_mask & CAN_MOVE_LEFT) << 7) | ((cur_boosted_mask & CAN_MOVE_RIGHT) << 9) | (enemy_boosted_mask >> 8)) & free_mask)) & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = 63 - __builtin_clzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            const int32_t new_eval = evaluation + 8;

            BRANCH_ENTER_MIN("firewall compl");

            int32_t reschild;
            if (depth > 1)
            {
                reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw + 256 + (bit_pos << 9)});
                if (reschild < beta && alpha < beta - 1)
                    reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw + 256 + (bit_pos << 9)});
            }
            else
                reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask,
                                        (const extra_args_t){.raw = args.raw + 256 + (bit_pos << 9)});
            BRANCH_EXIT_MIN();

            best_section = (reschild < score) ? MOVE_SECTION : best_section;
            score = (reschild < score) ? reschild : score;
            beta = (reschild < beta) ? reschild : beta;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MIN();
            }

            clear_highest_set_bit(temp, pos);
        }
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(8);
    if (args.fields.is_swap_available_sec)
    {
        uint64_t link_mask = sec_link_mask;

        while (link_mask)
        {
            const uint64_t link_pos = (1ULL << (63 - __builtin_clzll(link_mask))); // front -> back

            uint64_t virus_mask = sec_virus_mask;

            while (virus_mask)
            {
                const uint64_t virus_pos = (1ULL << (63 - __builtin_clzll(virus_mask))); // front -> back

                const int32_t new_eval = evaluation + 256;

                BRANCH_ENTER_MIN("swap");
                int32_t reschild;
                if (depth > 1)
                {
                    reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw ^ 72057594037927936ULL});
                    if (reschild < beta && alpha < beta - 1)
                        reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask,
                                                (const extra_args_t){.raw = args.raw ^ 72057594037927936ULL});
                }
                else
                    reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, new_eval, is_fir_mask, is_sec_mask, is_link_mask ^ (link_pos | virus_pos), is_boosted_mask,
                                            (const extra_args_t){.raw = args.raw ^ 72057594037927936ULL});
                BRANCH_EXIT_MIN();

                best_section = (reschild < score) ? MOVE_SECTION : best_section;
                score = (reschild < score) ? reschild : score;
                beta = (reschild < beta) ? reschild : beta;

                if (beta <= alpha)
                {
                    TRACK_ENTRY_MIN();
                }

                clear_highest_set_bit(virus_mask, virus_pos);
            }

            clear_highest_set_bit(link_mask, link_pos);
        }
    }
    MOVE_SECTION_END(__minimize_begin);

    MOVE_SECTION_BEGIN(9);
    if (cur_boosted_mask == 0)
    {
        uint64_t temp = sec_link_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            BRANCH_ENTER_MIN("boost link");
            int32_t reschild;
            if (depth > 1)
            {
                reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
                if (reschild < beta && alpha < beta - 1)
                    reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
            }
            else
                reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask | pos, args);
            BRANCH_EXIT_MIN();

            best_section = (reschild < score) ? MOVE_SECTION : best_section;
            score = (reschild < score) ? reschild : score;
            beta = (reschild < beta) ? reschild : beta;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MIN();
            }

            clear_highest_set_bit(temp, pos);
        }
    }
    else
    {
        BRANCH_ENTER_MIN("un-boost");
        int32_t reschild;
        if (depth > 1)
        {
            reschild = MINIMAX_CALL(minimax_max, depth, beta - 1, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
            if (reschild < beta && alpha < beta - 1)
                reschild = MINIMAX_CALL(minimax_max, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
        }
        else
            reschild = MINIMAX_CALL(minimax_max_term, depth, alpha, beta, evaluation, is_fir_mask, is_sec_mask, is_link_mask, is_boosted_mask ^ cur_boosted_mask, args);
        BRANCH_EXIT_MIN();

        best_section = (reschild < score) ? MOVE_SECTION : best_section;
        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }
    MOVE_SECTION_END(__minimize_begin);

    // [                                                       ][][    ] [   ][              ]
    // [00000000] [00000000] [00000000] [00000000] [00000000] [00000000] [00000000] [00000000]

    if (depth > MIN_CACHE_DEPTH)
    {
        *(uint64_t *__restrict__)((depth >= (*((uint8_t *)bucket + 5) >> 3)) ? &bucket->depth_preferred : &bucket->scratch) = (hash) | ((uint64_t)(((score >= betabeg) ? TT_LOWERBOUND : TT_EXACT)) << (ENTRY_HASH_SIZE)) | ((uint64_t)((uint32_t)depth) << (ENTRY_HASH_SIZE + 2)) | ((uint64_t)best_section << (ENTRY_HASH_SIZE + 2 + 5)) | ((uint64_t)(uint32_t)score << 52);
    }
    return score;

track_min:
    if (depth > MIN_CACHE_DEPTH)
    {
        *(uint64_t *__restrict__)((depth >= (*((uint8_t *)bucket + 5) >> 3)) ? &bucket->depth_preferred : &bucket->scratch) = (hash) | ((uint64_t)TT_UPPERBOUND << (ENTRY_HASH_SIZE)) | ((uint64_t)((uint32_t)depth) << (ENTRY_HASH_SIZE + 2)) | ((uint64_t)best_section << (ENTRY_HASH_SIZE + 2 + 5)) | ((uint64_t)(uint32_t)score << 52);
    }
    return score;
}

END_BRANCH_TRACKING();

typedef struct
{
    int16_t move_eval;
    uint8_t move_id;
    bool is_exact; // false = upper bound from failed null-window
} move_scores_wrapper;

STATIC_BSS move_scores_wrapper move_scores[MAX_MOVES] __attribute__((aligned(64)));
STATIC_BSS field_t possible_moves_buf[MAX_MOVES];
STATIC_BSS int32_t possible_moves_buf_moves_count;

static void minimax_iteration_main_st_max(const int32_t max_depth, minimax_main_result_t *__restrict__ ret)
{
    RNAB_ASSUME(possible_moves_buf_moves_count > 0);

    STATIC_BSS TIME_TYPE start, start_it, stop, global_start;
    STATIC_BSS field_t *best_field;
    STATIC_BSS field_t *pos;

    debug_printf("minimax_iteration_main_st_max entry\n");

    debug_get_time(global_start);

    for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i)
        if (possible_moves_buf[i].args.fields.fir_link > 3)
        {
            *ret = (minimax_main_result_t){.best_field = possible_moves_buf[i], .evaluation = SCORE_TERMINAL_BASE + max_depth};
            return;
        }

    int32_t prev_alpha = MIN;

    for (int32_t current_depth = 4; current_depth <= max_depth; current_depth += 2)
    {
#ifdef TRACK_CUTOFF_STATS
        for (int i = 0; i < current_depth; ++i)
        {
            for (int k = 0; k < 16; ++k)
            {
                best_section_cutoff_tracker[i][k] = 0;
                best_section_cutoff_tracker_all[i][k] = 0;
            }

            ply_stats[i].nodes = 0;
            ply_stats[i].tt_cutoffs = 0;
            ply_stats[i].tt_hits = 0;
        }
#endif

        int32_t best_move_idx = -1;

        uint64_t cur_rec_count = rec_counter;
        debug_get_time(start);

        if (current_depth > 4)
        {
            for (int32_t i = 1; i < possible_moves_buf_moves_count; ++i)
            {
                move_scores_wrapper key = move_scores[i];
                int32_t j = i - 1;
                while (j >= 0)
                {
                    move_scores_wrapper *prev = &move_scores[j];
                    if (prev->move_eval > key.move_eval)
                        break;
                    if (prev->move_eval == key.move_eval)
                    {
                        if (prev->is_exact && !key.is_exact)
                            break; // exact always wins over non-exact
                        if (prev->is_exact == key.is_exact &&
                            prev->move_id <= key.move_id)
                            break; // stable by move_id only among equals
                    }
                    move_scores[j + 1] = *prev;
                    --j;
                }
                move_scores[j + 1] = key;
            }
        }

        int32_t iteration_alpha = prev_alpha - 5;
        int32_t iteration_beta = (current_depth == 4) ? MAX : (prev_alpha + 5);

        debug_printf("Window guess: [%d ~ %d]\n", iteration_alpha, iteration_beta);

        do
        {
            move_scores_wrapper *__restrict__ move_ptr = move_scores;

            for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i, ++move_ptr)
            {
                int32_t move_idx = move_ptr->move_id;
                pos = &possible_moves_buf[move_idx];

                debug_get_time(start_it);

                int32_t childres;
                if (i == 0) // very likely for the score to be higher
                {
                    childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                    debug_get_time(stop);

                    move_ptr->is_exact = true;

                    // debug_printf(FG_BLUE "[f %d: %d -> %d : %lld] " RESET, move_idx, iteration_alpha, childres, get_time_diff_millis(stop, start_it));
                }
                else
                {
                    childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_alpha + 1, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                    debug_get_time(stop);

                    // debug_printf(FG_GREEN "[s %d: %d -> %d : %lld] " RESET, move_idx, iteration_alpha, childres, get_time_diff_millis(stop, start_it));

                    if (childres > iteration_alpha)
                    {
                        childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                        debug_get_time(stop);

                        move_ptr->is_exact = true;

                        // debug_printf(FG_RED "[r %d: %d -> %d : %lld] " RESET, move_idx, iteration_alpha, childres, get_time_diff_millis(stop, start_it));
                    }
                    else
                    {
                        move_ptr->is_exact = false;
                    }
                }

                if (should_exit)
                {
                    if (best_move_idx != -1)
                    {
                        // at least one move completed and beat the lower bound
                        // best_field and iteration_alpha already reflect the best found so far
                        *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_alpha};
                    }

                    debug_printf("Search order: ");
                    for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                        debug_printf("%d, ", move_scores[u].move_id);
                    debug_printf("\n");
                    debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d\n", i, possible_moves_buf_moves_count, best_move_idx, iteration_alpha);

                    return;
                }

                move_ptr->move_eval = childres;

                if (childres > iteration_alpha)
                {
                    best_field = pos;
                    iteration_alpha = childres;
                    best_move_idx = move_idx;
                }

                if (iteration_beta <= iteration_alpha)
                {
                    debug_printf("cutoff!!!!\n");
                    break;
                }
            }
        } while (0);

        bool guessed_incorrectly = (iteration_alpha <= prev_alpha - 5) || (iteration_alpha >= iteration_beta);

        iteration_beta = (iteration_alpha >= iteration_beta) ? MAX : iteration_beta;
        iteration_alpha = (iteration_alpha <= prev_alpha - 5) ? MIN : iteration_alpha;

        if (guessed_incorrectly)
        {
            debug_printf("Guess failed, updated window: [%d ~ %d]\n", iteration_alpha, iteration_beta);

            best_move_idx = -1;

            do
            {
                move_scores_wrapper *__restrict__ move_ptr = move_scores;

                for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i, ++move_ptr)
                {
                    debug_get_time(start_it);

                    int32_t move_idx = move_ptr->move_id;

                    pos = &possible_moves_buf[move_idx];

                    int32_t childres;
                    if (i == 0)
                    {
                        childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);

                        debug_get_time(stop);

                        move_ptr->is_exact = true;
                        // debug_printf(FG_BLUE "[f %d: %d -> %d : %" PRId64 "] " RESET, move_idx, iteration_alpha, childres, get_time_diff_millis(stop, start_it));
                    }
                    else
                    {
                        childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_alpha + 1, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                        debug_get_time(stop);
                        // debug_printf(FG_GREEN "[s %d: %d -> %d : %" PRId64 "] " RESET, move_idx, iteration_alpha, childres, get_time_diff_millis(stop, start_it));
                        if (childres > iteration_alpha)
                        {
                            childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                            debug_get_time(stop);

                            move_ptr->is_exact = true;
                            // debug_printf(FG_RED "[r %d: %d -> %d : %" PRId64 "] " RESET, move_idx, iteration_alpha, childres, get_time_diff_millis(stop, start_it));
                        }
                        else
                        {
                            move_ptr->is_exact = false;
                        }
                    }

                    if (should_exit)
                    {
                        if (best_move_idx != -1)
                        {
                            // at least one move completed and beat the lower bound
                            // best_field and iteration_alpha already reflect the best found so far
                            *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_alpha};
                        }

                        debug_printf("Search order: ");
                        for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                            debug_printf("%d, ", move_scores[u].move_id);
                        debug_printf("\n");
                        debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d\n", i, possible_moves_buf_moves_count, best_move_idx, iteration_alpha);

                        return;
                    }

                    move_ptr->move_eval = childres;

                    if (childres > iteration_alpha)
                    {
                        best_field = pos;
                        iteration_alpha = childres;
                        best_move_idx = move_idx;
                    }
                }
            } while (0);
        }

#ifdef TRACK_CUTOFF_STATS
        for (int i = 0; i < 16; ++i)
        {
            _printf("%02d        ", i);
        }
        for (int i = 0; i < current_depth; ++i)
        {
            for (int k = 0; k < 16; ++k)
            {
                int64_t pct_whole = 0, pct_frac = 0;
                if (best_section_cutoff_tracker_all[i][k] > 0)
                {
                    int64_t pct_int = best_section_cutoff_tracker[i][k] * 10000000LL / best_section_cutoff_tracker_all[i][k];
                    pct_whole = pct_int / 100000;
                    pct_frac = pct_int % 100000;

                    _printf("%03lld.%05lld ", pct_whole, pct_frac);
                }
                else
                {
                    _printf("          ");
                }
            }
            _printf("\n");
        }

        for (int i = 0; i < current_depth; ++i)
        {
            for (int k = 0; k < 16; ++k)
            {
                _printf("(%lld / %lld) ", best_section_cutoff_tracker[i][k], best_section_cutoff_tracker_all[i][k]);
            }
            _printf("\n");
        }

        for (int i = 0; i < current_depth; ++i)
        {
            int64_t pct_whole = 0, pct_frac = 0;
            if (ply_stats[i].tt_hits > 0)
            {
                int64_t pct_int = ply_stats[i].tt_cutoffs * 10000000LL / ply_stats[i].tt_hits;
                pct_whole = pct_int / 100000;
                pct_frac = pct_int % 100000;
            }

            _printf("ply_stats[%d]: nodes=%llu, cutoffs/hits = %llu / %llu (cutoff%%=%lld.%05lld)\n", i, ply_stats[i].nodes, ply_stats[i].tt_cutoffs, ply_stats[i].tt_hits, pct_whole, pct_frac);
        }
#endif

        debug_printf("Search order: ");
        for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i)
            debug_printf("%d, ", move_scores[i].move_id);
        debug_printf("\n");

        debug_get_time(stop);

        uint64_t duration = get_time_diff_millis(stop, start);

        debug_printf("Depth %d completed in %llu ms (best_move_idx = %d), evaluation: %d"
#ifdef BRANCH_DEBUG
                     ", checked_pos: %llu, pos/ms: %llu\n",
#else
                     "\n",
#endif
                     current_depth,
                     duration,
                     best_move_idx,
                     iteration_alpha
#ifdef BRANCH_DEBUG
                     ,
                     rec_counter - cur_rec_count,
                     (rec_counter - cur_rec_count) / ((duration == 0) ? 1 : duration));
#else
        );
#endif

        prev_alpha = iteration_alpha;
        *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_alpha};

        if (iteration_alpha > SCORE_TERMINAL_BASE || iteration_alpha < -SCORE_TERMINAL_BASE)
        {
            debug_printf("end condition detected, exiting\n");
            break;
        }
    }
}

static void minimax_iteration_main_st_min(const int32_t max_depth, minimax_main_result_t *__restrict__ ret)
{
    RNAB_ASSUME(possible_moves_buf_moves_count > 0);

    STATIC_BSS TIME_TYPE start, start_it, stop, global_start;
    STATIC_BSS field_t *best_field;
    STATIC_BSS field_t *pos;

    debug_printf("minimax_iteration_main_st_min entry\n");

    debug_get_time(global_start);

    for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i)
        if (possible_moves_buf[i].args.fields.sec_link > 3)
        {
            *ret = (minimax_main_result_t){.best_field = possible_moves_buf[i], .evaluation = -SCORE_TERMINAL_BASE - max_depth};
            return;
        }

    int32_t prev_beta = MAX;

    for (int32_t current_depth = 4; current_depth <= max_depth; current_depth += 2)
    {
#ifdef TRACK_CUTOFF_STATS
        for (int i = 0; i < current_depth; ++i)
        {
            for (int k = 0; k < 16; ++k)
            {
                best_section_cutoff_tracker[i][k] = 0;
                best_section_cutoff_tracker_all[i][k] = 0;
            }
            ply_stats[i].nodes = 0;
            ply_stats[i].tt_cutoffs = 0;
            ply_stats[i].tt_hits = 0;
        }
#endif

        int32_t best_move_idx = -1;

        uint64_t cur_rec_count = rec_counter;
        debug_get_time(start);

        if (current_depth > 4)
        {
            for (int32_t i = 1; i < possible_moves_buf_moves_count; ++i)
            {
                move_scores_wrapper key = move_scores[i];
                int32_t j = i - 1;
                while (j >= 0)
                {
                    move_scores_wrapper *prev = &move_scores[j];
                    if (prev->move_eval < key.move_eval)
                        break; // lower eval wins for minimizer
                    if (prev->move_eval == key.move_eval)
                    {
                        if (prev->is_exact && !key.is_exact)
                            break;
                        if (prev->is_exact == key.is_exact &&
                            prev->move_id <= key.move_id)
                            break;
                    }
                    move_scores[j + 1] = *prev;
                    --j;
                }
                move_scores[j + 1] = key;
            }
        }

        int32_t iteration_alpha = (current_depth == 4) ? MIN : (prev_beta - 5);
        int32_t iteration_beta = prev_beta + 5;

        debug_printf("Window guess: [%d ~ %d]\n", iteration_alpha, iteration_beta);
        do
        {
            move_scores_wrapper *__restrict__ move_ptr = move_scores;

            for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i, ++move_ptr)
            {
                int32_t move_idx = move_ptr->move_id;
                pos = &possible_moves_buf[move_idx];

                int32_t childres;

                if (i == 0) // very likely for the score to be higher
                {
                    childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);

                    move_ptr->is_exact = true;
                }
                else
                {
                    childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_beta - 1, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                    if (childres < iteration_beta)
                    {
                        childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                        move_ptr->is_exact = true;
                    }
                    else
                    {
                        move_ptr->is_exact = false;
                    }
                }

                if (should_exit)
                {
                    if (best_move_idx != -1)
                    {
                        // at least one move completed and beat the upper bound
                        // best_field and iteration_beta already reflect the best found so far
                        *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_beta};
                    }

                    debug_printf("Search order: ");
                    for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                        debug_printf("%d, ", move_scores[u].move_id);
                    debug_printf("\n");
                    debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d\n", i, possible_moves_buf_moves_count, best_move_idx, iteration_beta);

                    return;
                }

                move_ptr->move_eval = childres;

                if (childres < iteration_beta)
                {
                    best_field = pos;
                    iteration_beta = childres;
                    best_move_idx = move_idx;
                }

                if (iteration_beta <= iteration_alpha)
                {
                    debug_printf("cutoff!!!!\n");
                    break;
                }
            }
        } while (0);

        bool guessed_incorrectly = (iteration_beta >= prev_beta + 5) || (iteration_beta <= iteration_alpha);

        iteration_alpha = (iteration_beta <= iteration_alpha) ? MIN : iteration_alpha;
        iteration_beta = (iteration_beta >= prev_beta + 5) ? MAX : iteration_beta;

        if (guessed_incorrectly)
        {
            debug_printf("Guess failed, updated window: [%d ~ %d]\n", iteration_alpha, iteration_beta);

            best_move_idx = -1;

            do
            {
                move_scores_wrapper *__restrict__ move_ptr = move_scores;

                for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i, ++move_ptr)
                {
                    int32_t move_idx = move_ptr->move_id;
                    pos = &possible_moves_buf[move_idx];
                    int32_t childres;

                    if (i == 0)
                    {
                        childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);

                        move_ptr->is_exact = true;
                    }
                    else
                    {
                        childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_beta - 1, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                        if (childres < iteration_beta)
                        {
                            childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                            move_ptr->is_exact = true;
                        }
                        else
                        {
                            move_ptr->is_exact = false;
                        }
                    }

                    if (should_exit)
                    {
                        if (best_move_idx != -1)
                        {
                            // at least one move completed and beat the upper bound
                            // best_field and iteration_beta already reflect the best found so far
                            *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_beta};
                        }

                        debug_printf("Search order: ");
                        for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                            debug_printf("%d, ", move_scores[u].move_id);
                        debug_printf("\n");
                        debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d\n", i, possible_moves_buf_moves_count, best_move_idx, iteration_beta);

                        return;
                    }

                    move_ptr->move_eval = childres;

                    if (childres < iteration_beta)
                    {
                        best_field = pos;
                        iteration_beta = childres;
                        best_move_idx = move_idx;
                    }
                }
            } while (0);
        }

#ifdef TRACK_CUTOFF_STATS
        for (int i = 0; i < 16; ++i)
        {
            _printf("%02d        ", i);
        }
        for (int i = 0; i < current_depth; ++i)
        {
            for (int k = 0; k < 16; ++k)
            {
                int64_t pct_whole = 0, pct_frac = 0;
                if (best_section_cutoff_tracker_all[i][k] > 0)
                {
                    int64_t pct_int = best_section_cutoff_tracker[i][k] * 10000000LL / best_section_cutoff_tracker_all[i][k];
                    pct_whole = pct_int / 100000;
                    pct_frac = pct_int % 100000;

                    _printf("%03lld.%05lld ", pct_whole, pct_frac);
                }
                else
                {
                    _printf("          ");
                }
            }
            _printf("\n");
        }

        for (int i = 0; i < current_depth; ++i)
        {
            for (int k = 0; k < 16; ++k)
            {
                _printf("(%lld / %lld) ", best_section_cutoff_tracker[i][k], best_section_cutoff_tracker_all[i][k]);
            }
            _printf("\n");
        }

        for (int i = 0; i < current_depth; ++i)
        {
            int64_t pct_whole = 0, pct_frac = 0;
            if (ply_stats[i].tt_hits > 0)
            {
                int64_t pct_int = ply_stats[i].tt_cutoffs * 10000000LL / ply_stats[i].tt_hits;
                pct_whole = pct_int / 100000;
                pct_frac = pct_int % 100000;
            }

            _printf("ply_stats[%d]: nodes=%llu, cutoffs/hits = %llu / %llu (cutoff%%=%lld.%05lld)\n", i, ply_stats[i].nodes, ply_stats[i].tt_cutoffs, ply_stats[i].tt_hits, pct_whole, pct_frac);
        }
#endif

        debug_printf("Search order: ");
        for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i)
            debug_printf("%d, ", move_scores[i].move_id);
        debug_printf("\n");

        debug_get_time(stop);

        int64_t duration = get_time_diff_millis(stop, start);

        debug_printf("Depth %d completed in %llu ms (best_move_idx = %d), evaluation: %d"
#ifdef BRANCH_DEBUG
                     ", checked_pos: %llu, pos/ms: %llu\n",
#else
                     "\n",
#endif
                     current_depth,
                     duration,
                     best_move_idx,
                     iteration_beta
#ifdef BRANCH_DEBUG
                     ,
                     rec_counter - cur_rec_count,
                     (rec_counter - cur_rec_count) / ((duration == 0) ? 1 : duration));
#else
        );
#endif

        prev_beta = iteration_beta;
        *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_beta};

        if (iteration_beta > SCORE_TERMINAL_BASE || iteration_beta < -SCORE_TERMINAL_BASE)
        {
            debug_printf("end condition detected, exiting\n");
            break;
        }
    }
}

#ifdef RNAB_MT

extern uint8_t stack_buffer[STACK_SIZE * MAX_THREADS];

_Atomic int shared_eval; // either beta or alpha
_Atomic int queue_head;

volatile int queue_size;
volatile int other_bound;
volatile int search_depth;

void atomic_max(int local)
{
    int current = atomic_load_explicit(&shared_eval, memory_order_relaxed);
    while (local > current)
    {
        if (atomic_compare_exchange_weak_explicit(&shared_eval, &current, local,
                                                  memory_order_release,
                                                  memory_order_acquire))
        {
            break;
        }
    }
}

void atomic_min(int local)
{
    int current = atomic_load_explicit(&shared_eval, memory_order_relaxed);
    while (local < current)
    {
        if (atomic_compare_exchange_weak_explicit(&shared_eval, &current, local,
                                                  memory_order_release,
                                                  memory_order_acquire))
        {
            break;
        }
    }
}

int queue_claim_index()
{
    int idx = atomic_fetch_add_explicit(&queue_head, 1, memory_order_relaxed);

    resume_main_thread(); // always wake up the main thread

    return (idx < queue_size) ? idx : -1;
}

void minimax_max_worker()
{
    int data_id;

    const int32_t beta = other_bound;
    const int32_t current_depth = search_depth;

    while ((data_id = queue_claim_index()) != -1)
    {
        move_scores_wrapper *out_ptr = &move_scores[data_id];

        int32_t move_idx = out_ptr->move_id;
        const field_t *pos = &possible_moves_buf[move_idx];

        int32_t alpha = atomic_load_explicit(&shared_eval, memory_order_relaxed);

        if (beta <= alpha)
        {
            continue; // we have to consume all nodes
        }

        int32_t childres = MINIMAX_CALL(minimax_min, current_depth - 1, alpha, alpha + 1, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);

        if (childres > alpha)
        {
            childres = MINIMAX_CALL(minimax_min, current_depth - 1, alpha, beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);

            out_ptr->is_exact = true;
        }
        else
        {
            out_ptr->is_exact = false;
        }

        if (should_exit) // search was interrupted, dont store unreliable data
        {
            atomic_store_explicit(&queue_head, 32768, memory_order_relaxed); // overwrite queue head
            resume_main_thread();                                            // wake up the main thread, other helper threads should exit very soon too
            return;
        }

        out_ptr->move_eval = childres;

        atomic_max(childres);
    }
}

void minimax_min_worker()
{
    int data_id;

    const int32_t alpha = other_bound;
    const int32_t current_depth = search_depth;

    while ((data_id = queue_claim_index()) != -1)
    {
        move_scores_wrapper *out_ptr = &move_scores[data_id];

        int32_t move_idx = out_ptr->move_id;
        const field_t *pos = &possible_moves_buf[move_idx];

        int32_t beta = atomic_load_explicit(&shared_eval, memory_order_relaxed);

        if (beta <= alpha)
        {
            continue; // we have to consume all nodes
        }

        int32_t childres = MINIMAX_CALL(minimax_max, current_depth - 1, beta - 1, beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);

        if (childres < beta)
        {
            childres = MINIMAX_CALL(minimax_max, current_depth - 1, alpha, beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);

            out_ptr->is_exact = true;
        }
        else
        {
            out_ptr->is_exact = false;
        }

        if (should_exit) // search was interrupted, dont store unreliable data
        {
            atomic_store_explicit(&queue_head, 32768, memory_order_relaxed); // overwrite queue head
            resume_main_thread();                                            // wake up the main thread, other helper threads should exit very soon too
            return;
        }

        out_ptr->move_eval = childres;

        atomic_min(childres);
    }
}

static void minimax_iteration_main_mt_max(const int32_t max_depth, minimax_main_result_t *__restrict__ ret)
{
    RNAB_ASSUME(possible_moves_buf_moves_count > 0);

    STATIC_BSS TIME_TYPE start, start_it, stop, global_start;
    STATIC_BSS field_t *best_field;
    STATIC_BSS field_t *pos;
    bool guessed_incorrectly;

    debug_printf("minimax_iteration_main_mt_max entry\n");

    debug_get_time(global_start);

    for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i)
        if (possible_moves_buf[i].args.fields.fir_link > 3)
        {
            *ret = (minimax_main_result_t){.best_field = possible_moves_buf[i], .evaluation = SCORE_TERMINAL_BASE + max_depth};
            return;
        }

    int32_t prev_alpha = MIN;

    for (int32_t current_depth = 4; current_depth <= max_depth; current_depth += 2)
    {
        int32_t best_move_idx = -1;

        uint64_t cur_rec_count = rec_counter;
        debug_get_time(start);

        if (current_depth > 4)
        {
            for (int32_t i = 1; i < possible_moves_buf_moves_count; ++i)
            {
                move_scores_wrapper key = move_scores[i];
                int32_t j = i - 1;
                while (j >= 0)
                {
                    move_scores_wrapper *prev = &move_scores[j];
                    if (prev->move_eval > key.move_eval)
                        break;
                    if (prev->move_eval == key.move_eval)
                    {
                        if (prev->is_exact && !key.is_exact)
                            break; // exact always wins over non-exact
                        if (prev->is_exact == key.is_exact &&
                            prev->move_id <= key.move_id)
                            break; // stable by move_id only among equals
                    }
                    move_scores[j + 1] = *prev;
                    --j;
                }
                move_scores[j + 1] = key;
            }

            do // reset the scores
            {
                move_scores_wrapper *out_ptr = move_scores;

                for (int k = 0; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
                {
                    out_ptr->move_eval = MIN;
                }
            } while (0);
        }

        int32_t iteration_alpha = prev_alpha - 5;
        int32_t iteration_beta = (current_depth == 4) ? MAX : (prev_alpha + 5);

        debug_printf("Window guess: [%d ~ %d]\n", iteration_alpha, iteration_beta);

        do
        {
            debug_get_time(start_it);

            int32_t move_id = move_scores[0].move_id;

            pos = &possible_moves_buf[move_id];
            int32_t childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
            debug_get_time(stop);

            // debug_printf(FG_BLUE "[f %d: %d -> %d : %lld] " RESET, 0, iteration_alpha, childres, get_time_diff_millis(stop, start_it));

            if (should_exit)
            {
                debug_printf("Search order: ");
                for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                    debug_printf("%d, ", move_scores[u].move_id);
                debug_printf("\n");

                return;
            }

            move_scores[0].is_exact = true;
            move_scores[0].move_eval = childres;

            if (childres > iteration_alpha)
            {
                best_field = pos;
                iteration_alpha = childres;
                best_move_idx = move_id;
            }

            if (iteration_beta <= iteration_alpha)
            {
                guessed_incorrectly = true;
                goto __cutoff_alpha;
            }
        } while (0);

        int thread_num = 0;

        do
        {
            atomic_store_explicit(&shared_eval, iteration_alpha, memory_order_relaxed);
            atomic_store_explicit(&queue_head, 1, memory_order_relaxed);

            queue_size = possible_moves_buf_moves_count;
            search_depth = current_depth;
            other_bound = iteration_beta;

            uint8_t *stack_top = stack_buffer + STACK_SIZE;

            for (int k = 1; k < possible_moves_buf_moves_count; ++k, stack_top += STACK_SIZE)
            {
                thread_create(stack_top, minimax_max_worker);
                ++thread_num;
            }
        } while (0);

        if (thread_num == 0) // all threads failed
        {
            debug_printf("all threads failed\n");
            // timer is already running
            minimax_iteration_main_st_max(max_depth, ret);
            return;
        }

        wait_for_queue();

        // move_scores[k].move_eval remains MIN if a search was aborted

        do
        {
            move_scores_wrapper *out_ptr = &move_scores[1];

            for (int k = 1; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
            {
                const int32_t childres = out_ptr->move_eval;
                const int32_t move_id = out_ptr->move_id;

                if (childres != MIN && childres > iteration_alpha)
                {
                    best_field = &possible_moves_buf[move_id];
                    iteration_alpha = childres;
                    best_move_idx = move_id;
                }
            }
        } while (0);

        if (should_exit)
        {
            if (best_move_idx != -1)
            {
                // at least one move completed and beat the lower bound
                // best_field and iteration_alpha already reflect the best found so far
                *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_alpha};
            }

            debug_printf("Search order: ");
            for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                debug_printf("%d, ", move_scores[u].move_id);
            debug_printf("\n");
            debug_printf("timed out p2 2, best_move_idx=%d, eval=%d\n", best_move_idx, iteration_alpha);

            return;
        }

        // gets here naturally if should_exit is not set
        guessed_incorrectly = (iteration_alpha <= prev_alpha - 5) || (iteration_alpha >= iteration_beta);

    __cutoff_alpha:
        iteration_beta = (iteration_alpha >= iteration_beta) ? MAX : iteration_beta;
        iteration_alpha = (iteration_alpha <= prev_alpha - 5) ? MIN : iteration_alpha;

        if (guessed_incorrectly)
        {
            debug_printf("Guess failed, updated window: [%d ~ %d]\n", iteration_alpha, iteration_beta);

            do // reset the scores
            {
                move_scores_wrapper *out_ptr = move_scores;

                for (int k = 0; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
                {
                    out_ptr->move_eval = MIN;
                }
            } while (0);

            do
            {
                debug_get_time(start_it);

                int32_t move_id = move_scores[0].move_id;

                pos = &possible_moves_buf[move_id];
                int32_t childres = MINIMAX_CALL(minimax_min, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                debug_get_time(stop);

                // debug_printf(FG_BLUE "[f %d: %d -> %d : %lld] " RESET, 0, iteration_alpha, childres, get_time_diff_millis(stop, start_it));

                if (should_exit)
                {
                    debug_printf("Search order: ");
                    for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                        debug_printf("%d, ", move_scores[u].move_id);
                    debug_printf("\n");

                    return;
                }

                move_scores[0].is_exact = true;
                move_scores[0].move_eval = childres;

                if (childres > iteration_alpha)
                {
                    best_field = pos;
                    iteration_alpha = childres;
                    best_move_idx = move_id;
                }
            } while (0);

            int thread_num = 0;

            do
            {
                atomic_store_explicit(&shared_eval, iteration_alpha, memory_order_relaxed);
                atomic_store_explicit(&queue_head, 1, memory_order_relaxed);

                queue_size = possible_moves_buf_moves_count;
                search_depth = current_depth;
                other_bound = iteration_beta;

                uint8_t *stack_top = stack_buffer + STACK_SIZE;

                for (int k = 1; k < possible_moves_buf_moves_count; ++k, stack_top += STACK_SIZE)
                {
                    thread_create(stack_top, minimax_max_worker);
                    ++thread_num;
                }
            } while (0);

            if (thread_num == 0) // all threads failed
            {
                debug_printf("all threads failed\n");
                // timer is already running
                minimax_iteration_main_st_max(max_depth, ret);
                return;
            }

            wait_for_queue();

            // move_scores[k].move_eval remains MIN if a search was aborted

            do
            {
                move_scores_wrapper *out_ptr = &move_scores[1];

                for (int k = 1; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
                {
                    const int32_t childres = out_ptr->move_eval;
                    const int32_t move_id = out_ptr->move_id;

                    if (childres != MIN && childres > iteration_alpha)
                    {
                        best_field = &possible_moves_buf[move_id];
                        iteration_alpha = childres;
                        best_move_idx = move_id;
                    }
                }
            } while (0);

            if (should_exit)
            {
                if (best_move_idx != -1)
                {
                    // at least one move completed and beat the lower bound
                    // best_field and iteration_alpha already reflect the best found so far
                    *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_alpha};
                }

                debug_printf("Search order: ");
                for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                    debug_printf("%d, ", move_scores[u].move_id);
                debug_printf("\n");
                debug_printf("timed out p2 2, best_move_idx=%d, eval=%d\n", best_move_idx, iteration_alpha);

                return;
            }
        }

        debug_printf("Search order: ");
        for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i)
            debug_printf("%d, ", move_scores[i].move_id);
        debug_printf("\n");

        debug_get_time(stop);

        uint64_t duration = get_time_diff_millis(stop, start);

        debug_printf("Depth %d completed in %llu ms (best_move_idx = %d), evaluation: %d"
#ifdef BRANCH_DEBUG
                     ", checked_pos: %llu, pos/ms: %llu\n",
#else
                     "\n",
#endif
                     current_depth,
                     duration,
                     best_move_idx,
                     iteration_alpha
#ifdef BRANCH_DEBUG
                     ,
                     rec_counter - cur_rec_count,
                     (rec_counter - cur_rec_count) / ((duration == 0) ? 1 : duration));
#else
        );
#endif

        prev_alpha = iteration_alpha;
        *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_alpha};

        if (iteration_alpha > SCORE_TERMINAL_BASE || iteration_alpha < -SCORE_TERMINAL_BASE)
        {
            debug_printf("end condition detected, exiting\n");
            break;
        }
    }
}

static void minimax_iteration_main_mt_min(const int32_t max_depth, minimax_main_result_t *__restrict__ ret)
{
    RNAB_ASSUME(possible_moves_buf_moves_count > 0);

    STATIC_BSS TIME_TYPE start, start_it, stop, global_start;
    STATIC_BSS field_t *best_field;
    STATIC_BSS field_t *pos;
    bool guessed_incorrectly;

    debug_printf("minimax_iteration_main_mt_min entry\n");

    debug_get_time(global_start);

    for (int32_t i = 0; i < MAX_MOVES; ++i)
        if (possible_moves_buf[i].args.fields.sec_link > 3)
        {
            *ret = (minimax_main_result_t){.best_field = possible_moves_buf[i], .evaluation = -SCORE_TERMINAL_BASE - max_depth};
            return;
        }

    int32_t prev_beta = MAX;

    for (int32_t current_depth = 4; current_depth <= max_depth; current_depth += 2)
    {
        int32_t best_move_idx = -1;

        uint64_t cur_rec_count = rec_counter;
        debug_get_time(start);

        if (current_depth > 4)
        {
            for (int32_t i = 1; i < possible_moves_buf_moves_count; ++i)
            {
                move_scores_wrapper key = move_scores[i];
                int32_t j = i - 1;
                while (j >= 0)
                {
                    move_scores_wrapper *prev = &move_scores[j];
                    if (prev->move_eval < key.move_eval)
                        break; // lower eval wins for minimizer
                    if (prev->move_eval == key.move_eval)
                    {
                        if (prev->is_exact && !key.is_exact)
                            break;
                        if (prev->is_exact == key.is_exact &&
                            prev->move_id <= key.move_id)
                            break;
                    }
                    move_scores[j + 1] = *prev;
                    --j;
                }
                move_scores[j + 1] = key;
            }

            do // reset the scores
            {
                move_scores_wrapper *out_ptr = move_scores;

                for (int k = 0; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
                {
                    out_ptr->move_eval = MAX;
                }
            } while (0);
        }

        int32_t iteration_alpha = (current_depth == 4) ? MIN : (prev_beta - 5);
        int32_t iteration_beta = prev_beta + 5;

        debug_printf("Window guess: [%d ~ %d]\n", iteration_alpha, iteration_beta);

        do
        {
            debug_get_time(start_it);

            int32_t move_id = move_scores[0].move_id;

            pos = &possible_moves_buf[move_id];
            int32_t childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
            debug_get_time(stop);

            if (should_exit)
            {
                debug_printf("Search order: ");
                for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                    debug_printf("%d, ", move_scores[u].move_id);
                debug_printf("\n");

                return;
            }

            move_scores[0].is_exact = true;
            move_scores[0].move_eval = childres;

            if (childres < iteration_beta)
            {
                best_field = pos;
                iteration_beta = childres;
                best_move_idx = move_id;
            }

            if (iteration_beta <= iteration_alpha)
            {
                guessed_incorrectly = true;
                goto __cutoff_beta;
            }
        } while (0);

        int thread_num = 0;

        do
        {
            atomic_store_explicit(&shared_eval, iteration_beta, memory_order_relaxed);
            atomic_store_explicit(&queue_head, 1, memory_order_relaxed);

            queue_size = possible_moves_buf_moves_count;
            search_depth = current_depth;
            other_bound = iteration_alpha;

            uint8_t *stack_top = stack_buffer + STACK_SIZE;

            for (int k = 1; k < possible_moves_buf_moves_count; ++k, stack_top += STACK_SIZE)
            {
                thread_create(stack_top, minimax_min_worker);
                ++thread_num;
            }
        } while (0);

        if (thread_num == 0) // all threads failed
        {
            debug_printf("all threads failed\n");
            // timer is already running
            minimax_iteration_main_st_min(max_depth, ret);
            return;
        }

        wait_for_queue();

        // move_scores[k].move_eval remains MAX if a search was aborted

        do
        {
            move_scores_wrapper *out_ptr = &move_scores[1];

            for (int k = 1; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
            {
                const int32_t childres = out_ptr->move_eval;
                const int32_t move_id = out_ptr->move_id;

                if (childres != MAX && childres < iteration_beta)
                {
                    best_field = &possible_moves_buf[move_id];
                    iteration_beta = childres;
                    best_move_idx = move_id;
                }
            }
        } while (0);

        if (should_exit)
        {
            if (best_move_idx != -1)
            {
                // at least one move completed and beat the lower bound
                // best_field and iteration_alpha already reflect the best found so far
                *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_beta};
            }

            debug_printf("Search order: ");
            for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                debug_printf("%d, ", move_scores[u].move_id);
            debug_printf("\n");
            debug_printf("timed out p2 2, best_move_idx=%d, eval=%d\n", best_move_idx, iteration_beta);

            return;
        }

        // gets here naturally if should_exit is not set
        guessed_incorrectly = (iteration_beta >= prev_beta + 5) || (iteration_beta <= iteration_alpha);

    __cutoff_beta:
        iteration_alpha = (iteration_beta <= iteration_alpha) ? MIN : iteration_alpha;
        iteration_beta = (iteration_beta >= prev_beta + 5) ? MAX : iteration_beta;

        if (guessed_incorrectly)
        {
            debug_printf("Guess failed, updated window: [%d ~ %d]\n", iteration_alpha, iteration_beta);

            do // reset the scores
            {
                move_scores_wrapper *out_ptr = move_scores;

                for (int k = 0; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
                {
                    out_ptr->move_eval = MAX;
                }
            } while (0);

            do
            {
                debug_get_time(start_it);

                int32_t move_id = move_scores[0].move_id;

                pos = &possible_moves_buf[move_id];
                int32_t childres = MINIMAX_CALL(minimax_max, current_depth - 1, iteration_alpha, iteration_beta, field_evaluate_slow(pos), pos->is_fir_mask, pos->is_sec_mask, pos->is_link_mask, pos->is_boosted_mask, pos->args);
                debug_get_time(stop);

                if (should_exit)
                {
                    debug_printf("Search order: ");
                    for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                        debug_printf("%d, ", move_scores[u].move_id);
                    debug_printf("\n");

                    return;
                }

                move_scores[0].is_exact = true;
                move_scores[0].move_eval = childres;

                if (childres < iteration_beta)
                {
                    best_field = pos;
                    iteration_beta = childres;
                    best_move_idx = move_id;
                }
            } while (0);

            int thread_num = 0;

            do
            {
                atomic_store_explicit(&shared_eval, iteration_beta, memory_order_relaxed);
                atomic_store_explicit(&queue_head, 1, memory_order_relaxed);

                queue_size = possible_moves_buf_moves_count;
                search_depth = current_depth;
                other_bound = iteration_alpha;

                uint8_t *stack_top = stack_buffer + STACK_SIZE;

                for (int k = 1; k < possible_moves_buf_moves_count; ++k, stack_top += STACK_SIZE)
                {
                    thread_create(stack_top, minimax_min_worker);
                    ++thread_num;
                }
            } while (0);

            if (thread_num == 0) // all threads failed
            {
                debug_printf("all threads failed\n");
                // timer is already running
                minimax_iteration_main_st_min(max_depth, ret);
                return;
            }

            wait_for_queue();

            // move_scores[k].move_eval remains MAX if a search was aborted

            do
            {
                move_scores_wrapper *out_ptr = &move_scores[1];

                for (int k = 1; k < possible_moves_buf_moves_count; ++k, ++out_ptr)
                {
                    const int32_t childres = out_ptr->move_eval;
                    const int32_t move_id = out_ptr->move_id;

                    if (childres != MAX && childres < iteration_beta)
                    {
                        best_field = &possible_moves_buf[move_id];
                        iteration_beta = childres;
                        best_move_idx = move_id;
                    }
                }
            } while (0);

            if (should_exit)
            {
                if (best_move_idx != -1)
                {
                    // at least one move completed and beat the lower bound
                    // best_field and iteration_alpha already reflect the best found so far
                    *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_beta};
                }

                debug_printf("Search order: ");
                for (int32_t u = 0; u < possible_moves_buf_moves_count; ++u)
                    debug_printf("%d, ", move_scores[u].move_id);
                debug_printf("\n");
                debug_printf("timed out p2 2, best_move_idx=%d, eval=%d\n", best_move_idx, iteration_beta);

                return;
            }
        }

        debug_printf("Search order: ");
        for (int32_t i = 0; i < possible_moves_buf_moves_count; ++i)
            debug_printf("%d, ", move_scores[i].move_id);
        debug_printf("\n");

        debug_get_time(stop);

        int64_t duration = get_time_diff_millis(stop, start);

        debug_printf("Depth %d completed in %llu ms (best_move_idx = %d), evaluation: %d"
#ifdef BRANCH_DEBUG
                     ", checked_pos: %llu, pos/ms: %llu\n",
#else
                     "\n",
#endif
                     current_depth,
                     duration,
                     best_move_idx,
                     iteration_beta
#ifdef BRANCH_DEBUG
                     ,
                     rec_counter - cur_rec_count,
                     (rec_counter - cur_rec_count) / ((duration == 0) ? 1 : duration));
#else
        );
#endif

        prev_beta = iteration_beta;
        *ret = (minimax_main_result_t){.best_field = *best_field, .evaluation = iteration_beta};

        if (iteration_beta > SCORE_TERMINAL_BASE || iteration_beta < -SCORE_TERMINAL_BASE)
        {
            debug_printf("end condition detected, exiting\n");
            break;
        }
    }
}

#endif

static void minimax_iteration_main(const int32_t max_depth, const uint32_t max_search_time, const bool player, const field_t *__restrict__ position, minimax_main_result_t *__restrict__ ret)
{
    (player ? possible_moves_max : possible_moves_min)(position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask, position->args, possible_moves_buf, &possible_moves_buf_moves_count);

    if (possible_moves_buf_moves_count == 0) // guard
        return;

    CLEAR_TT();

    do
    {
        uint32_t base = (uint32_t)(uint16_t)(player ? MAX : MIN);
        for (int32_t i = 0; i < MAX_MOVES; ++i, base += (1 << 16))
        {
            move_scores[i] = *(move_scores_wrapper *)&base;
        }
    } while (0);

    start_search_timer(max_search_time);

#ifdef RNAB_MT
    ((get_thread_count() > 2) ? (player ? minimax_iteration_main_mt_max : minimax_iteration_main_mt_min) : ((player ? minimax_iteration_main_st_max : minimax_iteration_main_st_min)))(max_depth, ret);
#else
    (player ? minimax_iteration_main_st_max : minimax_iteration_main_st_min)(max_depth, ret);
#endif

    stop_search_timer();
}

// cleanup

#undef MOVE_SECTION_BEGIN
#undef MOVE_SECTION_END
#undef PERFORM_ITERATION_FIR
#undef PERFORM_ITERATION_SEC
#undef TRACK_ENTRY_MIN
#undef TRACK_ENTRY_MAX
#undef RNAB_MT
#undef RNAB_REVCACHE
#undef TRACK_CUTOFF_STATS
#undef STB_SPRINTF_MIN
#undef MAX_MOVES
#undef MIN_MT_DEPTH
#undef MIN
#undef MAX
#undef MIN_CACHE_DEPTH
#undef ITERATION_CURRENT_IS_FIRST_UNKNOWN
#undef ITERATION_CURRENT_IS_FIRST_LINK
#undef ITERATION_CURRENT_IS_FIRST_VIRUS
#undef ITERATION_CURRENT_IS_SECOND_UNKNOWN
#undef ITERATION_CURRENT_IS_SECOND_LINK
#undef ITERATION_CURRENT_IS_SECOND_VIRUS
#undef TT_EXACT
#undef TT_LOWERBOUND
#undef TT_UPPERBOUND
#undef CAN_MOVE_DOUBLE_RIGHT
#undef CAN_MOVE_RIGHT
#undef CAN_MOVE_LEFT
#undef CAN_MOVE_DOUBLE_LEFT
#undef RESET
#undef FG_RED
#undef FG_GREEN
#undef FG_BLUE
#undef FG_WHITE
#undef BG_RED
#undef BG_GREEN
#undef RNAB_ASSUME
#undef TABLE_SIZE 
#undef ENTRY_HASH_SIZE
#undef ENTRY_HASH_MASK
#undef TABLE_SIZE_BITS
#undef clear_lowest_set_bit
#undef clear_highest_set_bit
#undef extract_lsb
#undef MAX_BRANCHES
#undef SCORE_REGULAR_MAX
#undef SCORE_TERMINAL_BASE
#undef BEGIN_BRANCH_TRACKING
#undef BRANCH_ENTER_MAX
#undef BRANCH_ENTER_MIN
#undef BRANCH_EXIT_MAX
#undef BRANCH_EXIT_MIN
#undef END_BRANCH_TRACKING
#undef TRACK_ENTRY_MAX
#undef TRACK_ENTRY_MIN
#undef MOVE_SECTION_BEGIN
#undef MOVE_SECTION_END
#undef MINIMAX_FUNC
#undef MINIMAX_CALL
#undef BRANCHED_MINIMAX_CALL
#undef MINIMAX_UNPACK
