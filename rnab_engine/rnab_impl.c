#define _GNU_SOURCE
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include "../third_party/rapidhash.h"

#ifdef __AVX2__
#include <immintrin.h>
#endif

#if defined(__BMI__)
#define clear_lowest_set_bit(x, var) (x) = _blsr_u64(x)
#define clear_highest_set_bit(x, var) ((x) ^= (var))
#else
#define clear_lowest_set_bit(x, var) ((x) ^= (var))
#define clear_highest_set_bit(x, var) ((x) ^= (var))
#endif

#ifndef GAME_CONSTANTS
#define GAME_CONSTANTS

#define MAX_MOVES 80
#define MAX_DEPTH 40

#define MIN -1000000
#define MAX 1000000

#define MAX_BRANCHES 1000

#ifndef MIN_CACHE_DEPTH
#define MIN_CACHE_DEPTH 1
#endif

#define ITERATION_CURRENT_IS_FIRST_UNKNOWN 0
#define ITERATION_CURRENT_IS_FIRST_LINK 1
#define ITERATION_CURRENT_IS_FIRST_VIRUS 2
#define ITERATION_CURRENT_IS_SECOND_UNKNOWN 3
#define ITERATION_CURRENT_IS_SECOND_LINK 4
#define ITERATION_CURRENT_IS_SECOND_VIRUS 5

#define TT_EXACT 0
#define TT_LOWERBOUND 1
#define TT_UPPERBOUND 2

#endif

#define CAN_MOVE_BACKWARD_RIGHT 18374403900871474688ULL
#define CAN_MOVE_BACKWARD_LEFT 9187201950435737344ULL
#define CAN_MOVE_FORWARD_RIGHT 71775015237779198ULL
#define CAN_MOVE_FORWARD_LEFT 35887507618889599ULL

#define CAN_MOVE_DOUBLE_RIGHT 18229723555195321596ULL
#define CAN_MOVE_RIGHT 18374403900871474942ULL
#define CAN_MOVE_LEFT 9187201950435737471ULL
#define CAN_MOVE_DOUBLE_LEFT 4557430888798830399ULL
#define CAN_MOVE_DOUBLE_FORWARD 281474976710655ULL
#define CAN_MOVE_DOUBLE_BACKWARD 18446744073709486080ULL

#ifdef RNAB_DEBUG
#define debug_printf(...) __builtin_printf(__VA_ARGS__)
#else
#define debug_printf(...) ((void)0)
#endif

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

typedef struct
{
    uint64_t hash_entry;
    int32_t eval;
    uint8_t depth;
    uint8_t flag;
} tt_entry_t;

_Static_assert(MAX_DEPTH < UINT8_MAX, "MAX_DEPTH should be smaller than 256"); // uint8_t limit

typedef struct
{
    tt_entry_t depth_preferred;
    tt_entry_t scratch;
} tt_bucket_t;

typedef struct
{
    uint64_t is_fir_mask;
    uint64_t is_sec_mask;
    uint64_t is_link_mask;
    uint64_t is_boosted_mask;

    uint8_t forward_adv_fir;
    uint8_t forward_adv_sec;

    uint8_t firewall_fir;
    uint8_t firewall_sec;

    uint8_t fir_link : 4;
    uint8_t sec_link : 4;
    uint8_t fir_virus : 4;
    uint8_t sec_virus : 4;

    uint8_t is_swap_available_fir;
    uint8_t is_swap_available_sec;
} field_t;

_Static_assert(sizeof(field_t) == 40, "field must be exactly 40 bytes");

typedef struct
{
    field_t best_field;
    int evaluation;
    bool has_timed_out;
} minimax_main_result_t;

typedef struct
{
    field_t moves[MAX_MOVES];
    int moves_count;
} possible_moves_t;

#ifdef BRANCH_DEBUG
typedef struct
{
    int64_t total_entries;
    int64_t cutoff_entries;
    int64_t improved_score;
    int64_t recursion_cost;
    const char *msg;
    int temp_score;
    int pad;
} cutoff_tracker_t;
#endif

#ifndef TABLE_SIZE // should never be undefined, supress warnings
#define TABLE_SIZE 1024
#endif

#define CLEAR_TT()                               \
    __builtin_memset(tt_fir, 0, sizeof(tt_fir)); \
    __builtin_memset(tt_sec, 0, sizeof(tt_sec))

static const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

static int cur_search_depth = 0;
static int64_t rec_counter = 0;

static tt_bucket_t tt_fir[TABLE_SIZE] __attribute__((aligned(4096))) = {0};
static tt_bucket_t tt_sec[TABLE_SIZE] __attribute__((aligned(4096))) = {0};

#ifdef BRANCH_DEBUG
cutoff_tracker_t cutoff_tracker[MAX_BRANCHES] = {0};

void clear_branch_tracker()
{
    for (int i = 0; i < MAX_BRANCHES; ++i)
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
        const int cur_pos = __builtin_ctzll(mask);
        const uint64_t cur_mask = 1ULL << cur_pos;

        res |= (1ULL << (63 - cur_pos));

        mask ^= cur_mask;
    }

    return res;
}

static field_t reverse_field(field_t *__restrict__ f) // should only be used for debugging
{
    field_t new_field;

    new_field.is_fir_mask = reverse_mask(f->is_sec_mask);
    new_field.is_sec_mask = reverse_mask(f->is_fir_mask);
    new_field.is_link_mask = reverse_mask(f->is_link_mask);
    new_field.is_boosted_mask = reverse_mask(f->is_boosted_mask);

    new_field.forward_adv_fir = f->forward_adv_sec;
    new_field.forward_adv_sec = f->forward_adv_fir;

    new_field.firewall_fir = (f->firewall_sec) ? ((126 - (f->firewall_sec & 126)) | (f->firewall_sec & 1)) : 0;
    new_field.firewall_sec = (f->firewall_fir) ? ((126 - (f->firewall_fir & 126)) | (f->firewall_fir & 1)) : 0;

    new_field.fir_link = f->sec_link;
    new_field.fir_virus = f->sec_virus;

    new_field.sec_link = f->fir_link;
    new_field.sec_virus = f->fir_virus;

    new_field.is_swap_available_fir = f->is_swap_available_sec;
    new_field.is_swap_available_sec = f->is_swap_available_fir;

    return new_field;
}

static inline __attribute__((always_inline)) field_t init_field(uint8_t pos_fir, uint8_t pos_sec)
{
    field_t f = {0};

    f.is_sec_mask = 0;
    f.is_link_mask = 0;
    f.is_fir_mask = 0;
    f.is_boosted_mask = 0;

    f.fir_link = 0;
    f.fir_virus = 0;
    f.sec_link = 0;
    f.sec_virus = 0;

    f.forward_adv_fir = 2;
    f.forward_adv_sec = 2;

    f.firewall_fir = 0;
    f.firewall_sec = 0;

    f.is_swap_available_fir = 1;
    f.is_swap_available_sec = 1;

    static const int init_pos_fir[8] = {63, 62, 61, 52, 51, 58, 57, 56};
    static const int init_pos_sec[8] = {0, 1, 2, 11, 12, 5, 6, 7};

    for (int i = 0; i < 8; ++i)
    {
        f.is_sec_mask |= (1ULL << init_pos_sec[i]);
        f.is_fir_mask |= (1ULL << init_pos_fir[i]);
        f.is_link_mask |= ((uint64_t)(((~pos_fir) >> i) & 1) << init_pos_fir[i]) | ((uint64_t)(((~pos_sec) >> i) & 1) << init_pos_sec[i]);
    }

    return f;
}

static void print_field(field_t *__restrict__ f)
{
    __builtin_printf("Virus: %d         Link: %d\n", f->fir_virus, f->fir_link);
    __builtin_printf("   " BG_GREEN "[C]" RESET);
    if (f->is_swap_available_fir)
        __builtin_printf(BG_GREEN "[S]" RESET "      ");
    else
        __builtin_printf(BG_RED "[S]" RESET "      ");
    if (f->firewall_fir == 0)
        __builtin_printf(BG_GREEN "[F]" RESET);
    else
        __builtin_printf(BG_RED "[F]" RESET);
    if (f->is_boosted_mask & f->is_fir_mask)
        __builtin_printf(BG_RED "[B]" RESET "\n");
    else
        __builtin_printf(BG_GREEN "[B]" RESET "\n");
    for (int i = 63; i >= 0; --i)
    {
        if (f->firewall_fir == ((i << 1) | 1))
            __builtin_printf(BG_GREEN);
        if (f->firewall_sec == ((i << 1) | 1))
            __builtin_printf(BG_RED);
        if (((f->is_fir_mask & f->is_link_mask) >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                __builtin_printf("" FG_GREEN "[" FG_BLUE "L" FG_GREEN "]");
            else
                __builtin_printf("[" FG_GREEN "L" FG_WHITE "]");
        }
        else if ((f->is_fir_mask >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                __builtin_printf("" FG_GREEN "[" FG_BLUE "V" FG_GREEN "]");
            else
                __builtin_printf("[" FG_GREEN "V" FG_WHITE "]");
        }
        else if (((f->is_sec_mask & f->is_link_mask) >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                __builtin_printf(FG_RED "[" FG_BLUE "L" FG_RED "]");
            else
                __builtin_printf("[" FG_RED "L" FG_WHITE "]");
        }
        else if ((f->is_sec_mask >> i) & 1)
        {
            if ((f->is_boosted_mask >> i) & 1)
                __builtin_printf(FG_RED "[" FG_BLUE "V" FG_RED "]");
            else
                __builtin_printf("[" FG_RED "V" FG_WHITE "]");
        }
        else
            __builtin_printf("[ ]");

        if (i % 8 == 0)
            __builtin_printf("\n");
        __builtin_printf(RESET);
    }
    __builtin_printf("   " BG_GREEN "[C]" RESET);
    if (f->is_swap_available_sec)
        __builtin_printf(BG_GREEN "[S]" RESET "      ");
    else
        __builtin_printf(BG_RED "[S]" RESET "      ");
    if (f->firewall_sec == 0)
        __builtin_printf(BG_GREEN "[F]" RESET);
    else
        __builtin_printf(BG_RED "[F]" RESET);
    if (f->is_boosted_mask & f->is_sec_mask)
        __builtin_printf(BG_RED "[B]" RESET "\n");
    else
        __builtin_printf(BG_GREEN "[B]" RESET "\n");

    __builtin_printf("Virus: %d         Link: %d\n", f->sec_virus, f->sec_link);
}

static inline __attribute__((always_inline)) int field_evaluate(const field_t *__restrict__ f)
{
    return ((128 << f->fir_link) - (64 << f->fir_virus) - (128 << f->sec_link) + (64 << f->sec_virus)) + (int)f->forward_adv_fir - (int)f->forward_adv_sec + 256 * (int)f->is_swap_available_fir - 256 * (int)f->is_swap_available_sec;
}

#define PERFORM_ITERATION_FIR(shift_func, shift_count, forward_adv, is_boosted, current_card) \
    if (secmask & new_pos_bitboard)                                                           \
    {                                                                                         \
        if (sec_link_mask & new_pos_bitboard)                                                 \
        {                                                                                     \
            field_t temp_field = *position;                                                   \
                                                                                              \
            int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                            \
            temp_field.forward_adv_sec -= (uint8_t)(new_pos_coord >> 3);                      \
            temp_field.forward_adv_fir += forward_adv;                                        \
            if (is_boosted)                                                                   \
            {                                                                                 \
                temp_field.is_boosted_mask ^= (cur_pos_bitboard);                             \
                temp_field.is_boosted_mask |= (new_pos_bitboard);                             \
            }                                                                                 \
            else                                                                              \
            {                                                                                 \
                temp_field.is_boosted_mask &= ~(new_pos_bitboard);                            \
            }                                                                                 \
            temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                  \
            temp_field.is_link_mask &= ~new_pos_bitboard;                                     \
            if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                           \
            {                                                                                 \
                uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                   \
                temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));            \
            }                                                                                 \
            else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                         \
            {                                                                                 \
                temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);             \
            }                                                                                 \
            temp_field.is_sec_mask ^= new_pos_bitboard;                                       \
            ++temp_field.fir_link;                                                            \
                                                                                              \
            WRITE_MOVE();                                                                     \
        }                                                                                     \
        else if (position->fir_virus < 3)                                                     \
        {                                                                                     \
            field_t temp_field = *position;                                                   \
                                                                                              \
            int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                            \
            temp_field.forward_adv_sec -= (uint8_t)(new_pos_coord >> 3);                      \
            temp_field.forward_adv_fir += forward_adv;                                        \
            if (is_boosted)                                                                   \
            {                                                                                 \
                temp_field.is_boosted_mask ^= (cur_pos_bitboard);                             \
                temp_field.is_boosted_mask |= (new_pos_bitboard);                             \
            }                                                                                 \
            else                                                                              \
            {                                                                                 \
                temp_field.is_boosted_mask &= ~(new_pos_bitboard);                            \
            }                                                                                 \
            temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                  \
            if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                           \
            {                                                                                 \
                uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                   \
                temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));            \
            }                                                                                 \
            else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                         \
            {                                                                                 \
                temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);             \
            }                                                                                 \
            temp_field.is_sec_mask ^= new_pos_bitboard;                                       \
            ++temp_field.fir_virus;                                                           \
                                                                                              \
            WRITE_MOVE();                                                                     \
        }                                                                                     \
    }                                                                                         \
    else if ((firmask & new_pos_bitboard) == 0)                                               \
    {                                                                                         \
        field_t temp_field = *position;                                                       \
                                                                                              \
        temp_field.forward_adv_fir += forward_adv;                                            \
        if (is_boosted)                                                                       \
            temp_field.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);              \
        temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                      \
        if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                               \
        {                                                                                     \
            uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                       \
            temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                \
        }                                                                                     \
        else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                             \
        {                                                                                     \
            temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                 \
        }                                                                                     \
                                                                                              \
        WRITE_MOVE();                                                                         \
    }

#define PERFORM_ITERATION_SEC(shift_func, shift_count, forward_adv, is_boosted, current_card) \
    if (firmask & new_pos_bitboard)                                                           \
    {                                                                                         \
        if (fir_link_mask & new_pos_bitboard)                                                 \
        {                                                                                     \
            field_t temp_field = *position;                                                   \
                                                                                              \
            int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                            \
            temp_field.forward_adv_fir -= (uint8_t)(7 - (new_pos_coord >> 3));                \
            temp_field.forward_adv_sec += forward_adv;                                        \
            if (is_boosted)                                                                   \
            {                                                                                 \
                temp_field.is_boosted_mask ^= (cur_pos_bitboard);                             \
                temp_field.is_boosted_mask |= (new_pos_bitboard);                             \
            }                                                                                 \
            else                                                                              \
            {                                                                                 \
                temp_field.is_boosted_mask &= ~(new_pos_bitboard);                            \
            }                                                                                 \
            temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                  \
            temp_field.is_link_mask &= ~new_pos_bitboard;                                     \
            if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                          \
            {                                                                                 \
                uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                   \
                temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));            \
            }                                                                                 \
            else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                        \
            {                                                                                 \
                temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);             \
            }                                                                                 \
            temp_field.is_fir_mask ^= new_pos_bitboard;                                       \
            ++temp_field.sec_link;                                                            \
                                                                                              \
            WRITE_MOVE();                                                                     \
        }                                                                                     \
        else if (position->sec_virus < 3)                                                     \
        {                                                                                     \
            field_t temp_field = *position;                                                   \
                                                                                              \
            int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                            \
            temp_field.forward_adv_fir -= (uint8_t)(7 - (new_pos_coord >> 3));                \
            temp_field.forward_adv_sec += forward_adv;                                        \
            if (is_boosted)                                                                   \
            {                                                                                 \
                temp_field.is_boosted_mask ^= (cur_pos_bitboard);                             \
                temp_field.is_boosted_mask |= (new_pos_bitboard);                             \
            }                                                                                 \
            else                                                                              \
            {                                                                                 \
                temp_field.is_boosted_mask &= ~(new_pos_bitboard);                            \
            }                                                                                 \
            temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                  \
            if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                          \
            {                                                                                 \
                uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                   \
                temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));            \
            }                                                                                 \
            else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                        \
            {                                                                                 \
                temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);             \
            }                                                                                 \
            temp_field.is_fir_mask ^= new_pos_bitboard;                                       \
            ++temp_field.sec_virus;                                                           \
                                                                                              \
            WRITE_MOVE();                                                                     \
        }                                                                                     \
    }                                                                                         \
    else if ((secmask & new_pos_bitboard) == 0)                                               \
    {                                                                                         \
        field_t temp_field = *position;                                                       \
                                                                                              \
        temp_field.forward_adv_sec += forward_adv;                                            \
        if (is_boosted)                                                                       \
            temp_field.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);              \
        temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                      \
        if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                              \
        {                                                                                     \
            uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                       \
            temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                \
        }                                                                                     \
        else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                            \
        {                                                                                     \
            temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                 \
        }                                                                                     \
                                                                                              \
        WRITE_MOVE();                                                                         \
    }

#if defined(__clang__)
static __attribute__((minsize, cold)) possible_moves_t possible_moves(const field_t *__restrict__ position, const bool player)
#elif defined(__GNUC__)
static __attribute__((optimize("Os"), cold)) possible_moves_t possible_moves(const field_t *__restrict__ position, const bool player)
#else
static possible_moves_t possible_moves(const field_t *__restrict__ position, const bool player)
#endif
{
    possible_moves_t res;
    res.moves_count = 0;

#define WRITE_MOVE()                      \
    int cached_count = res.moves_count;   \
    res.moves[cached_count] = temp_field; \
    res.moves_count = cached_count + 1;

    const uint64_t firmask = position->is_fir_mask, secmask = position->is_sec_mask;

    if (player)
    {
        const uint64_t fir_link_mask = position->is_link_mask & firmask;
        const uint64_t fir_virus_mask = firmask ^ fir_link_mask;
        const uint64_t sec_link_mask = position->is_link_mask ^ fir_link_mask;

        const uint64_t cur_boosted_mask = position->is_boosted_mask & firmask;
        const uint64_t enemy_firewall_mask = (uint64_t)(position->firewall_sec & 1) << (position->firewall_sec >> 1);
        const uint64_t free_mask = ~(firmask | secmask | enemy_firewall_mask);

        if (__builtin_expect(fir_link_mask & 8ULL, 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_fir -= (uint8_t)(7 - (__builtin_ctzll(8ULL) >> 3));
            temp_field.is_boosted_mask &= ~8ULL;
            temp_field.is_fir_mask &= ~8ULL;
            temp_field.is_link_mask &= ~8ULL;
            ++temp_field.fir_link;

            WRITE_MOVE();
        }
        if (__builtin_expect(fir_link_mask & 16ULL, 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_fir -= (uint8_t)(7 - (__builtin_ctzll(16ULL) >> 3));
            temp_field.is_boosted_mask &= ~16ULL;
            temp_field.is_fir_mask &= ~16ULL;
            temp_field.is_link_mask &= ~16ULL;
            ++temp_field.fir_link;

            WRITE_MOVE();
        }

        const uint64_t boosted_link = cur_boosted_mask & fir_link_mask;

        if (__builtin_expect(((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 24ULL, 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_fir -= (uint8_t)(7 - (__builtin_ctzll(cur_boosted_mask) >> 3));
            temp_field.is_boosted_mask &= ~cur_boosted_mask;
            temp_field.is_fir_mask &= ~cur_boosted_mask;
            temp_field.is_link_mask &= ~cur_boosted_mask;
            ++temp_field.fir_link;

            WRITE_MOVE();
        }

        if (cur_boosted_mask == 0)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                field_t temp_field = *position;

                temp_field.is_boosted_mask |= pos;

                WRITE_MOVE();

                clear_lowest_set_bit(temp, pos);
            }
        }
        else
        {
            const uint64_t cur_pos_bitboard = cur_boosted_mask;
            const uint64_t links_masked_out = sec_link_mask & ~enemy_firewall_mask;

            if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

                RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            const uint64_t legal_mask = (~enemy_firewall_mask) & (~links_masked_out) & (~firmask);

            if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

                RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

                PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }
        }

        uint64_t unboosted_cards_mask = fir_virus_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));
            const uint64_t legal_mask = (secmask & (~enemy_firewall_mask));

            if (cur_pos_bitboard & (legal_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            if (cur_pos_bitboard & (legal_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        unboosted_cards_mask = fir_link_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));
            const uint64_t legal_mask = (secmask & (~enemy_firewall_mask));

            if (cur_pos_bitboard & (legal_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            if (cur_pos_bitboard & (legal_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME(secmask & new_pos_bitboard);

                PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        unboosted_cards_mask = fir_link_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));

            if (cur_pos_bitboard & (free_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            if (cur_pos_bitboard & (free_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        unboosted_cards_mask = fir_virus_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));

            if (cur_pos_bitboard & (free_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            if (cur_pos_bitboard & (free_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        if (position->firewall_fir == 0)
        {
            uint64_t temp = fir_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = __builtin_ctzll(temp);
                const uint64_t pos = (1ULL << bit_pos); // front -> back

                field_t temp_field = *position;

                temp_field.firewall_fir = (uint8_t)((bit_pos << 1) | 1);

                WRITE_MOVE();

                clear_lowest_set_bit(temp, pos);
            }

            temp = (fir_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

            if (temp)
            {
                const int bit_pos = __builtin_ctzll(temp);

                field_t temp_field = *position;

                temp_field.firewall_fir = (uint8_t)((bit_pos << 1) | 1);

                WRITE_MOVE();
            }
        }
        else
        {
            field_t temp_field = *position;

            temp_field.firewall_fir = 0;

            WRITE_MOVE();
        }

        if (position->is_swap_available_fir)
        {
            uint64_t link_mask = fir_link_mask;

            while (link_mask)
            {
                const uint64_t link_pos = (1ULL << __builtin_ctzll(link_mask)); // front -> back

                uint64_t virus_mask = fir_virus_mask;

                while (virus_mask)
                {
                    const uint64_t virus_pos = (1ULL << __builtin_ctzll(virus_mask)); // front -> back

                    field_t temp_field = *position;

                    temp_field.is_swap_available_fir = 0;
                    temp_field.is_link_mask ^= (link_pos | virus_pos);

                    WRITE_MOVE();

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
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                field_t temp_field = *position;

                temp_field.is_boosted_mask |= pos;

                WRITE_MOVE();

                clear_lowest_set_bit(temp, pos);
            }
        }
        else
        {
            field_t temp_field = *position;

            temp_field.is_boosted_mask &= temp_field.is_sec_mask;

            WRITE_MOVE();
        }
    }
    else
    {
        const uint64_t fir_link_mask = position->is_link_mask & firmask;
        const uint64_t sec_link_mask = position->is_link_mask ^ fir_link_mask;
        const uint64_t sec_virus_mask = secmask ^ sec_link_mask;

        const uint64_t cur_boosted_mask = position->is_boosted_mask & secmask;
        const uint64_t enemy_firewall_mask = (uint64_t)(position->firewall_fir & 1) << (position->firewall_fir >> 1);
        const uint64_t free_mask = ~(firmask | secmask | enemy_firewall_mask);

        if (__builtin_expect(sec_link_mask & 1152921504606846976ULL, 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_sec -= (uint8_t)(__builtin_ctzll(1152921504606846976ULL) >> 3);
            temp_field.is_boosted_mask &= ~1152921504606846976ULL;
            temp_field.is_sec_mask &= ~1152921504606846976ULL;
            temp_field.is_link_mask &= ~1152921504606846976ULL;
            ++temp_field.sec_link;

            WRITE_MOVE();
        }

        if (__builtin_expect(sec_link_mask & 576460752303423488ULL, 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_sec -= (uint8_t)(__builtin_ctzll(576460752303423488ULL) >> 3);
            temp_field.is_boosted_mask &= ~576460752303423488ULL;
            temp_field.is_sec_mask &= ~576460752303423488ULL;
            temp_field.is_link_mask &= ~576460752303423488ULL;
            ++temp_field.sec_link;

            WRITE_MOVE();
        }

        const uint64_t boosted_link = cur_boosted_mask & sec_link_mask;

        if (__builtin_expect(((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 1729382256910270464ULL, 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_sec -= (uint8_t)(__builtin_ctzll(cur_boosted_mask) >> 3);
            temp_field.is_boosted_mask &= ~cur_boosted_mask;
            temp_field.is_sec_mask &= ~cur_boosted_mask;
            temp_field.is_link_mask &= ~cur_boosted_mask;
            ++temp_field.sec_link;

            WRITE_MOVE();
        }

        if (cur_boosted_mask == 0)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = *position;

                temp_field.is_boosted_mask |= pos;

                WRITE_MOVE();

                clear_highest_set_bit(temp, pos);
            }
        }
        else
        {
            const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask;
            const uint64_t cur_pos_bitboard = cur_boosted_mask;

            if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

                RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

                PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            const uint64_t legal_mask = (~enemy_firewall_mask) & (~links_masked_out) & (~secmask);

            if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

                RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

                PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }
        }

        uint64_t unboosted_cards_mask = sec_virus_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
            const uint64_t legal_mask = (firmask & (~enemy_firewall_mask));

            if (cur_pos_bitboard & (legal_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            if (cur_pos_bitboard & (legal_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        unboosted_cards_mask = sec_link_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
            const uint64_t legal_mask = (firmask & (~enemy_firewall_mask));

            if (cur_pos_bitboard & (legal_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            if (cur_pos_bitboard & (legal_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME(firmask & new_pos_bitboard);

                PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        unboosted_cards_mask = sec_link_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back

            if (cur_pos_bitboard & (free_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            if (cur_pos_bitboard & (free_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        unboosted_cards_mask = sec_virus_mask & (~cur_boosted_mask);

        while (unboosted_cards_mask)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back

            if (cur_pos_bitboard & (free_mask >> 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            if (cur_pos_bitboard & (free_mask << 8))
            {
                const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

                RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

                PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
        }

        if (position->firewall_sec == 0)
        {
            uint64_t temp = sec_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = 63 - __builtin_clzll(temp);
                const uint64_t pos = (1ULL << bit_pos); // front -> back

                field_t temp_field = *position;

                temp_field.firewall_sec = (uint8_t)((bit_pos << 1) | 1);

                WRITE_MOVE();

                clear_highest_set_bit(temp, pos);
            }

            temp = (sec_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

            if (temp)
            {
                const int bit_pos = 63 - __builtin_clzll(temp);

                field_t temp_field = *position;

                temp_field.firewall_sec = (uint8_t)((bit_pos << 1) | 1);

                WRITE_MOVE();
            }
        }
        else
        {
            field_t temp_field = *position;

            temp_field.firewall_sec = 0;

            WRITE_MOVE();
        }

        if (position->is_swap_available_sec)
        {
            uint64_t link_mask = sec_link_mask;

            while (link_mask)
            {
                const uint64_t link_pos = (1ULL << (63 - __builtin_clzll(link_mask))); // front -> back

                uint64_t virus_mask = sec_virus_mask;

                while (virus_mask)
                {
                    const uint64_t virus_pos = (1ULL << (63 - __builtin_clzll(virus_mask))); // front -> back

                    field_t temp_field = *position;

                    temp_field.is_swap_available_sec = 0;
                    temp_field.is_link_mask ^= (link_pos | virus_pos);

                    WRITE_MOVE();

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

                field_t temp_field = *position;

                temp_field.is_boosted_mask |= pos;

                WRITE_MOVE();

                clear_highest_set_bit(temp, pos);
            }
        }
        else
        {
            field_t temp_field = *position;

            temp_field.is_boosted_mask &= temp_field.is_fir_mask;

            WRITE_MOVE();
        }
    }

    return res;
}

#undef WRITE_MOVE
#undef PERFORM_ITERATION_FIR
#undef PERFORM_ITERATION_SEC

#define PERFORM_ITERATION_FIR(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                                                                                                                                                                                                          \
    if (secmask & new_pos_bitboard)                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                                                  \
        if (sec_link_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                                                          \
        {                                                                                                                                                                                                                                                                                                                                              \
            const uint64_t unknown_mask = position->is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                   \
            const field_t temp_field = {position->is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), position->is_sec_mask ^ new_pos_bitboard,                                                                                                                                                                                                       \
                                        ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (position->is_link_mask ^ cur_pos_bitboard) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (position->is_link_mask ^ new_pos_bitboard) : ((position->is_link_mask ^ new_pos_bitboard ^ unknown_mask) | (unknown_mask shift_func shift_count)))), \
                                        ((is_boosted) ? ((position->is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (position->is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                        \
                                        position->forward_adv_fir + (uint8_t)forward_adv, position->forward_adv_sec - (uint8_t)(__builtin_ctzll(new_pos_bitboard) >> 3),                                                                                                                                                                               \
                                        position->firewall_fir, position->firewall_sec,                                                                                                                                                                                                                                                                \
                                        position->fir_link + 1, position->sec_link, position->fir_virus, position->sec_virus,                                                                                                                                                                                                                          \
                                        position->is_swap_available_fir, position->is_swap_available_sec};                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                       \
            BRANCH_ENTER_MAX("capture link");                                                                                                                                                                                                                                                                                                          \
            int reschild = (depth != 0) ? minimax_min(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);                                                                                                                                                                                                                                  \
            BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                                         \
            score = (reschild > score) ? reschild : score;                                                                                                                                                                                                                                                                                             \
            alpha = (reschild > alpha) ? reschild : alpha;                                                                                                                                                                                                                                                                                             \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                                         \
            {                                                                                                                                                                                                                                                                                                                                          \
                TRACK_ENTRY_MAX();                                                                                                                                                                                                                                                                                                                     \
            }                                                                                                                                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                                                                                                                              \
        else if (position->fir_virus < 3)                                                                                                                                                                                                                                                                                                              \
        {                                                                                                                                                                                                                                                                                                                                              \
            const uint64_t unknown_mask = position->is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                   \
            const field_t temp_field = {position->is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), position->is_sec_mask ^ new_pos_bitboard,                                                                                                                                                                                                       \
                                        ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (position->is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (position->is_link_mask) : ((position->is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                  \
                                        ((is_boosted) ? ((position->is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (position->is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                        \
                                        position->forward_adv_fir + (uint8_t)forward_adv, position->forward_adv_sec - (uint8_t)(__builtin_ctzll(new_pos_bitboard) >> 3),                                                                                                                                                                               \
                                        position->firewall_fir, position->firewall_sec,                                                                                                                                                                                                                                                                \
                                        position->fir_link, position->sec_link, position->fir_virus + 1, position->sec_virus,                                                                                                                                                                                                                          \
                                        position->is_swap_available_fir, position->is_swap_available_sec};                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                       \
            int reschild;                                                                                                                                                                                                                                                                                                                              \
            if (is_boosted || current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                                                                                                                                                                                                         \
            {                                                                                                                                                                                                                                                                                                                                          \
                BRANCH_ENTER_MAX("capture virus boosted || link");                                                                                                                                                                                                                                                                                     \
                reschild = (depth > 0) ? minimax_min(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);                                                                                                                                                                                                                                   \
                BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                                     \
            }                                                                                                                                                                                                                                                                                                                                          \
            else                                                                                                                                                                                                                                                                                                                                       \
            {                                                                                                                                                                                                                                                                                                                                          \
                BRANCH_ENTER_MAX("capture virus not boosted");                                                                                                                                                                                                                                                                                         \
                if (depth > 0)                                                                                                                                                                                                                                                                                                                         \
                {                                                                                                                                                                                                                                                                                                                                      \
                    reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);                                                                                                                                                                                                                                                                      \
                    if (reschild > alpha && beta > alpha + 1)                                                                                                                                                                                                                                                                                          \
                        reschild = minimax_min(depth, alpha, beta, &temp_field);                                                                                                                                                                                                                                                                       \
                }                                                                                                                                                                                                                                                                                                                                      \
                else                                                                                                                                                                                                                                                                                                                                   \
                    reschild = field_evaluate(&temp_field);                                                                                                                                                                                                                                                                                            \
                                                                                                                                                                                                                                                                                                                                                       \
                BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                                     \
            }                                                                                                                                                                                                                                                                                                                                          \
            score = (reschild > score) ? reschild : score;                                                                                                                                                                                                                                                                                             \
            alpha = (reschild > alpha) ? reschild : alpha;                                                                                                                                                                                                                                                                                             \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                                         \
            {                                                                                                                                                                                                                                                                                                                                          \
                TRACK_ENTRY_MAX();                                                                                                                                                                                                                                                                                                                     \
            }                                                                                                                                                                                                                                                                                                                                          \
        }                                                                                                                                                                                                                                                                                                                                              \
    }                                                                                                                                                                                                                                                                                                                                                  \
    else if ((firmask & new_pos_bitboard) == 0)                                                                                                                                                                                                                                                                                                        \
    {                                                                                                                                                                                                                                                                                                                                                  \
        const uint64_t unknown_mask = position->is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                       \
        const field_t temp_field = {position->is_fir_mask ^ (cur_pos_bitboard | new_pos_bitboard), position->is_sec_mask,                                                                                                                                                                                                                              \
                                    ((current_card == ITERATION_CURRENT_IS_FIRST_LINK) ? (position->is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) ? (position->is_link_mask) : ((position->is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                      \
                                    ((is_boosted) ? (position->is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (position->is_boosted_mask)),                                                                                                                                                                                                \
                                    position->forward_adv_fir + (uint8_t)forward_adv, position->forward_adv_sec,                                                                                                                                                                                                                                       \
                                    position->firewall_fir, position->firewall_sec,                                                                                                                                                                                                                                                                    \
                                    position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,                                                                                                                                                                                                                                  \
                                    position->is_swap_available_fir, position->is_swap_available_sec};                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                                       \
        int reschild;                                                                                                                                                                                                                                                                                                                                  \
        if (is_boosted)                                                                                                                                                                                                                                                                                                                                \
        {                                                                                                                                                                                                                                                                                                                                              \
            BRANCH_ENTER_MAX("move boosted");                                                                                                                                                                                                                                                                                                          \
            reschild = (depth > 0) ? minimax_min(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);                                                                                                                                                                                                                                       \
            BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                                                                                                                                              \
        else                                                                                                                                                                                                                                                                                                                                           \
        {                                                                                                                                                                                                                                                                                                                                              \
            BRANCH_ENTER_MAX("move not boosted");                                                                                                                                                                                                                                                                                                      \
            if (depth > 0)                                                                                                                                                                                                                                                                                                                             \
            {                                                                                                                                                                                                                                                                                                                                          \
                reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);                                                                                                                                                                                                                                                                          \
                if (reschild > alpha && beta > alpha + 1)                                                                                                                                                                                                                                                                                              \
                    reschild = minimax_min(depth, alpha, beta, &temp_field);                                                                                                                                                                                                                                                                           \
            }                                                                                                                                                                                                                                                                                                                                          \
            else                                                                                                                                                                                                                                                                                                                                       \
                reschild = field_evaluate(&temp_field);                                                                                                                                                                                                                                                                                                \
                                                                                                                                                                                                                                                                                                                                                       \
            BRANCH_EXIT_MAX();                                                                                                                                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                                                                                                                                              \
        score = (reschild > score) ? reschild : score;                                                                                                                                                                                                                                                                                                 \
        alpha = (reschild > alpha) ? reschild : alpha;                                                                                                                                                                                                                                                                                                 \
        if (beta <= alpha)                                                                                                                                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                                                                                                                                              \
            TRACK_ENTRY_MAX();                                                                                                                                                                                                                                                                                                                         \
        }                                                                                                                                                                                                                                                                                                                                              \
    }

#define PERFORM_ITERATION_SEC(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                                                                                                                                                                                                            \
    if (firmask & new_pos_bitboard)                                                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                                                                                    \
        if (fir_link_mask & new_pos_bitboard)                                                                                                                                                                                                                                                                                                            \
        {                                                                                                                                                                                                                                                                                                                                                \
            const uint64_t unknown_mask = position->is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                     \
            const field_t temp_field = {position->is_fir_mask ^ new_pos_bitboard, position->is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                                         \
                                        ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (position->is_link_mask ^ cur_pos_bitboard) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (position->is_link_mask ^ new_pos_bitboard) : ((position->is_link_mask ^ new_pos_bitboard ^ unknown_mask) | (unknown_mask shift_func shift_count)))), \
                                        ((is_boosted) ? ((position->is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (position->is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                          \
                                        position->forward_adv_fir - (uint8_t)(7 - (__builtin_ctzll(new_pos_bitboard) >> 3)), position->forward_adv_sec + (uint8_t)forward_adv,                                                                                                                                                                           \
                                        position->firewall_fir, position->firewall_sec,                                                                                                                                                                                                                                                                  \
                                        position->fir_link, position->sec_link + 1, position->fir_virus, position->sec_virus,                                                                                                                                                                                                                            \
                                        position->is_swap_available_fir, position->is_swap_available_sec};                                                                                                                                                                                                                                               \
                                                                                                                                                                                                                                                                                                                                                         \
            BRANCH_ENTER_MIN("capture link");                                                                                                                                                                                                                                                                                                            \
            int reschild = (depth != 0) ? minimax_max(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);                                                                                                                                                                                                                                    \
            BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                           \
            score = (reschild < score) ? reschild : score;                                                                                                                                                                                                                                                                                               \
            beta = (reschild < beta) ? reschild : beta;                                                                                                                                                                                                                                                                                                  \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                                           \
            {                                                                                                                                                                                                                                                                                                                                            \
                TRACK_ENTRY_MIN();                                                                                                                                                                                                                                                                                                                       \
            }                                                                                                                                                                                                                                                                                                                                            \
        }                                                                                                                                                                                                                                                                                                                                                \
        else if (position->sec_virus < 3)                                                                                                                                                                                                                                                                                                                \
        {                                                                                                                                                                                                                                                                                                                                                \
            const uint64_t unknown_mask = position->is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                     \
            const field_t temp_field = {position->is_fir_mask ^ new_pos_bitboard, position->is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                                         \
                                        ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (position->is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (position->is_link_mask) : ((position->is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                  \
                                        ((is_boosted) ? ((position->is_boosted_mask ^ cur_pos_bitboard) | new_pos_bitboard) : (position->is_boosted_mask & ~new_pos_bitboard)),                                                                                                                                                                          \
                                        position->forward_adv_fir - (uint8_t)(7 - (__builtin_ctzll(new_pos_bitboard) >> 3)), position->forward_adv_sec + (uint8_t)forward_adv,                                                                                                                                                                           \
                                        position->firewall_fir, position->firewall_sec,                                                                                                                                                                                                                                                                  \
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus + 1,                                                                                                                                                                                                                            \
                                        position->is_swap_available_fir, position->is_swap_available_sec};                                                                                                                                                                                                                                               \
                                                                                                                                                                                                                                                                                                                                                         \
            int reschild;                                                                                                                                                                                                                                                                                                                                \
            if (is_boosted || current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                                                                                                                                                                                                          \
            {                                                                                                                                                                                                                                                                                                                                            \
                BRANCH_ENTER_MIN("capture virus boosted || link");                                                                                                                                                                                                                                                                                       \
                reschild = (depth > 0) ? minimax_max(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);                                                                                                                                                                                                                                     \
                BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                       \
            }                                                                                                                                                                                                                                                                                                                                            \
            else                                                                                                                                                                                                                                                                                                                                         \
            {                                                                                                                                                                                                                                                                                                                                            \
                BRANCH_ENTER_MIN("capture virus not boosted");                                                                                                                                                                                                                                                                                           \
                if (depth > 0)                                                                                                                                                                                                                                                                                                                           \
                {                                                                                                                                                                                                                                                                                                                                        \
                    reschild = minimax_max(depth, beta - 1, beta, &temp_field);                                                                                                                                                                                                                                                                          \
                    if (reschild < beta && alpha < beta - 1)                                                                                                                                                                                                                                                                                             \
                        reschild = minimax_max(depth, alpha, beta, &temp_field);                                                                                                                                                                                                                                                                         \
                }                                                                                                                                                                                                                                                                                                                                        \
                else                                                                                                                                                                                                                                                                                                                                     \
                    reschild = field_evaluate(&temp_field);                                                                                                                                                                                                                                                                                              \
                                                                                                                                                                                                                                                                                                                                                         \
                BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                       \
            }                                                                                                                                                                                                                                                                                                                                            \
            score = (reschild < score) ? reschild : score;                                                                                                                                                                                                                                                                                               \
            beta = (reschild < beta) ? reschild : beta;                                                                                                                                                                                                                                                                                                  \
            if (beta <= alpha)                                                                                                                                                                                                                                                                                                                           \
            {                                                                                                                                                                                                                                                                                                                                            \
                TRACK_ENTRY_MIN();                                                                                                                                                                                                                                                                                                                       \
            }                                                                                                                                                                                                                                                                                                                                            \
        }                                                                                                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                                                    \
    else if ((secmask & new_pos_bitboard) == 0)                                                                                                                                                                                                                                                                                                          \
    {                                                                                                                                                                                                                                                                                                                                                    \
        const uint64_t unknown_mask = position->is_link_mask & cur_pos_bitboard;                                                                                                                                                                                                                                                                         \
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask ^ (cur_pos_bitboard | new_pos_bitboard),                                                                                                                                                                                                                                \
                                    ((current_card == ITERATION_CURRENT_IS_SECOND_LINK) ? (position->is_link_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : ((current_card == ITERATION_CURRENT_IS_SECOND_VIRUS) ? (position->is_link_mask) : ((position->is_link_mask ^ unknown_mask) | (unknown_mask shift_func shift_count)))),                      \
                                    ((is_boosted) ? (position->is_boosted_mask ^ (cur_pos_bitboard | new_pos_bitboard)) : (position->is_boosted_mask)),                                                                                                                                                                                                  \
                                    position->forward_adv_fir, position->forward_adv_sec + (uint8_t)forward_adv,                                                                                                                                                                                                                                         \
                                    position->firewall_fir, position->firewall_sec,                                                                                                                                                                                                                                                                      \
                                    position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,                                                                                                                                                                                                                                    \
                                    position->is_swap_available_fir, position->is_swap_available_sec};                                                                                                                                                                                                                                                   \
                                                                                                                                                                                                                                                                                                                                                         \
        int reschild;                                                                                                                                                                                                                                                                                                                                    \
        if (is_boosted)                                                                                                                                                                                                                                                                                                                                  \
        {                                                                                                                                                                                                                                                                                                                                                \
            BRANCH_ENTER_MIN("move boosted");                                                                                                                                                                                                                                                                                                            \
            reschild = (depth > 0) ? minimax_max(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);                                                                                                                                                                                                                                         \
            BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                           \
        }                                                                                                                                                                                                                                                                                                                                                \
        else                                                                                                                                                                                                                                                                                                                                             \
        {                                                                                                                                                                                                                                                                                                                                                \
            BRANCH_ENTER_MIN("move not boosted");                                                                                                                                                                                                                                                                                                        \
            if (depth > 0)                                                                                                                                                                                                                                                                                                                               \
            {                                                                                                                                                                                                                                                                                                                                            \
                reschild = minimax_max(depth, beta - 1, beta, &temp_field);                                                                                                                                                                                                                                                                              \
                if (reschild < beta && alpha < beta - 1)                                                                                                                                                                                                                                                                                                 \
                    reschild = minimax_max(depth, alpha, beta, &temp_field);                                                                                                                                                                                                                                                                             \
            }                                                                                                                                                                                                                                                                                                                                            \
            else                                                                                                                                                                                                                                                                                                                                         \
                reschild = field_evaluate(&temp_field);                                                                                                                                                                                                                                                                                                  \
                                                                                                                                                                                                                                                                                                                                                         \
            BRANCH_EXIT_MIN();                                                                                                                                                                                                                                                                                                                           \
        }                                                                                                                                                                                                                                                                                                                                                \
        score = (reschild < score) ? reschild : score;                                                                                                                                                                                                                                                                                                   \
        beta = (reschild < beta) ? reschild : beta;                                                                                                                                                                                                                                                                                                      \
        if (beta <= alpha)                                                                                                                                                                                                                                                                                                                               \
        {                                                                                                                                                                                                                                                                                                                                                \
            TRACK_ENTRY_MIN();                                                                                                                                                                                                                                                                                                                           \
        }                                                                                                                                                                                                                                                                                                                                                \
    }

#ifdef BRANCH_DEBUG

#define BEGIN_BRANCH_TRACKING() \
    static const int _branch_counter_base = __COUNTER__

#define BRANCH_ENTER_MAX(MSG)                                                         \
    static const int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    const int64_t cur_rec_count_##__LINE__ = rec_counter;                             \
    cutoff_tracker[_branch_idx_##__LINE__].total_entries++;                           \
    cutoff_tracker[_branch_idx_##__LINE__].msg = MSG;                                 \
    cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries++;                          \
    cutoff_tracker[_branch_idx_##__LINE__].temp_score = alpha

#define BRANCH_ENTER_MIN(MSG)                                                         \
    static const int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    const int64_t cur_rec_count_##__LINE__ = rec_counter;                             \
    cutoff_tracker[_branch_idx_##__LINE__].total_entries++;                           \
    cutoff_tracker[_branch_idx_##__LINE__].msg = MSG;                                 \
    cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries++;                          \
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
    const int _total_branch_count = (__COUNTER__ - _branch_counter_base - 1)

#else

#define BEGIN_BRANCH_TRACKING()
#define BRANCH_ENTER_MAX(MSG)
#define BRANCH_ENTER_MIN(MSG)
#define BRANCH_EXIT_MAX()
#define BRANCH_EXIT_MIN()
#define END_BRANCH_TRACKING()

#endif

#define TRACK_ENTRY_MAX() \
    goto track_max;

#define TRACK_ENTRY_MIN() \
    goto track_min;

#define GET_CACHE_COUNT() 0

BEGIN_BRANCH_TRACKING();

static __attribute__((hot)) int minimax_min(int depth, int alpha, int beta, const field_t *__restrict__ position);

static __attribute__((hot)) int minimax_max(int depth, int alpha, int beta, const field_t *__restrict__ position)
{
#ifdef BRANCH_DEBUG
    ++rec_counter;
#endif
    RNAB_ASSUME(depth > 0);

    const uint64_t firmask = position->is_fir_mask, secmask = position->is_sec_mask;
    const uint64_t fir_link_mask = position->is_link_mask & firmask;
    const uint64_t fir_virus_mask = firmask ^ fir_link_mask;
    const uint64_t sec_link_mask = position->is_link_mask ^ fir_link_mask;

    const uint64_t cur_boosted_mask = position->is_boosted_mask & firmask;
    const uint64_t enemy_firewall_mask = (uint64_t)(position->firewall_sec & 1) << (position->firewall_sec >> 1);
    const uint64_t free_mask = ~(firmask | secmask | enemy_firewall_mask);
    const uint64_t boosted_link = cur_boosted_mask & fir_link_mask;
    const uint64_t links_masked_out = sec_link_mask & ~enemy_firewall_mask;

    int score = MIN;

    RNAB_ASSUME(position->fir_link <= 3);
    RNAB_ASSUME(position->fir_virus <= 3);
    RNAB_ASSUME(position->sec_link <= 3);
    RNAB_ASSUME(position->sec_virus <= 3);

    if (position->fir_link == 3) // fast path if we are about to win
    {
        // there is either a link at an exit square or a boosted link which can reach it
        if ((fir_link_mask | (((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask)) & 24ULL)
            return (4096 * depth);

        if (links_masked_out)
        {
            // check if any of the cards can simply reach unprotected enemy link in one move
#ifdef __AVX2__
            __m256i tgt = _mm256_set1_epi64x(links_masked_out);

            __m256i shuffled = _mm256_and_si256(_mm256_blend_epi32(_mm256_or_si256(_mm256_srli_si256(tgt, 9), _mm256_srli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, CAN_MOVE_RIGHT, 0, 0)), 1)), _mm256_or_si256(_mm256_slli_si256(tgt, 9), _mm256_slli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, 0, CAN_MOVE_LEFT, 0)), 1)), 0b11110000), _mm256_set1_epi64x(firmask));

            if (!_mm256_testz_si256(shuffled, shuffled))
#else
            if (((links_masked_out >> 8) | (links_masked_out << 8) | ((links_masked_out & CAN_MOVE_RIGHT) >> 1) | ((links_masked_out & CAN_MOVE_LEFT) << 1)) & firmask)
#endif
                return (4096 * depth);

            // now let's check if we can reach any of the unprotected links with a boosted card if there is one
            if (cur_boosted_mask)
            {
                // it looks like SIMD check is almost 3 times faster than the scalar one
#ifdef __AVX2__
                /*
                links_masked_out & CAN_MOVE_BACKWARD_LEFT  & (cur_boosted_mask << 7)  & ((free_mask >> 1) | (free_mask << 8))
                links_masked_out & CAN_MOVE_BACKWARD_RIGHT & (cur_boosted_mask << 9)  & ((free_mask << 1) | (free_mask << 8))
                links_masked_out & -1                      & (cur_boosted_mask << 16) & (0                | (free_mask << 8))
                links_masked_out & CAN_MOVE_DOUBLE_RIGHT   & (cur_boosted_mask << 2)  & ((free_mask << 1) | 0               )


                links_masked_out & CAN_MOVE_FORWARD_LEFT   & (cur_boosted_mask >> 9)  & ((free_mask >> 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_FORWARD_RIGHT  & (cur_boosted_mask >> 7)  & ((free_mask << 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_DOUBLE_LEFT    & (cur_boosted_mask >> 2)  & ((free_mask >> 1) | 0               )
                links_masked_out & -1                      & (cur_boosted_mask >> 16) & (0                | (free_mask >> 8))
                */

                const __m256i zero_mask = _mm256_setzero_si256();

                __m256i fm = _mm256_set1_epi64x(free_mask);
                __m256i cm = _mm256_set1_epi64x(cur_boosted_mask);
                __m256i shift_comb = _mm256_blend_epi32(_mm256_slli_epi64(fm, 1), _mm256_srli_epi64(fm, 1), 0b00110011);

                if (!_mm256_testz_si256(_mm256_or_si256(_mm256_and_si256(_mm256_and_si256(_mm256_sllv_epi64(cm, _mm256_setr_epi64x(7, 9, 16, 2)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(-1, 0, 1, 2, 3, 4, 5, 6, -1, 8, 9, 10, 11, 12, 13, 14, -1, 16, 17, 18, 19, 20, 21, 22, -1, -1, -1, -1, -1, -1, -1, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b11001111))), _mm256_setr_epi64x(CAN_MOVE_BACKWARD_LEFT, CAN_MOVE_BACKWARD_RIGHT, -1, CAN_MOVE_DOUBLE_RIGHT)), _mm256_and_si256(_mm256_and_si256(_mm256_srlv_epi64(cm, _mm256_setr_epi64x(9, 7, 2, 16)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(1, 2, 3, 4, 5, 6, 7, -1, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, 26, 27, 28, 29, 30, 31, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b00111111))), _mm256_setr_epi64x(CAN_MOVE_FORWARD_LEFT, CAN_MOVE_FORWARD_RIGHT, CAN_MOVE_DOUBLE_LEFT, -1))), tgt))
#else
                if (links_masked_out & (((free_mask << 8) & (cur_boosted_mask << 16)) |                                               // down and not blocked
                                        ((free_mask >> 8) & (cur_boosted_mask >> 16)) |                                               // up and not blocked
                                        (CAN_MOVE_DOUBLE_RIGHT & (cur_boosted_mask << 2) & (free_mask << 1)) |                        // right and not blocked
                                        (CAN_MOVE_DOUBLE_LEFT & (cur_boosted_mask >> 2) & (free_mask >> 1)) |                         // left and not blocked
                                        (CAN_MOVE_BACKWARD_LEFT & (cur_boosted_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) |  // down left
                                        (CAN_MOVE_BACKWARD_RIGHT & (cur_boosted_mask << 9) & ((free_mask << 1) | (free_mask << 8))) | // down right
                                        (CAN_MOVE_FORWARD_LEFT & (cur_boosted_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) |   // up left
                                        (CAN_MOVE_FORWARD_RIGHT & (cur_boosted_mask >> 7) & ((free_mask << 1) | (free_mask >> 8)))))  // up right
#endif
                    return (4096 * depth);
            }
        }
    }

    --depth;

    int alphabeg = alpha;

    // check if the move is cached
    if (depth > MIN_CACHE_DEPTH)
    {
        uint64_t hash = rapidhashNano((const void *__restrict__)position, 0);
        tt_bucket_t *__restrict__ bucket = &tt_fir[hash & (TABLE_SIZE - 1)];
        tt_entry_t *__restrict__ entry = (bucket->depth_preferred.hash_entry == hash) ? &bucket->depth_preferred : ((bucket->scratch.hash_entry == hash) ? &bucket->scratch : NULL);

        if (entry && entry->depth >= depth)
        {
            if (entry->flag == TT_EXACT)
            {
                return entry->eval;
            }
            else if (entry->flag == TT_LOWERBOUND)
            {
                if (entry->eval >= beta)
                    return entry->eval;
                alpha = (entry->eval > alpha) ? entry->eval : alpha;
            }
            else if (entry->flag == TT_UPPERBOUND)
            {
                if (entry->eval <= alpha)
                    return entry->eval;
                beta = (entry->eval < beta) ? entry->eval : beta;
            }
        }
    }

    if (__builtin_expect(fir_link_mask & 8ULL, 0))
    {
        const field_t temp_field = {position->is_fir_mask & ~8ULL, position->is_sec_mask, position->is_link_mask & ~8ULL, position->is_boosted_mask & ~8ULL,
                                    position->forward_adv_fir - (uint8_t)(7 - (__builtin_ctzll(8ULL) >> 3)), position->forward_adv_sec,
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link + 1, position->sec_link, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MAX("deposit close");
        int reschild = (depth != 0) ? minimax_min(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }

    if (__builtin_expect(fir_link_mask & 16ULL, 0))
    {
        const field_t temp_field = {position->is_fir_mask & ~16ULL, position->is_sec_mask, position->is_link_mask & ~16ULL, position->is_boosted_mask & ~16ULL,
                                    position->forward_adv_fir - (uint8_t)(7 - (__builtin_ctzll(16ULL) >> 3)), position->forward_adv_sec,
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link + 1, position->sec_link, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MAX("deposit close");
        int reschild = (depth != 0) ? minimax_min(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }

    if (__builtin_expect(((boosted_link >> 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 24ULL, 0))
    {
        const field_t temp_field = {position->is_fir_mask & ~cur_boosted_mask, position->is_sec_mask, position->is_link_mask & ~cur_boosted_mask, position->is_boosted_mask & ~cur_boosted_mask,
                                    position->forward_adv_fir - (uint8_t)(7 - (__builtin_ctzll(cur_boosted_mask) >> 3)), position->forward_adv_sec,
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link + 1, position->sec_link, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MAX("deposit far");
        int reschild = (depth != 0) ? minimax_min(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = fir_virus_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask | pos,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        position->firewall_fir, position->firewall_sec,
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MAX("boost virus");
            int reschild = (depth != 0) ? minimax_min(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
            BRANCH_EXIT_MAX();

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

        if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((secmask & new_pos_bitboard) && (sec_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        const uint64_t legal_mask = (~enemy_firewall_mask) & (~links_masked_out) & (~firmask);

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((sec_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & firmask) == 0);

            PERFORM_ITERATION_FIR(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
        }
    }

    uint64_t unboosted_cards_mask = fir_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));
        const uint64_t legal_mask = (secmask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    unboosted_cards_mask = fir_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));
        const uint64_t legal_mask = (secmask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(secmask & new_pos_bitboard);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    unboosted_cards_mask = fir_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    unboosted_cards_mask = fir_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(unboosted_cards_mask));

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_FIR(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
        }

        clear_lowest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    if (position->firewall_fir == 0)
    {
        uint64_t temp = fir_link_mask & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = __builtin_ctzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        (uint8_t)((bit_pos << 1) | 1), position->firewall_sec,
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MAX("firewall link");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);
                if (reschild > alpha && beta > alpha + 1)
                    reschild = minimax_min(depth, alpha, beta, &temp_field);
            }
            else
                reschild = field_evaluate(&temp_field);
            BRANCH_EXIT_MAX();

            score = (reschild > score) ? reschild : score;
            alpha = (reschild > alpha) ? reschild : alpha;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MAX();
            }

            clear_lowest_set_bit(temp, pos);
        }

        temp = (fir_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

        if (temp)
        {
            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        (uint8_t)((__builtin_ctzll(temp) << 1) | 1), position->firewall_sec,
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MAX("firewall boosted virus");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);
                if (reschild > alpha && beta > alpha + 1)
                    reschild = minimax_min(depth, alpha, beta, &temp_field);
            }
            else
                reschild = field_evaluate(&temp_field);
            BRANCH_EXIT_MAX();

            score = (reschild > score) ? reschild : score;
            alpha = (reschild > alpha) ? reschild : alpha;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MAX();
            }
        }
    }
    else
    {
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask,
                                    position->forward_adv_fir, position->forward_adv_sec,
                                    0, position->firewall_sec,
                                    position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MAX("un-firewall");
        int reschild;
        if (depth > 0)
        {
            reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);
            if (reschild > alpha && beta > alpha + 1)
                reschild = minimax_min(depth, alpha, beta, &temp_field);
        }
        else
            reschild = field_evaluate(&temp_field);
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }

    if (position->is_swap_available_fir)
    {
        uint64_t link_mask = fir_link_mask;

        while (link_mask)
        {
            const uint64_t link_pos = (1ULL << __builtin_ctzll(link_mask)); // front -> back

            uint64_t virus_mask = fir_virus_mask;

            while (virus_mask)
            {
                const uint64_t virus_pos = (1ULL << __builtin_ctzll(virus_mask)); // front -> back

                const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask ^ (link_pos | virus_pos), position->is_boosted_mask,
                                            position->forward_adv_fir, position->forward_adv_sec,
                                            position->firewall_fir, position->firewall_sec,
                                            position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                            0, position->is_swap_available_sec};

                BRANCH_ENTER_MAX("swap");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);
                    if (reschild > alpha && beta > alpha + 1)
                        reschild = minimax_min(depth, alpha, beta, &temp_field);
                }
                else
                    reschild = field_evaluate(&temp_field);
                BRANCH_EXIT_MAX();

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

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = fir_link_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask | pos,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        position->firewall_fir, position->firewall_sec,
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MAX("boost link");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);
                if (reschild > alpha && beta > alpha + 1)
                    reschild = minimax_min(depth, alpha, beta, &temp_field);
            }
            else
                reschild = field_evaluate(&temp_field);
            BRANCH_EXIT_MAX();

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
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask & temp_field.is_sec_mask,
                                    position->forward_adv_fir, position->forward_adv_sec,
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MAX("un-boost");
        int reschild;
        if (depth > 0)
        {
            reschild = minimax_min(depth, alpha, alpha + 1, &temp_field);
            if (reschild > alpha && beta > alpha + 1)
                reschild = minimax_min(depth, alpha, beta, &temp_field);
        }
        else
            reschild = field_evaluate(&temp_field);
        BRANCH_EXIT_MAX();

        score = (reschild > score) ? reschild : score;
        alpha = (reschild > alpha) ? reschild : alpha;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MAX();
        }
    }
    if (depth > MIN_CACHE_DEPTH)
    {
        uint64_t hash = rapidhashNano((const void *__restrict__)position, 0);
        tt_bucket_t *__restrict__ bucket = &tt_fir[hash & (TABLE_SIZE - 1)];
        *(tt_entry_t *__restrict__)((depth >= bucket->depth_preferred.depth) ? &bucket->depth_preferred : &bucket->scratch) = (tt_entry_t){hash, score, (uint8_t)depth, (uint8_t)((score <= alphabeg) ? TT_UPPERBOUND : TT_EXACT)};
    }

    return score;

track_max:
    if (depth > MIN_CACHE_DEPTH)
    {
        uint64_t hash = rapidhashNano((const void *__restrict__)position, 0);
        tt_bucket_t *__restrict__ bucket = &tt_fir[hash & (TABLE_SIZE - 1)];
        *(tt_entry_t *__restrict__)((depth >= bucket->depth_preferred.depth) ? &bucket->depth_preferred : &bucket->scratch) = (tt_entry_t){hash, score, (uint8_t)depth, TT_LOWERBOUND};
    }

    return score;
}

static __attribute__((hot)) int minimax_min(int depth, int alpha, int beta, const field_t *__restrict__ position)
{
#ifdef BRANCH_DEBUG
    ++rec_counter;
#endif
    RNAB_ASSUME(depth > 0);

    const uint64_t firmask = position->is_fir_mask, secmask = position->is_sec_mask;
    const uint64_t fir_link_mask = position->is_link_mask & firmask;
    const uint64_t sec_link_mask = position->is_link_mask ^ fir_link_mask;
    const uint64_t sec_virus_mask = secmask ^ sec_link_mask;

    const uint64_t cur_boosted_mask = position->is_boosted_mask & secmask;
    const uint64_t enemy_firewall_mask = (uint64_t)(position->firewall_fir & 1) << (position->firewall_fir >> 1);
    const uint64_t free_mask = ~(firmask | secmask | enemy_firewall_mask);
    const uint64_t boosted_link = cur_boosted_mask & sec_link_mask;
    const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask;

    int score = MAX;

    RNAB_ASSUME(position->fir_link <= 3);
    RNAB_ASSUME(position->fir_virus <= 3);
    RNAB_ASSUME(position->sec_link <= 3);
    RNAB_ASSUME(position->sec_virus <= 3);

    if (position->sec_link == 3) // fast path if we are about to win
    {
        // there is either a link at an exit square or a boosted link which can reach it
        if ((sec_link_mask | (((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask)) & 1729382256910270464ULL)
            return (-4096 * depth);
        if (links_masked_out)
        {
            // check if any of the cards can simply reach unprotected enemy link in one move
#ifdef __AVX2__
            __m256i tgt = _mm256_set1_epi64x(links_masked_out);

            __m256i shuffled = _mm256_and_si256(_mm256_blend_epi32(_mm256_or_si256(_mm256_srli_si256(tgt, 9), _mm256_srli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, CAN_MOVE_RIGHT, 0, 0)), 1)), _mm256_or_si256(_mm256_slli_si256(tgt, 9), _mm256_slli_epi64(_mm256_and_si256(tgt, _mm256_setr_epi64x(0, 0, CAN_MOVE_LEFT, 0)), 1)), 0b11110000), _mm256_set1_epi64x(secmask));

            if (!_mm256_testz_si256(shuffled, shuffled))
#else
            if (((links_masked_out >> 8) | (links_masked_out << 8) | ((links_masked_out & CAN_MOVE_RIGHT) >> 1) | ((links_masked_out & CAN_MOVE_LEFT) << 1)) & secmask)
#endif
                return (-4096 * depth);

            // now let's check if we can reach any of the unprotected links with a boosted card if there is one
            if (cur_boosted_mask)
            {
                // it looks like SIMD check is almost 3 times faster than the scalar one
#ifdef __AVX2__
                /*
                links_masked_out & CAN_MOVE_BACKWARD_LEFT  & (cur_boosted_mask << 7)  & ((free_mask >> 1) | (free_mask << 8))
                links_masked_out & CAN_MOVE_BACKWARD_RIGHT & (cur_boosted_mask << 9)  & ((free_mask << 1) | (free_mask << 8))
                links_masked_out & -1                      & (cur_boosted_mask << 16) & (0                | (free_mask << 8))
                links_masked_out & CAN_MOVE_DOUBLE_RIGHT   & (cur_boosted_mask << 2)  & ((free_mask << 1) | 0               )


                links_masked_out & CAN_MOVE_FORWARD_LEFT   & (cur_boosted_mask >> 9)  & ((free_mask >> 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_FORWARD_RIGHT  & (cur_boosted_mask >> 7)  & ((free_mask << 1) | (free_mask >> 8))
                links_masked_out & CAN_MOVE_DOUBLE_LEFT    & (cur_boosted_mask >> 2)  & ((free_mask >> 1) | 0               )
                links_masked_out & -1                      & (cur_boosted_mask >> 16) & (0                | (free_mask >> 8))
                */

                const __m256i zero_mask = _mm256_setzero_si256();

                __m256i fm = _mm256_set1_epi64x(free_mask);
                __m256i cm = _mm256_set1_epi64x(cur_boosted_mask);
                __m256i shift_comb = _mm256_blend_epi32(_mm256_slli_epi64(fm, 1), _mm256_srli_epi64(fm, 1), 0b00110011);

                if (!_mm256_testz_si256(_mm256_or_si256(_mm256_and_si256(_mm256_and_si256(_mm256_sllv_epi64(cm, _mm256_setr_epi64x(7, 9, 16, 2)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(-1, 0, 1, 2, 3, 4, 5, 6, -1, 8, 9, 10, 11, 12, 13, 14, -1, 16, 17, 18, 19, 20, 21, 22, -1, -1, -1, -1, -1, -1, -1, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b11001111))), _mm256_setr_epi64x(CAN_MOVE_BACKWARD_LEFT, CAN_MOVE_BACKWARD_RIGHT, -1, CAN_MOVE_DOUBLE_RIGHT)), _mm256_and_si256(_mm256_and_si256(_mm256_srlv_epi64(cm, _mm256_setr_epi64x(9, 7, 2, 16)), _mm256_or_si256(_mm256_shuffle_epi8(fm, _mm256_setr_epi8(1, 2, 3, 4, 5, 6, 7, -1, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, 25, 26, 27, 28, 29, 30, 31, -1)), _mm256_blend_epi32(zero_mask, shift_comb, 0b00111111))), _mm256_setr_epi64x(CAN_MOVE_FORWARD_LEFT, CAN_MOVE_FORWARD_RIGHT, CAN_MOVE_DOUBLE_LEFT, -1))), tgt))
#else
                if (links_masked_out & (((free_mask << 8) & (cur_boosted_mask << 16)) |                                               // down and not blocked
                                        ((free_mask >> 8) & (cur_boosted_mask >> 16)) |                                               // up and not blocked
                                        (CAN_MOVE_DOUBLE_RIGHT & (cur_boosted_mask << 2) & (free_mask << 1)) |                        // right and not blocked
                                        (CAN_MOVE_DOUBLE_LEFT & (cur_boosted_mask >> 2) & (free_mask >> 1)) |                         // left and not blocked
                                        (CAN_MOVE_BACKWARD_LEFT & (cur_boosted_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) |  // down left
                                        (CAN_MOVE_BACKWARD_RIGHT & (cur_boosted_mask << 9) & ((free_mask << 1) | (free_mask << 8))) | // down right
                                        (CAN_MOVE_FORWARD_LEFT & (cur_boosted_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) |   // up left
                                        (CAN_MOVE_FORWARD_RIGHT & (cur_boosted_mask >> 7) & ((free_mask << 1) | (free_mask >> 8)))))  // up right
#endif
                    return (-4096 * depth);
            }
        }
    }

    --depth;

    int betabeg = beta;

    // check if the move is cached
    if (depth > MIN_CACHE_DEPTH)
    {
        uint64_t hash = rapidhashNano((const void *__restrict__)position, 0);
        tt_bucket_t *__restrict__ bucket = &tt_sec[hash & (TABLE_SIZE - 1)];
        tt_entry_t *__restrict__ entry = (bucket->depth_preferred.hash_entry == hash) ? &bucket->depth_preferred : ((bucket->scratch.hash_entry == hash) ? &bucket->scratch : NULL);

        if (entry && entry->depth >= depth)
        {
            if (entry->flag == TT_EXACT)
            {
                return entry->eval;
            }
            else if (entry->flag == TT_LOWERBOUND)
            {
                if (entry->eval >= beta)
                    return entry->eval;
                alpha = (entry->eval > alpha) ? entry->eval : alpha;
            }
            else if (entry->flag == TT_UPPERBOUND)
            {
                if (entry->eval <= alpha)
                    return entry->eval;
                beta = (entry->eval < beta) ? entry->eval : beta;
            }
        }
    }

    if (__builtin_expect(sec_link_mask & 1152921504606846976ULL, 0))
    {
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask & ~1152921504606846976ULL, position->is_link_mask & ~1152921504606846976ULL, position->is_boosted_mask & ~1152921504606846976ULL,
                                    position->forward_adv_fir, position->forward_adv_sec - (uint8_t)(__builtin_ctzll(1152921504606846976ULL) >> 3),
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link, position->sec_link + 1, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MIN("deposit close");
        int reschild = (depth != 0) ? minimax_max(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }

    if (__builtin_expect(sec_link_mask & 576460752303423488ULL, 0))
    {
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask & ~576460752303423488ULL, position->is_link_mask & ~576460752303423488ULL, position->is_boosted_mask & ~576460752303423488ULL,
                                    position->forward_adv_fir, position->forward_adv_sec - (uint8_t)(__builtin_ctzll(576460752303423488ULL) >> 3),
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link, position->sec_link + 1, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MIN("deposit close");
        int reschild = (depth != 0) ? minimax_max(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }

    if (__builtin_expect(((boosted_link << 8) | (boosted_link >> 1) | (boosted_link << 1)) & free_mask & 1729382256910270464ULL, 0))
    {
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask & ~cur_boosted_mask, position->is_link_mask & ~cur_boosted_mask, position->is_boosted_mask & ~cur_boosted_mask,
                                    position->forward_adv_fir, position->forward_adv_sec - (uint8_t)(__builtin_ctzll(cur_boosted_mask) >> 3),
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link, position->sec_link + 1, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MIN("deposit far");
        int reschild = (depth != 0) ? minimax_max(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = sec_virus_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask | pos,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        position->firewall_fir, position->firewall_sec,
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MIN("boost virus");
            int reschild = (depth != 0) ? minimax_max(depth, alpha, beta, &temp_field) : field_evaluate(&temp_field);
            BRANCH_EXIT_MIN();

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

        if (((cur_pos_bitboard & (free_mask >> 8) & (links_masked_out >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (links_masked_out >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (links_masked_out >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (links_masked_out << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (links_masked_out << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (links_masked_out >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (links_masked_out >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (links_masked_out << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (links_masked_out << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (links_masked_out << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask << 8) & (links_masked_out << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((firmask & new_pos_bitboard) && (fir_link_mask & new_pos_bitboard));

            PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        const uint64_t legal_mask = (~enemy_firewall_mask) & (~links_masked_out) & (~secmask);

        if (((cur_pos_bitboard & (free_mask >> 8) & (legal_mask >> 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 16);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask >> 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_RIGHT & (legal_mask >> 7) & ((free_mask << 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 7);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_FORWARD_LEFT & (legal_mask >> 9) & ((free_mask >> 1) | (free_mask >> 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 9);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_RIGHT & (legal_mask << 2) & (free_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 2);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_DOUBLE_LEFT & (free_mask >> 1) & (legal_mask >> 2)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 2);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if ((cur_pos_bitboard & (legal_mask << 8)) != 0)
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_RIGHT & (legal_mask << 9) & ((free_mask << 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 9);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & CAN_MOVE_BACKWARD_LEFT & (legal_mask << 7) & ((free_mask >> 1) | (free_mask << 8))) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 7);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }

        if (((cur_pos_bitboard & (free_mask << 8) & (legal_mask << 16)) != 0))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 16);

            RNAB_ASSUME((fir_link_mask & new_pos_bitboard) == 0 && (new_pos_bitboard & secmask) == 0);

            PERFORM_ITERATION_SEC(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
        }
    }

    uint64_t unboosted_cards_mask = sec_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
        const uint64_t legal_mask = (firmask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    unboosted_cards_mask = sec_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back
        const uint64_t legal_mask = (firmask & (~enemy_firewall_mask));

        if (cur_pos_bitboard & (legal_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (legal_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (legal_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & (legal_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME(firmask & new_pos_bitboard);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    unboosted_cards_mask = sec_link_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    unboosted_cards_mask = sec_virus_mask & (~cur_boosted_mask);

    RNAB_ASSUME(__builtin_popcountll(unboosted_cards_mask) >= 0 && __builtin_popcountll(unboosted_cards_mask) <= 4);

    while (unboosted_cards_mask)
    {
        const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(unboosted_cards_mask))); // front -> back

        if (cur_pos_bitboard & (free_mask >> 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_RIGHT & (free_mask << 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & CAN_MOVE_LEFT & (free_mask >> 1))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard << 1);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        if (cur_pos_bitboard & (free_mask << 8))
        {
            const uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);

            RNAB_ASSUME((firmask & new_pos_bitboard) == 0 && (secmask & new_pos_bitboard) == 0);

            PERFORM_ITERATION_SEC(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
        }

        clear_highest_set_bit(unboosted_cards_mask, cur_pos_bitboard);
    }

    if (position->firewall_sec == 0)
    {
        uint64_t temp = sec_link_mask & 16717361816799281127ULL;

        while (temp)
        {
            const int bit_pos = 63 - __builtin_clzll(temp);
            const uint64_t pos = (1ULL << bit_pos); // front -> back

            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        position->firewall_fir, (uint8_t)((bit_pos << 1) | 1),
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MIN("firewall link");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax_max(depth, beta - 1, beta, &temp_field);
                if (reschild < beta && alpha < beta - 1)
                    reschild = minimax_max(depth, alpha, beta, &temp_field);
            }
            else
                reschild = field_evaluate(&temp_field);
            BRANCH_EXIT_MIN();

            score = (reschild < score) ? reschild : score;
            beta = (reschild < beta) ? reschild : beta;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MIN();
            }

            clear_highest_set_bit(temp, pos);
        }

        temp = (sec_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

        if (temp)
        {
            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        position->firewall_fir, (uint8_t)(((63 - __builtin_clzll(temp)) << 1) | 1),
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MIN("firewall virus");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax_max(depth, beta - 1, beta, &temp_field);
                if (reschild < beta && alpha < beta - 1)
                    reschild = minimax_max(depth, alpha, beta, &temp_field);
            }
            else
                reschild = field_evaluate(&temp_field);
            BRANCH_EXIT_MIN();

            score = (reschild < score) ? reschild : score;
            beta = (reschild < beta) ? reschild : beta;

            if (beta <= alpha)
            {
                TRACK_ENTRY_MIN();
            }
        }
    }
    else
    {
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask,
                                    position->forward_adv_fir, position->forward_adv_sec,
                                    position->firewall_fir, 0,
                                    position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MIN("un-firewall");
        int reschild;
        if (depth > 0)
        {
            reschild = minimax_max(depth, beta - 1, beta, &temp_field);
            if (reschild < beta && alpha < beta - 1)
                reschild = minimax_max(depth, alpha, beta, &temp_field);
        }
        else
            reschild = field_evaluate(&temp_field);
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }

    if (position->is_swap_available_sec)
    {
        uint64_t link_mask = sec_link_mask;

        while (link_mask)
        {
            const uint64_t link_pos = (1ULL << (63 - __builtin_clzll(link_mask))); // front -> back

            uint64_t virus_mask = sec_virus_mask;

            while (virus_mask)
            {
                const uint64_t virus_pos = (1ULL << (63 - __builtin_clzll(virus_mask))); // front -> back

                const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask ^ (link_pos | virus_pos), position->is_boosted_mask,
                                            position->forward_adv_fir, position->forward_adv_sec,
                                            position->firewall_fir, position->firewall_sec,
                                            position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                            position->is_swap_available_fir, 0};

                BRANCH_ENTER_MIN("swap");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax_max(depth, beta - 1, beta, &temp_field);
                    if (reschild < beta && alpha < beta - 1)
                        reschild = minimax_max(depth, alpha, beta, &temp_field);
                }
                else
                    reschild = field_evaluate(&temp_field);
                BRANCH_EXIT_MIN();

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

    if (cur_boosted_mask == 0)
    {
        uint64_t temp = sec_link_mask;

        while (temp)
        {
            const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask | pos,
                                        position->forward_adv_fir, position->forward_adv_sec,
                                        position->firewall_fir, position->firewall_sec,
                                        position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                        position->is_swap_available_fir, position->is_swap_available_sec};

            BRANCH_ENTER_MIN("boost link");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax_max(depth, beta - 1, beta, &temp_field);
                if (reschild < beta && alpha < beta - 1)
                    reschild = minimax_max(depth, alpha, beta, &temp_field);
            }
            else
                reschild = field_evaluate(&temp_field);
            BRANCH_EXIT_MIN();

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
        const field_t temp_field = {position->is_fir_mask, position->is_sec_mask, position->is_link_mask, position->is_boosted_mask & temp_field.is_fir_mask,
                                    position->forward_adv_fir, position->forward_adv_sec,
                                    position->firewall_fir, position->firewall_sec,
                                    position->fir_link, position->sec_link, position->fir_virus, position->sec_virus,
                                    position->is_swap_available_fir, position->is_swap_available_sec};

        BRANCH_ENTER_MIN("un-boost");
        int reschild;
        if (depth > 0)
        {
            reschild = minimax_max(depth, beta - 1, beta, &temp_field);
            if (reschild < beta && alpha < beta - 1)
                reschild = minimax_max(depth, alpha, beta, &temp_field);
        }
        else
            reschild = field_evaluate(&temp_field);
        BRANCH_EXIT_MIN();

        score = (reschild < score) ? reschild : score;
        beta = (reschild < beta) ? reschild : beta;

        if (beta <= alpha)
        {
            TRACK_ENTRY_MIN();
        }
    }
    if (depth > MIN_CACHE_DEPTH)
    {
        uint64_t hash = rapidhashNano((const void *__restrict__)position, 0);
        tt_bucket_t *__restrict__ bucket = &tt_sec[hash & (TABLE_SIZE - 1)];
        *(tt_entry_t *__restrict__)((depth >= bucket->depth_preferred.depth) ? &bucket->depth_preferred : &bucket->scratch) = (tt_entry_t){hash, score, (uint8_t)depth, (uint8_t)((score >= betabeg) ? TT_LOWERBOUND : TT_EXACT)};
    }
    return score;

track_min:
    if (depth > MIN_CACHE_DEPTH)
    {
        uint64_t hash = rapidhashNano((const void *__restrict__)position, 0);
        tt_bucket_t *__restrict__ bucket = &tt_sec[hash & (TABLE_SIZE - 1)];
        *(tt_entry_t *__restrict__)((depth >= bucket->depth_preferred.depth) ? &bucket->depth_preferred : &bucket->scratch) = (tt_entry_t){hash, score, (uint8_t)depth, TT_UPPERBOUND};
    }
    return score;
}

END_BRANCH_TRACKING();

#undef PERFORM_ITERATION_FIR
#undef PERFORM_ITERATION_SEC

static minimax_main_result_t minimax_main(const int depth, int alpha, int beta, const bool player, field_t *__restrict__ position)
{
    cur_search_depth = depth;
    struct timespec start, stop;

    assert(depth < MAX_DEPTH);

    if (player)
    {
        possible_moves_t all_moves = possible_moves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (4096 * depth), .has_timed_out = false};

        field_t best_field = all_moves.moves[0];

        CLEAR_TT();

        bool is_first_move = true;

        for (int i = 0; i < all_moves.moves_count; ++i)
        {
            clock_gettime(CLOCK_MONOTONIC, &start);

            field_t pos = all_moves.moves[i];

            int childres;
            if (is_first_move)
            {
                childres = minimax_min(depth - 1, alpha, beta, &pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                debug_printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
            }
            else
            {
                childres = minimax_min(depth - 1, alpha, alpha + 1, &pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                if (childres > alpha)
                {
                    debug_printf("Maximize first minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    childres = minimax_min(depth - 1, alpha, beta, &pos);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    debug_printf("Maximize second minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                }
                else
                {
                    debug_printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                }
            }

            best_field = (childres > alpha) ? pos : best_field;
            alpha = (childres > alpha) ? childres : alpha;
            is_first_move = false;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = alpha, .has_timed_out = false};
    }
    else
    {
        possible_moves_t all_moves = possible_moves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-4096 * depth), .has_timed_out = false};

        field_t best_field = all_moves.moves[0];

        CLEAR_TT();

        bool is_first_move = true;

        for (int i = 0; i < all_moves.moves_count; ++i)
        {
            clock_gettime(CLOCK_MONOTONIC, &start);
            int childres;

            field_t pos = all_moves.moves[i];

            if (is_first_move)
            {
                childres = minimax_max(depth - 1, alpha, beta, &pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                debug_printf("Minimize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
            }
            else
            {
                childres = minimax_max(depth - 1, beta - 1, beta, &pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                if (childres < beta)
                {
                    debug_printf("Minimize first minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    childres = minimax_max(depth - 1, alpha, beta, &pos);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    debug_printf("Minimize second minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                }
                else
                {
                    debug_printf("Minimize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                }
            }

            best_field = (childres < beta) ? pos : best_field;
            beta = (childres < beta) ? childres : beta;
            is_first_move = false;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = beta, .has_timed_out = false};
    }
}

static inline float simple_sqrt(int n)
{
    float x = (float)n;
    float prev;

    do
    {
        prev = x;
        x = (x + n / x) * 0.5f;
    } while (x - prev > 0.0001f || prev - x > 0.0001f);

    return x;
}

static minimax_main_result_t minimax_iteration_main(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *__restrict__ position)
{
    assert(max_depth >= 2 && max_depth % 2 == 0 && "Depth must be at least 2 and even (divisible by 2) for iterative deepening");
    assert(max_search_time >= 100 && "max_search_time must be at least 100 milliseconds");
    assert(max_depth < MAX_DEPTH);

    struct timespec start, start_it, stop, global_start;
    minimax_main_result_t best_result;

    typedef struct
    {
        int move_id;
        int move_eval;
        bool is_exact; // false = upper bound from failed null-window
    } move_scores_wrapper;

    clock_gettime(CLOCK_MONOTONIC, &global_start);

    if (player)
    {
        possible_moves_t all_moves = possible_moves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (4096 * max_depth), .has_timed_out = false};

        float branching_factor_guess = simple_sqrt(all_moves.moves_count);

        field_t best_field = all_moves.moves[0];
        int prev_alpha = alpha;

        move_scores_wrapper move_scores[MAX_MOVES];
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = (move_scores_wrapper){i, MIN, false};

        for (int current_depth = 2; current_depth <= max_depth; current_depth += 2)
        {
            cur_search_depth = current_depth; // dont forget to cur_search_depth locally!

            int best_move_idx = -1;

            int64_t cur_rec_count = rec_counter;
            clock_gettime(CLOCK_MONOTONIC, &start);

            if (current_depth > 2)
            {
                for (int i = 1; i < all_moves.moves_count; ++i)
                {
                    move_scores_wrapper key = move_scores[i];
                    int j = i - 1;
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

            int iteration_alpha = prev_alpha - 56;
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                clock_gettime(CLOCK_MONOTONIC, &start_it);

                int move_idx = move_scores[i].move_id;

                field_t pos = all_moves.moves[move_idx];

                int childres;
                if (i == 0) // very likely for the score to be higher
                {
                    childres = minimax_min(current_depth - 1, iteration_alpha, beta, &pos);
                    clock_gettime(CLOCK_MONOTONIC, &stop);

                    move_scores[i].is_exact = true;

                    // debug_printf(FG_BLUE "[f %d: %d -> %d : %ld] " RESET, move_idx, iteration_alpha, childres, (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start_it.tv_sec * 1000 + start_it.tv_nsec / 1000000));
                    // fflush(stdout);
                }
                else
                {
                    childres = minimax_min(current_depth - 1, iteration_alpha, iteration_alpha + 1, &pos);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    // debug_printf(FG_GREEN "[s %d: %d -> %d : %ld] " RESET, move_idx, iteration_alpha, childres, (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start_it.tv_sec * 1000 + start_it.tv_nsec / 1000000));
                    // fflush(stdout);
                    if (childres > iteration_alpha)
                    {
                        childres = minimax_min(current_depth - 1, iteration_alpha, beta, &pos);
                        clock_gettime(CLOCK_MONOTONIC, &stop);

                        move_scores[i].is_exact = true;

                        // debug_printf(FG_RED "[r %d: %d -> %d : %ld] " RESET, move_idx, iteration_alpha, childres, (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start_it.tv_sec * 1000 + start_it.tv_nsec / 1000000));
                        // fflush(stdout);
                    }
                    else
                    {
                        move_scores[i].is_exact = false;
                    }
                }

                move_scores[i].move_eval = childres;

                if (childres > iteration_alpha)
                {
                    best_field = pos;
                    iteration_alpha = childres;
                    best_move_idx = move_idx;
                }

                int64_t elapsed_time = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (global_start.tv_sec * 1000 + global_start.tv_nsec / 1000000);

                if (elapsed_time > max_search_time)
                {
                    debug_printf("Search order: ");
                    for (int u = 0; u < all_moves.moves_count; ++u)
                        debug_printf("%d, ", move_scores[u].move_id);
                    debug_printf("\n");
                    debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d, off = %ld\n", i, all_moves.moves_count, best_move_idx, iteration_alpha, elapsed_time - max_search_time);

                    best_result.has_timed_out = true;
                    return best_result;
                }
            }

            if (iteration_alpha == prev_alpha - 56)
            {
                debug_printf("Guess failed\n");

                iteration_alpha = MIN;
                for (int i = 0; i < all_moves.moves_count; ++i)
                {
                    clock_gettime(CLOCK_MONOTONIC, &start_it);

                    int move_idx = move_scores[i].move_id;

                    field_t pos = all_moves.moves[move_idx];

                    int childres;
                    if (i == 0)
                    {
                        childres = minimax_min(current_depth - 1, iteration_alpha, beta, &pos);
                        clock_gettime(CLOCK_MONOTONIC, &stop);

                        move_scores[i].is_exact = true;
                        // debug_printf(FG_BLUE "[f %d: %d -> %d : %ld] " RESET, move_idx, iteration_alpha, childres, (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start_it.tv_sec * 1000 + start_it.tv_nsec / 1000000));
                        // fflush(stdout);
                    }
                    else
                    {
                        childres = minimax_min(current_depth - 1, iteration_alpha, iteration_alpha + 1, &pos);
                        clock_gettime(CLOCK_MONOTONIC, &stop);
                        // debug_printf(FG_GREEN "[s %d: %d -> %d : %ld] " RESET, move_idx, iteration_alpha, childres, (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start_it.tv_sec * 1000 + start_it.tv_nsec / 1000000));
                        // fflush(stdout);
                        if (childres > iteration_alpha)
                        {
                            childres = minimax_min(current_depth - 1, iteration_alpha, beta, &pos);
                            clock_gettime(CLOCK_MONOTONIC, &stop);

                            move_scores[i].is_exact = true;
                            // debug_printf(FG_RED "[r %d: %d -> %d : %ld] " RESET, move_idx, iteration_alpha, childres, (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start_it.tv_sec * 1000 + start_it.tv_nsec / 1000000));
                            // fflush(stdout);
                        }
                        else
                        {
                            move_scores[i].is_exact = false;
                        }
                    }

                    move_scores[i].move_eval = childres;

                    if (childres > iteration_alpha)
                    {
                        best_field = pos;
                        iteration_alpha = childres;
                        best_move_idx = move_idx;
                    }

                    clock_gettime(CLOCK_MONOTONIC, &stop);

                    int64_t elapsed_time = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (global_start.tv_sec * 1000 + global_start.tv_nsec / 1000000);

                    if (elapsed_time > max_search_time)
                    {
                        debug_printf("Search order: ");
                        for (int u = 0; u < all_moves.moves_count; ++u)
                            debug_printf("%d, ", move_scores[u].move_id);
                        debug_printf("\n");
                        debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d, off = %ld\n", i, all_moves.moves_count, best_move_idx, iteration_alpha, elapsed_time - max_search_time);
                        best_result.has_timed_out = true;
                        return best_result;
                    }
                }
            }

            debug_printf("Search order: ");
            for (int i = 0; i < all_moves.moves_count; ++i)
                debug_printf("%d, ", move_scores[i].move_id);
            debug_printf("\n");

            clock_gettime(CLOCK_MONOTONIC, &stop);

            int64_t duration = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);

            debug_printf("Depth %d completed in %ld ms (best_move_idx = %d), evaluation: %d, checked_pos: %ld, pos/ms: %f\n",
                         current_depth,
                         duration,
                         best_move_idx,
                         iteration_alpha,
                         rec_counter - cur_rec_count,
                         (double)(rec_counter - cur_rec_count) / ((double)(stop.tv_sec * 1000000000 + stop.tv_nsec - start.tv_sec * 1000000000 - start.tv_nsec) / 1000000.0));

            prev_alpha = iteration_alpha;
            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_alpha, .has_timed_out = false};

            if (iteration_alpha > 4096 || iteration_alpha < -4096)
            {
                debug_printf("end condition detected, exiting\n");
                return best_result;
            }

            debug_printf("cur=%ld, next_guess=%ld\n", duration, (int64_t)((float)duration * branching_factor_guess));

            if ((int64_t)((float)duration * branching_factor_guess) > max_search_time)
            {
                debug_printf("Speculative exit\n");
                return best_result;
            }
        }

        return best_result;
    }
    else
    {
        possible_moves_t all_moves = possible_moves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-4096 * max_depth), .has_timed_out = false};

        float branching_factor_guess = simple_sqrt(all_moves.moves_count);

        field_t best_field = all_moves.moves[0];
        int prev_beta = beta;

        move_scores_wrapper move_scores[MAX_MOVES];
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = (move_scores_wrapper){i, MIN, false};

        for (int current_depth = 2; current_depth <= max_depth; current_depth += 2)
        {
            cur_search_depth = current_depth; // dont forget to cur_search_depth locally!

            int best_move_idx = -1;

            int64_t cur_rec_count = rec_counter;
            clock_gettime(CLOCK_MONOTONIC, &start);

            if (current_depth > 2)
            {
                for (int i = 1; i < all_moves.moves_count; ++i)
                {
                    move_scores_wrapper key = move_scores[i];
                    int j = i - 1;
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

            int iteration_beta = prev_beta + 56;
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                int move_idx = move_scores[i].move_id;
                field_t pos = all_moves.moves[move_idx];

                int childres;

                if (i == 0) // very likely for the score to be higher
                {
                    childres = minimax_max(current_depth - 1, alpha, iteration_beta, &pos);

                    move_scores[i].is_exact = true;
                }
                else
                {
                    childres = minimax_max(current_depth - 1, iteration_beta - 1, iteration_beta, &pos);
                    if (childres < iteration_beta)
                    {
                        childres = minimax_max(current_depth - 1, alpha, iteration_beta, &pos);
                        move_scores[i].is_exact = true;
                    }
                    else
                    {
                        move_scores[i].is_exact = false;
                    }
                }

                move_scores[i].move_eval = childres;

                if (childres < iteration_beta)
                {
                    best_field = all_moves.moves[move_idx];
                    iteration_beta = childres;
                    best_move_idx = move_idx;
                }

                clock_gettime(CLOCK_MONOTONIC, &stop);

                int64_t elapsed_time = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (global_start.tv_sec * 1000 + global_start.tv_nsec / 1000000);

                if (elapsed_time > max_search_time)
                {
                    debug_printf("Search order: ");
                    for (int u = 0; u < all_moves.moves_count; ++u)
                        debug_printf("%d, ", move_scores[u].move_id);
                    debug_printf("\n");
                    debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d, off = %ld\n", i, all_moves.moves_count, best_move_idx, iteration_beta, elapsed_time - max_search_time);

                    best_result.has_timed_out = true;
                    return best_result;
                }
            }

            if (iteration_beta == prev_beta + 56)
            {
                debug_printf("Guess failed\n");

                iteration_beta = MAX;
                for (int i = 0; i < all_moves.moves_count; ++i)
                {
                    int move_idx = move_scores[i].move_id;
                    field_t pos = all_moves.moves[move_idx];
                    int childres;

                    if (i == 0)
                    {
                        childres = minimax_max(current_depth - 1, alpha, iteration_beta, &pos);

                        move_scores[i].is_exact = true;
                    }
                    else
                    {
                        childres = minimax_max(current_depth - 1, iteration_beta - 1, iteration_beta, &pos);
                        if (childres < iteration_beta)
                        {
                            childres = minimax_max(current_depth - 1, alpha, iteration_beta, &pos);
                            move_scores[i].is_exact = true;
                        }
                        else
                        {
                            move_scores[i].is_exact = false;
                        }
                    }

                    move_scores[i].move_eval = childres;

                    if (childres < iteration_beta)
                    {
                        best_field = all_moves.moves[move_idx];
                        iteration_beta = childres;
                        best_move_idx = move_idx;
                    }

                    clock_gettime(CLOCK_MONOTONIC, &stop);

                    int64_t elapsed_time = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (global_start.tv_sec * 1000 + global_start.tv_nsec / 1000000);

                    if (elapsed_time > max_search_time)
                    {
                        debug_printf("Search order: ");
                        for (int u = 0; u < all_moves.moves_count; ++u)
                            debug_printf("%d, ", move_scores[u].move_id);
                        debug_printf("\n");
                        debug_printf("timed out p1 %d/%d, best_move_idx=%d, eval=%d, off = %ld\n", i, all_moves.moves_count, best_move_idx, iteration_beta, elapsed_time - max_search_time);
                        best_result.has_timed_out = true;
                        return best_result;
                    }
                }
            }

            debug_printf("Search order: ");
            for (int i = 0; i < all_moves.moves_count; ++i)
                debug_printf("%d, ", move_scores[i].move_id);
            debug_printf("\n");

            clock_gettime(CLOCK_MONOTONIC, &stop);

            int64_t duration = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);

            debug_printf("Depth %d completed in %ld ms (best_move_idx = %d), evaluation: %d, checked_pos: %ld, pos/ms: %f\n",
                         current_depth,
                         duration,
                         best_move_idx,
                         iteration_beta,
                         rec_counter - cur_rec_count,
                         (double)(rec_counter - cur_rec_count) / ((double)(stop.tv_sec * 1000000000 + stop.tv_nsec - start.tv_sec * 1000000000 - start.tv_nsec) / 1000000.0));

            prev_beta = iteration_beta;
            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_beta, .has_timed_out = false};

            if (iteration_beta > 4096 || iteration_beta < -4096)
            {
                debug_printf("end condition detected, exiting\n");
                return best_result;
            }

            debug_printf("cur=%ld, next_guess=%ld\n", duration, (int64_t)((float)duration * branching_factor_guess));

            if ((int64_t)((float)duration * branching_factor_guess) > max_search_time)
            {
                debug_printf("Speculative exit\n");
                return best_result;
            }
        }

        return best_result;
    }
}
