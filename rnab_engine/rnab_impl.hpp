#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#ifdef BOOST_CACHE_BACKEND
#include <boost/unordered/unordered_flat_map.hpp>
#else
#include <stdatomic.h>
#endif

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

#ifndef GAME_CONSTANTS
#define GAME_CONSTANTS

#define MAX_MOVES 80
#define MAX_DEPTH 40

#define MIN -1000000
#define MAX 1000000

#ifndef MIN_CACHE_DEPTH
#define MIN_CACHE_DEPTH 1
#endif

#define ITERATION_CURRENT_IS_FIRST_UNKNOWN 0
#define ITERATION_CURRENT_IS_FIRST_LINK 1
#define ITERATION_CURRENT_IS_FIRST_VIRUS 2
#define ITERATION_CURRENT_IS_SECOND_UNKNOWN 3
#define ITERATION_CURRENT_IS_SECOND_LINK 4
#define ITERATION_CURRENT_IS_SECOND_VIRUS 5

#ifndef COSTLY_POWERUPS_LOOKAHEAD
#define COSTLY_POWERUPS_LOOKAHEAD 6
#endif

#endif

#ifdef RNAB_DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) ((void)0)
#endif

#define PASTE_(a, b) a##_##b
#define PASTE(a, b) PASTE_(a, b)
#define SIMD_NAME(name) PASTE(name, SIMD_SUFFIX)
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#if defined(THREAD_GUARD) && defined(BOOST_CACHE_BACKEND)
#error "THREAD_GUARD is not supported with BOOST_CACHE_BACKEND"
#endif

typedef union
{
    uint64_t raw;
    struct
    {
        int32_t eval;
        uint16_t depth;
        uint16_t flag;
    } fields;
} tt_payload_t;

typedef struct
{
#ifdef THREAD_GUARD
    _Atomic(uint64_t) hash_entry;
    _Atomic(uint64_t) data;
#else
    uint64_t hash_entry;
    tt_payload_t data;
#endif
} tt_entry_t;

static_assert(MAX_DEPTH < UINT16_MAX); // uint16_t limit

typedef struct
{
    tt_entry_t depth_preferred;
    tt_entry_t scratch;
} tt_bucket_t;

#include "wyhash.h"

struct field_t
{
    using is_avalanching = void;

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

    uint8_t is_checker_available_fir : 1;
    uint8_t is_checker_available_sec : 1;
    uint8_t is_swap_available_fir : 1;
    uint8_t is_swap_available_sec : 1;

    inline __attribute__((always_inline)) bool operator==(const field_t &other) const
    {
        return __builtin_memcmp(this, &other, sizeof(field_t)) == 0;
    }

    inline __attribute__((always_inline)) uint64_t hash() const
    {
        return wyhash(this, sizeof(field_t), 0, _wyp);
    }

    inline __attribute__((always_inline)) size_t operator()(const field_t &s) const
    {
        return wyhash(&s, sizeof(field_t), 0, _wyp);
    }

    bool operator!=(const field_t &other) const
    {
        if (is_sec_mask != other.is_sec_mask)
        {
            printf("Mismatch in is_sec_mask\n");
            return true;
        }
        if (is_link_mask != other.is_link_mask)
        {
            printf("Mismatch in is_link_mask\n");
            return true;
        }
        if (is_fir_mask != other.is_fir_mask)
        {
            printf("Mismatch in is_fir_mask\n");
            return true;
        }
        if (is_boosted_mask != other.is_boosted_mask)
        {
            printf("Mismatch in is_boosted_mask\n");
            return true;
        }
        if (is_swap_available_fir != other.is_swap_available_fir)
        {
            printf("Mismatch in is_swap_available_fir\n");
            return true;
        }
        if (is_swap_available_sec != other.is_swap_available_sec)
        {
            printf("Mismatch in is_swap_available_sec\n");
            return true;
        }
        if (is_checker_available_fir != other.is_checker_available_fir)
        {
            printf("Mismatch in is_checker_available_fir\n");
            return true;
        }
        if (is_checker_available_sec != other.is_checker_available_sec)
        {
            printf("Mismatch in is_checker_available_sec\n");
            return true;
        }
        if (firewall_fir != other.firewall_fir)
        {
            printf("Mismatch in firewall_fir\n");
            return true;
        }
        if (firewall_sec != other.firewall_sec)
        {
            printf("Mismatch in firewall_sec\n");
            return true;
        }
        if (fir_virus != other.fir_virus)
        {
            printf("Mismatch in fir_virus\n");
            return true;
        }
        if (fir_link != other.fir_link)
        {
            printf("Mismatch in fir_link\n");
            return true;
        }
        if (forward_adv_fir != other.forward_adv_fir)
        {
            printf("Mismatch in forward_adv_fir %d != %d\n", forward_adv_fir, other.forward_adv_fir);
            return true;
        }
        if (forward_adv_sec != other.forward_adv_sec)
        {
            printf("Mismatch in forward_adv_sec\n");
            return true;
        }
        if (sec_virus != other.sec_virus)
        {
            printf("Mismatch in sec_virus\n");
            return true;
        }
        if (sec_link != other.sec_link)
        {
            printf("Mismatch in sec_link\n");
            return true;
        }
        return false;
    }

    bool check_integrity()
    {
        if (is_fir_mask & is_sec_mask)
        {
            printf("failed 7\n");
            return false;
        }

        if (__builtin_popcountll(is_fir_mask & is_boosted_mask) > 1)
        {
            printf("failed 1\n");
            return false;
        }

        if (__builtin_popcountll(is_sec_mask & is_boosted_mask) > 1)
        {
            printf("failed 2\n");
            return false;
        }

        int sum = 0;

        uint64_t mask = is_fir_mask;

        while (mask)
        {
            int pos = __builtin_ctzll(mask);

            sum += 7 - (pos >> 3);

            mask ^= (1ULL << pos);
        }

        if (sum != forward_adv_fir)
        {
            printf("failed 3, %d != %d\n", sum, forward_adv_fir);
            return false;
        }

        sum = 0;
        mask = is_sec_mask;

        while (mask)
        {
            int pos = __builtin_ctzll(mask);

            sum += (pos >> 3);

            mask ^= (1ULL << pos);
        }

        if (sum != forward_adv_sec)
        {
            printf("failed 4\n");
            return false;
        }

        return true;
    }

    uint64_t reverse_mask(uint64_t mask)
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

    field_t reverse_field() // should only be used for debugging
    {
        field_t new_field;

        new_field.is_fir_mask = reverse_mask(is_sec_mask);
        new_field.is_sec_mask = reverse_mask(is_fir_mask);
        new_field.is_link_mask = reverse_mask(is_link_mask);
        new_field.is_boosted_mask = reverse_mask(is_boosted_mask);

        new_field.forward_adv_fir = forward_adv_sec;
        new_field.forward_adv_sec = forward_adv_fir;

        new_field.firewall_fir = (firewall_sec) ? ((126 - (firewall_sec & 126)) | (firewall_sec & 1)) : 0;
        new_field.firewall_sec = (firewall_fir) ? ((126 - (firewall_fir & 126)) | (firewall_fir & 1)) : 0;

        new_field.fir_link = sec_link;
        new_field.fir_virus = sec_virus;

        new_field.sec_link = fir_link;
        new_field.sec_virus = fir_virus;

        new_field.is_checker_available_fir = is_checker_available_sec;
        new_field.is_checker_available_sec = is_checker_available_fir;

        new_field.is_swap_available_fir = is_swap_available_sec;
        new_field.is_swap_available_sec = is_swap_available_fir;

        return new_field;
    }

    field_t() {}; // no standart constructor

    field_t(uint8_t pos_fir, uint8_t pos_sec)
    {
        is_sec_mask = 0;
        is_link_mask = 0;
        is_fir_mask = 0;
        is_boosted_mask = 0;

        fir_link = 0;
        fir_virus = 0;
        sec_link = 0;
        sec_virus = 0;

        forward_adv_fir = 2;
        forward_adv_sec = 2;

        firewall_fir = 0;
        firewall_sec = 0;

        is_checker_available_fir = 1;
        is_checker_available_sec = 1;
        is_swap_available_fir = 1;
        is_swap_available_sec = 1;

        static const int init_pos_fir[8] = {63, 62, 61, 52, 51, 58, 57, 56};
        static const int init_pos_sec[8] = {0, 1, 2, 11, 12, 5, 6, 7};

        for (int i = 0; i < 8; ++i)
        {
            is_sec_mask |= (1ULL << init_pos_sec[i]);
            is_fir_mask |= (1ULL << init_pos_fir[i]);
            is_link_mask |= ((uint64_t)(((~pos_fir) >> i) & 1) << init_pos_fir[i]) | ((uint64_t)(((~pos_sec) >> i) & 1) << init_pos_sec[i]);
        }
    }

    inline __attribute__((always_inline)) int evaluate() const
    {
        return ((1024 << fir_link) - (512 << fir_virus) - (1024 << sec_link) + (512 << sec_virus)) + (int)forward_adv_fir - (int)forward_adv_sec + 2048 * (int)is_swap_available_fir - 2048 * (int)is_swap_available_sec;
    }

    void print_field()
    {
        printf("Virus: %d         Link: %d\n", fir_virus, fir_link);
        for (int i = 63; i >= 0; --i)
        {
            if (((is_fir_mask & is_link_mask) >> i) & 1)
            {
                if ((is_boosted_mask >> i) & 1)
                {
                    printf("\033[32m[\033[0m\033[34mL\033[0m\033[32m]\033[0m");
                    goto end;
                }
                printf("[\033[32mL\033[0m]");
            }
            else if ((is_fir_mask >> i) & 1)
            {
                if ((is_boosted_mask >> i) & 1)
                {
                    printf("\033[32m[\033[0m\033[34mV\033[0m\033[32m]\033[0m");
                    goto end;
                }
                printf("[\033[32mV\033[0m]");
            }
            else if (((is_sec_mask & is_link_mask) >> i) & 1)
            {
                if ((is_boosted_mask >> i) & 1)
                {
                    printf("\033[31m[\033[0m\033[34mL\033[0m\033[31m]\033[0m");
                    goto end;
                }
                printf("[\033[31mL\033[0m]");
            }
            else if ((is_sec_mask >> i) & 1)
            {
                if ((is_boosted_mask >> i) & 1)
                {
                    printf("\033[31m[\033[0m\033[34mV\033[0m\033[31m]\033[0m");
                    goto end;
                }
                printf("[\033[31mV\033[0m]");
            }
            else
                printf("[ ]");
        end:
            if (i % 8 == 0)
                printf("\n");
        }
        printf("Virus: %d         Link: %d\n", sec_virus, sec_link);
    }
};

static_assert(sizeof(field_t) == 40);

struct minimax_main_result_t
{
    field_t best_field;
    int evaluation;
    bool has_timed_out;
};

struct possible_moves_t
{
    field_t moves[MAX_MOVES];
    int moves_count;
};

//#define TU_COMPILE

#ifdef TU_COMPILE

#ifndef BOOST_CACHE_BACKEND

#define CLEAR_TT() memset(transposition_table, 0, sizeof(transposition_table))

extern tt_bucket_t transposition_table[TABLE_SIZE];

inline __attribute__((always_inline)) void tt_store(tt_bucket_t *bucket, uint64_t hash, int32_t eval, uint16_t depth, uint16_t flag)
{
    tt_payload_t packed{.fields{eval, depth, flag}};

#ifdef THREAD_GUARD
    tt_entry_t *target = (depth >= atomic_load_explicit((_Atomic tt_payload_t *)&(bucket->depth_preferred.data), memory_order_relaxed).fields.depth) ? &bucket->depth_preferred : &bucket->scratch;
    atomic_store_explicit(&target->data, packed.raw, memory_order_relaxed);
    atomic_store_explicit(&target->hash_entry, hash ^ packed.raw, memory_order_release);
#else
    tt_entry_t *target = (depth >= bucket->depth_preferred.data.fields.depth) ? &bucket->depth_preferred : &bucket->scratch;
    target->data = packed;
    target->hash_entry = hash ^ packed.raw;
#endif
}

inline __attribute__((always_inline)) bool tt_probe(tt_bucket_t *bucket, uint64_t hash, tt_payload_t *out)
{
    {
#ifdef THREAD_GUARD
        uint64_t key = atomic_load_explicit(&bucket->depth_preferred.hash_entry, memory_order_acquire);
        tt_payload_t data = atomic_load_explicit((_Atomic tt_payload_t *)&(bucket->depth_preferred.data), memory_order_relaxed);
#else
        uint64_t key = bucket->depth_preferred.hash_entry;
        tt_payload_t data = bucket->depth_preferred.data;
#endif

        if ((key ^ data.raw) == hash)
        {
            *out = data;
            return true;
        }
    }

    {
#ifdef THREAD_GUARD
        uint64_t key = atomic_load_explicit(&bucket->scratch.hash_entry, memory_order_acquire);
        tt_payload_t data = atomic_load_explicit((_Atomic tt_payload_t *)&(bucket->scratch.data), memory_order_relaxed);
#else
        uint64_t key = bucket->scratch.hash_entry;
        tt_payload_t data = bucket->scratch.data;
#endif
        if ((key ^ data.raw) == hash)
        {
            *out = data;
            return true;
        }
    }

    return false;
}

#else

#define CLEAR_TT()                            \
    for (int t_t = 0; t_t < MAX_DEPTH; ++t_t) \
    SIMD_NAME(cache)                          \
    [t_t].clear()

static boost::unordered_flat_map<field_t, tt_payload_t, field_t> SIMD_NAME(cache)[MAX_DEPTH];

#endif

int SIMD_NAME(cur_search_depth) = 0;

#define PERFORM_ITERATION(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                              \
    if ((new_pos_bitboard & enemy_firewall_mask) == 0)                                                                                                                 \
    {                                                                                                                                                                  \
        if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN || current_card == ITERATION_CURRENT_IS_FIRST_LINK || current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) \
        {                                                                                                                                                              \
            if (secmask & new_pos_bitboard)                                                                                                                            \
            {                                                                                                                                                          \
                if (sec_link_mask & new_pos_bitboard)                                                                                                                  \
                {                                                                                                                                                      \
                    field_t temp_field = *position;                                                                                                                    \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                \
                    temp_field.forward_adv_fir += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    temp_field.is_link_mask &= ~new_pos_bitboard;                                                                                                      \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.fir_link;                                                                                                                             \
                                                                                                                                                                       \
                    WRITE_MOVE();                                                                                                                                      \
                }                                                                                                                                                      \
                else if (position->fir_virus < 3)                                                                                                                      \
                {                                                                                                                                                      \
                    field_t temp_field = *position;                                                                                                                    \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                \
                    temp_field.forward_adv_fir += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.fir_virus;                                                                                                                            \
                                                                                                                                                                       \
                    WRITE_MOVE();                                                                                                                                      \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
            else if ((firmask & new_pos_bitboard) == 0)                                                                                                                \
            {                                                                                                                                                          \
                field_t temp_field = *position;                                                                                                                        \
                                                                                                                                                                       \
                temp_field.forward_adv_fir += forward_adv;                                                                                                             \
                if (is_boosted)                                                                                                                                        \
                    temp_field.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                               \
                temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                       \
                if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                                \
                {                                                                                                                                                      \
                    uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                        \
                    temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                 \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                              \
                {                                                                                                                                                      \
                    temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                  \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                WRITE_MOVE();                                                                                                                                          \
            }                                                                                                                                                          \
        }                                                                                                                                                              \
        else                                                                                                                                                           \
        {                                                                                                                                                              \
            if (firmask & new_pos_bitboard)                                                                                                                            \
            {                                                                                                                                                          \
                if (fir_link_mask & new_pos_bitboard)                                                                                                                  \
                {                                                                                                                                                      \
                    field_t temp_field = *position;                                                                                                                    \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                            \
                    temp_field.forward_adv_sec += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    temp_field.is_link_mask &= ~new_pos_bitboard;                                                                                                      \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.sec_link;                                                                                                                             \
                                                                                                                                                                       \
                    WRITE_MOVE();                                                                                                                                      \
                }                                                                                                                                                      \
                else if (position->sec_virus < 3)                                                                                                                      \
                {                                                                                                                                                      \
                    field_t temp_field = *position;                                                                                                                    \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                            \
                    temp_field.forward_adv_sec += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.sec_virus;                                                                                                                            \
                                                                                                                                                                       \
                    WRITE_MOVE();                                                                                                                                      \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
            else if ((secmask & new_pos_bitboard) == 0)                                                                                                                \
            {                                                                                                                                                          \
                field_t temp_field = *position;                                                                                                                        \
                                                                                                                                                                       \
                temp_field.forward_adv_sec += forward_adv;                                                                                                             \
                if (is_boosted)                                                                                                                                        \
                    temp_field.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                               \
                temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                       \
                if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                               \
                {                                                                                                                                                      \
                    uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                        \
                    temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                 \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                             \
                {                                                                                                                                                      \
                    temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                  \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                WRITE_MOVE();                                                                                                                                          \
            }                                                                                                                                                          \
        }                                                                                                                                                              \
    }

possible_moves_t SIMD_NAME(possible_moves)(const field_t *position, const bool player)
{
    possible_moves_t
        res{.moves_count = 0};

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
        const uint64_t unmoveable_mask = firmask | secmask | enemy_firewall_mask;

        if (__builtin_expect(fir_link_mask & 24ULL, 0))
        {
            if (fir_link_mask & 8ULL)
            {
                field_t temp_field = *position;

                temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp_field.is_boosted_mask &= ~8ULL;
                temp_field.is_fir_mask &= ~8ULL;
                temp_field.is_link_mask &= ~8ULL;
                ++temp_field.fir_link;

                WRITE_MOVE();
            }
            if (fir_link_mask & 16ULL)
            {
                field_t temp_field = *position;

                temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(16ULL) >> 3);
                temp_field.is_boosted_mask &= ~16ULL;
                temp_field.is_fir_mask &= ~16ULL;
                temp_field.is_link_mask &= ~16ULL;
                ++temp_field.fir_link;

                WRITE_MOVE();
            }
        }
        else if (__builtin_expect((((fir_link_mask & 2052ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 8ULL) == 0) || (((fir_link_mask & 4128ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 16ULL) == 0), 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(cur_boosted_mask) >> 3);
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

                temp ^= pos;
            }
        }
        else
        {
            const uint64_t cur_pos_bitboard = cur_boosted_mask;

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8); // double forward
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // forward right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // forward left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // double right
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // double left
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // backwards right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // backwards left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard << 8); // double backwards
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }
        }

        uint64_t temp = fir_virus_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = fir_virus_mask & (~cur_boosted_mask);

        const uint64_t secmask_inv = ~secmask;

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = fir_link_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            temp ^= cur_pos_bitboard;
        }

        if (position->firewall_fir == 0)
        {
            uint64_t temp = fir_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = __builtin_ctzll(temp);
                const uint64_t pos = (1ULL << bit_pos); // front -> back

                field_t temp_field = *position;

                temp_field.firewall_fir = (bit_pos << 1) | 1;

                WRITE_MOVE();

                temp ^= pos;
            }

            temp = (fir_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

            if (temp)
            {
                const int bit_pos = __builtin_ctzll(temp);

                field_t temp_field = *position;

                temp_field.firewall_fir = (bit_pos << 1) | 1;

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

                    virus_mask ^= virus_pos;
                }

                link_mask ^= link_pos;
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

                temp ^= pos;
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
        const uint64_t unmoveable_mask = firmask | secmask | enemy_firewall_mask;

        if (__builtin_expect(sec_link_mask & 1729382256910270464ULL, 0))
        {
            if (sec_link_mask & 576460752303423488ULL)
            {
                field_t temp_field = *position;

                temp_field.forward_adv_sec -= (__builtin_ctzll(576460752303423488ULL) >> 3);
                temp_field.is_boosted_mask &= ~576460752303423488ULL;
                temp_field.is_sec_mask &= ~576460752303423488ULL;
                temp_field.is_link_mask &= ~576460752303423488ULL;
                ++temp_field.sec_link;

                WRITE_MOVE();
            }
            if (sec_link_mask & 1152921504606846976ULL)
            {
                field_t temp_field = *position;

                temp_field.forward_adv_sec -= (__builtin_ctzll(1152921504606846976ULL) >> 3);
                temp_field.is_boosted_mask &= ~1152921504606846976ULL;
                temp_field.is_sec_mask &= ~1152921504606846976ULL;
                temp_field.is_link_mask &= ~1152921504606846976ULL;
                ++temp_field.sec_link;

                WRITE_MOVE();
            }
        }
        else if (__builtin_expect((((sec_link_mask & 2310346608841064448ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 1152921504606846976ULL) == 0) || (((sec_link_mask & 290482175965396992ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 576460752303423488ULL) == 0), 0))
        {
            field_t temp_field = *position;

            temp_field.forward_adv_sec -= (__builtin_ctzll(cur_boosted_mask) >> 3);
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

                temp ^= pos;
            }
        }
        else
        {
            const uint64_t cur_pos_bitboard = cur_boosted_mask;

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8); // double forward
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // forward right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // forward left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // double right
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // double left
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // backwards right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // backwards left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8); // double backwards
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }
        }

        uint64_t temp = sec_virus_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = sec_virus_mask & (~cur_boosted_mask);

        const uint64_t firmask_inv = ~firmask;

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = sec_link_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            temp ^= cur_pos_bitboard;
        }

        if (position->firewall_sec == 0)
        {
            uint64_t temp = sec_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = 63 - __builtin_clzll(temp);
                const uint64_t pos = (1ULL << bit_pos); // front -> back

                field_t temp_field = *position;

                temp_field.firewall_sec = (bit_pos << 1) | 1;

                WRITE_MOVE();

                temp ^= pos;
            }

            temp = (sec_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

            if (temp)
            {
                const int bit_pos = 63 - __builtin_clzll(temp);

                field_t temp_field = *position;

                temp_field.firewall_sec = (bit_pos << 1) | 1;

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

                    virus_mask ^= virus_pos;
                }

                link_mask ^= link_pos;
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

                temp ^= pos;
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

#undef PERFORM_ITERATION

#define PERFORM_ITERATION(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                              \
    if ((new_pos_bitboard & enemy_firewall_mask) == 0)                                                                                                                 \
    {                                                                                                                                                                  \
        if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN || current_card == ITERATION_CURRENT_IS_FIRST_LINK || current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) \
        {                                                                                                                                                              \
            if (secmask & new_pos_bitboard)                                                                                                                            \
            {                                                                                                                                                          \
                if (sec_link_mask & new_pos_bitboard)                                                                                                                  \
                {                                                                                                                                                      \
                    field_t temp_field = position;                                                                                                                     \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                \
                    temp_field.forward_adv_fir += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    temp_field.is_link_mask &= ~new_pos_bitboard;                                                                                                      \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.fir_link;                                                                                                                             \
                                                                                                                                                                       \
                    BRANCH_ENTER_MAX("capture link");                                                                                                                  \
                    int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field) : temp_field.evaluate();                                    \
                    BRANCH_EXIT_MAX();                                                                                                                                 \
                    alpha = (reschild > alpha) ? reschild : alpha;                                                                                                     \
                    if (beta <= alpha)                                                                                                                                 \
                    {                                                                                                                                                  \
                        if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                            TRACK_ENTRY_MAX();                                                                                                                         \
                        return alpha;                                                                                                                                  \
                    }                                                                                                                                                  \
                }                                                                                                                                                      \
                else if (position.fir_virus < 3)                                                                                                                       \
                {                                                                                                                                                      \
                    field_t temp_field = position;                                                                                                                     \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                \
                    temp_field.forward_adv_fir += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.fir_virus;                                                                                                                            \
                                                                                                                                                                       \
                    int reschild;                                                                                                                                      \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        BRANCH_ENTER_MAX("capture virus boosted");                                                                                                     \
                        reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field) : temp_field.evaluate();                                    \
                        BRANCH_EXIT_MAX();                                                                                                                             \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        BRANCH_ENTER_MAX("capture virus not boosted");                                                                                                 \
                        if (depth > 0)                                                                                                                                 \
                        {                                                                                                                                              \
                            reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);                                                                 \
                            if (reschild > alpha && beta > alpha + 1)                                                                                                  \
                                reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);                                                                  \
                        }                                                                                                                                              \
                        else                                                                                                                                           \
                            reschild = temp_field.evaluate();                                                                                                          \
                                                                                                                                                                       \
                        BRANCH_EXIT_MAX();                                                                                                                             \
                    }                                                                                                                                                  \
                    alpha = (reschild > alpha) ? reschild : alpha;                                                                                                     \
                    if (beta <= alpha)                                                                                                                                 \
                    {                                                                                                                                                  \
                        if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                            TRACK_ENTRY_MAX();                                                                                                                         \
                        return alpha;                                                                                                                                  \
                    }                                                                                                                                                  \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
            else if ((firmask & new_pos_bitboard) == 0)                                                                                                                \
            {                                                                                                                                                          \
                field_t temp_field = position;                                                                                                                         \
                                                                                                                                                                       \
                temp_field.forward_adv_fir += forward_adv;                                                                                                             \
                if (is_boosted)                                                                                                                                        \
                    temp_field.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                               \
                temp_field.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                       \
                if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                                \
                {                                                                                                                                                      \
                    uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                        \
                    temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                 \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                              \
                {                                                                                                                                                      \
                    temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                  \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                int reschild;                                                                                                                                          \
                if (is_boosted)                                                                                                                                        \
                {                                                                                                                                                      \
                    BRANCH_ENTER_MAX("move boosted");                                                                                                                  \
                    reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field) : temp_field.evaluate();                                        \
                    BRANCH_EXIT_MAX();                                                                                                                                 \
                }                                                                                                                                                      \
                else                                                                                                                                                   \
                {                                                                                                                                                      \
                    BRANCH_ENTER_MAX("move not boosted");                                                                                                              \
                    if (depth > 0)                                                                                                                                     \
                    {                                                                                                                                                  \
                        reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);                                                                     \
                        if (reschild > alpha && beta > alpha + 1)                                                                                                      \
                            reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);                                                                      \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                        reschild = temp_field.evaluate();                                                                                                              \
                                                                                                                                                                       \
                    BRANCH_EXIT_MAX();                                                                                                                                 \
                }                                                                                                                                                      \
                alpha = (reschild > alpha) ? reschild : alpha;                                                                                                         \
                if (beta <= alpha)                                                                                                                                     \
                {                                                                                                                                                      \
                    if (depth > MIN_CACHE_DEPTH)                                                                                                                       \
                        TRACK_ENTRY_MAX();                                                                                                                             \
                    return alpha;                                                                                                                                      \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
        }                                                                                                                                                              \
        else                                                                                                                                                           \
        {                                                                                                                                                              \
            if (firmask & new_pos_bitboard)                                                                                                                            \
            {                                                                                                                                                          \
                if (fir_link_mask & new_pos_bitboard)                                                                                                                  \
                {                                                                                                                                                      \
                    field_t temp_field = position;                                                                                                                     \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                            \
                    temp_field.forward_adv_sec += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    temp_field.is_link_mask &= ~new_pos_bitboard;                                                                                                      \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.sec_link;                                                                                                                             \
                                                                                                                                                                       \
                    BRANCH_ENTER_MIN("capture link");                                                                                                                  \
                    int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field) : temp_field.evaluate();                                     \
                    BRANCH_EXIT_MIN();                                                                                                                                 \
                    beta = (reschild < beta) ? reschild : beta;                                                                                                        \
                    if (beta <= alpha)                                                                                                                                 \
                    {                                                                                                                                                  \
                        if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                            TRACK_ENTRY_MIN();                                                                                                                         \
                        return beta;                                                                                                                                   \
                    }                                                                                                                                                  \
                }                                                                                                                                                      \
                else if (position.sec_virus < 3)                                                                                                                       \
                {                                                                                                                                                      \
                    field_t temp_field = position;                                                                                                                     \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp_field.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                            \
                    temp_field.forward_adv_sec += forward_adv;                                                                                                         \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask ^= (cur_pos_bitboard);                                                                                              \
                        temp_field.is_boosted_mask |= (new_pos_bitboard);                                                                                              \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp_field.is_boosted_mask &= ~(new_pos_bitboard);                                                                                             \
                    }                                                                                                                                                  \
                    temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                   \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                    \
                        temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                             \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                              \
                    }                                                                                                                                                  \
                    temp_field.is_fir_mask ^= new_pos_bitboard;                                                                                                        \
                    ++temp_field.sec_virus;                                                                                                                            \
                                                                                                                                                                       \
                    int reschild;                                                                                                                                      \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        BRANCH_ENTER_MIN("capture virus boosted");                                                                                                     \
                        reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field) : temp_field.evaluate();                                     \
                        BRANCH_EXIT_MIN();                                                                                                                             \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        BRANCH_ENTER_MIN("capture virus not boosted");                                                                                                 \
                        if (depth > 0)                                                                                                                                 \
                        {                                                                                                                                              \
                            reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);                                                                    \
                            if (reschild < beta && alpha < beta - 1)                                                                                                   \
                                reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);                                                                   \
                        }                                                                                                                                              \
                        else                                                                                                                                           \
                            reschild = temp_field.evaluate();                                                                                                          \
                                                                                                                                                                       \
                        BRANCH_EXIT_MIN();                                                                                                                             \
                    }                                                                                                                                                  \
                    beta = (reschild < beta) ? reschild : beta;                                                                                                        \
                    if (beta <= alpha)                                                                                                                                 \
                    {                                                                                                                                                  \
                        if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                            TRACK_ENTRY_MIN();                                                                                                                         \
                        return beta;                                                                                                                                   \
                    }                                                                                                                                                  \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
            else if ((secmask & new_pos_bitboard) == 0)                                                                                                                \
            {                                                                                                                                                          \
                field_t temp_field = position;                                                                                                                         \
                                                                                                                                                                       \
                temp_field.forward_adv_sec += forward_adv;                                                                                                             \
                if (is_boosted)                                                                                                                                        \
                    temp_field.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                               \
                temp_field.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                       \
                if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                               \
                {                                                                                                                                                      \
                    uint64_t mask = temp_field.is_link_mask & cur_pos_bitboard;                                                                                        \
                    temp_field.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                 \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                             \
                {                                                                                                                                                      \
                    temp_field.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                  \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                int reschild;                                                                                                                                          \
                if (is_boosted)                                                                                                                                        \
                {                                                                                                                                                      \
                    BRANCH_ENTER_MIN("move boosted");                                                                                                                  \
                    reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field) : temp_field.evaluate();                                         \
                    BRANCH_EXIT_MIN();                                                                                                                                 \
                }                                                                                                                                                      \
                else                                                                                                                                                   \
                {                                                                                                                                                      \
                    BRANCH_ENTER_MIN("move not boosted");                                                                                                              \
                    if (depth > 0)                                                                                                                                     \
                    {                                                                                                                                                  \
                        reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);                                                                        \
                        if (reschild < beta && alpha < beta - 1)                                                                                                       \
                            reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);                                                                       \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                        reschild = temp_field.evaluate();                                                                                                              \
                                                                                                                                                                       \
                    BRANCH_EXIT_MIN();                                                                                                                                 \
                }                                                                                                                                                      \
                beta = (reschild < beta) ? reschild : beta;                                                                                                            \
                if (beta <= alpha)                                                                                                                                     \
                {                                                                                                                                                      \
                    if (depth > MIN_CACHE_DEPTH)                                                                                                                       \
                        TRACK_ENTRY_MIN();                                                                                                                             \
                    return beta;                                                                                                                                       \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
        }                                                                                                                                                              \
    }

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

static cutoff_tracker_t SIMD_NAME(cutoff_tracker)[1000] = {0};

#define BEGIN_BRANCH_TRACKING() \
    static constexpr int _branch_counter_base = __COUNTER__

#define BRANCH_ENTER_MAX(MSG)                                                             \
    static constexpr int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    const int64_t cur_rec_count_##__LINE__ = SIMD_NAME(rec_counter);                      \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .total_entries++;                                                                 \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .msg = MSG;                                                                       \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .cutoff_entries++;                                                                \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .temp_score = alpha

#define BRANCH_ENTER_MIN(MSG)                                                             \
    static constexpr int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    const int64_t cur_rec_count_##__LINE__ = SIMD_NAME(rec_counter);                      \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .total_entries++;                                                                 \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .msg = MSG;                                                                       \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .cutoff_entries++;                                                                \
    SIMD_NAME(cutoff_tracker)                                                             \
    [_branch_idx_##__LINE__]                                                              \
        .temp_score = beta

#define BRANCH_EXIT_MAX()                                                        \
    SIMD_NAME(cutoff_tracker)                                                    \
    [_branch_idx_##__LINE__]                                                     \
        .recursion_cost += SIMD_NAME(rec_counter) - cur_rec_count_##__LINE__;    \
    if (beta > reschild)                                                         \
        SIMD_NAME(cutoff_tracker)                                                \
    [_branch_idx_##__LINE__]                                                     \
        .cutoff_entries--;                                                       \
    if (reschild > SIMD_NAME(cutoff_tracker)[_branch_idx_##__LINE__].temp_score) \
        SIMD_NAME(cutoff_tracker)                                                \
    [_branch_idx_##__LINE__]                                                     \
        .improved_score++

#define BRANCH_EXIT_MIN()                                                        \
    SIMD_NAME(cutoff_tracker)                                                    \
    [_branch_idx_##__LINE__]                                                     \
        .recursion_cost += SIMD_NAME(rec_counter) - cur_rec_count_##__LINE__;    \
    if (reschild > alpha)                                                        \
        SIMD_NAME(cutoff_tracker)                                                \
    [_branch_idx_##__LINE__]                                                     \
        .cutoff_entries--;                                                       \
    if (reschild < SIMD_NAME(cutoff_tracker)[_branch_idx_##__LINE__].temp_score) \
        SIMD_NAME(cutoff_tracker)                                                \
    [_branch_idx_##__LINE__]                                                     \
        .improved_score++

#define GET_BRANCH_COUNT() \
    (__COUNTER__ - _branch_counter_base - 1)

#else

#define BEGIN_BRANCH_TRACKING()
#define BRANCH_ENTER_MAX(MSG)
#define BRANCH_ENTER_MIN(MSG)
#define BRANCH_EXIT_MAX()
#define BRANCH_EXIT_MIN()
#define GET_BRANCH_COUNT() 0

#endif

#define TRACK_ENTRY_MAX()   \
    {                       \
        goto __cache_alpha; \
    }
#define TRACK_ENTRY_MIN()  \
    {                      \
        goto __cache_beta; \
    }

#define GET_CACHE_COUNT() 0

BEGIN_BRANCH_TRACKING();

static int64_t SIMD_NAME(rec_counter) = 0;

int SIMD_NAME(minimax)(int depth, int alpha, int beta, const bool player, const field_t &__restrict__ position)
{
#ifdef BRANCH_DEBUG
    ++SIMD_NAME(rec_counter);
#endif

    const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;

    if (player)
    {
        const uint64_t fir_link_mask = position.is_link_mask & firmask;
        const uint64_t fir_virus_mask = firmask ^ fir_link_mask;
        const uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;

        const uint64_t cur_boosted_mask = position.is_boosted_mask & firmask;
        const uint64_t enemy_firewall_mask = (uint64_t)(position.firewall_sec & 1) << (position.firewall_sec >> 1);
        const uint64_t unmoveable_mask = firmask | secmask | enemy_firewall_mask;

        if (position.fir_link == 3) // fast path if we are about to win
        {
            if ((fir_link_mask & 24) || (((fir_link_mask & 2052ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 8ULL) == 0) ||
                (((fir_link_mask & 4128ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 16ULL) == 0)) // bosted link can be deposited and nothing is blocking it
                return (32768 * depth);
            if (sec_link_mask)
            {
                const uint64_t links_masked_out = sec_link_mask & ~enemy_firewall_mask;
                if (((links_masked_out >> 8) & firmask) ||                             // up
                    ((links_masked_out << 8) & firmask) ||                             // down
                    (((links_masked_out & 18374403900871474942ULL) >> 1) & firmask) || // left
                    (((links_masked_out & 9187201950435737471ULL) << 1) & firmask))    // right
                    return (32768 * depth);

                if (cur_boosted_mask && links_masked_out)
                {
                    if ((((links_masked_out >> 16) & cur_boosted_mask) && ((unmoveable_mask >> 8) & cur_boosted_mask) == 0) ||                                                                                                              // up and not blocked
                        ((links_masked_out << 16) & cur_boosted_mask && ((unmoveable_mask << 8) & cur_boosted_mask) == 0) ||                                                                                                                // down and not blocked
                        ((((links_masked_out & 18229723555195321596ULL) >> 2) & cur_boosted_mask) && (((unmoveable_mask & 18374403900871474942ULL) >> 1) & cur_boosted_mask) == 0) ||                                                       // left and not blocked
                        ((((links_masked_out & 4557430888798830399ULL) << 2) & cur_boosted_mask) && (((unmoveable_mask & 9187201950435737471ULL) << 1) & cur_boosted_mask) == 0) ||                                                         // right and not blocked
                        ((((links_masked_out & 18374403900871474942ULL) >> 9) & cur_boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & cur_boosted_mask) == 0 || ((unmoveable_mask >> 8) & cur_boosted_mask) == 0)) || // up left
                        ((((links_masked_out & 18374403900871474942ULL) << 7) & cur_boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & cur_boosted_mask) == 0 || ((unmoveable_mask << 8) & cur_boosted_mask) == 0)) || // down left
                        ((((links_masked_out & 9187201950435737471ULL) << 9) & cur_boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & cur_boosted_mask) == 0 || ((unmoveable_mask << 8) & cur_boosted_mask) == 0)) ||   // down right
                        ((((links_masked_out & 9187201950435737471ULL) >> 7) & cur_boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & cur_boosted_mask) == 0 || ((unmoveable_mask >> 8) & cur_boosted_mask) == 0)))     // up right
                        return (32768 * depth);
                }
            }
        }

        --depth;

        int alphabeg;
        if (depth > MIN_CACHE_DEPTH)
        {
#ifdef BOOST_CACHE_BACKEND
            auto it = SIMD_NAME(cache)[depth].find(position);

            if (it != SIMD_NAME(cache)[depth].end() && (int)it->second.fields.depth >= depth)
            {
                tt_payload_t entry = it->second;
#else
            uint64_t this_hash = position.hash();
            tt_bucket_t *bucket = &transposition_table[this_hash & (TABLE_SIZE - 1)];
            tt_payload_t entry;

            if (tt_probe(bucket, this_hash, &entry) && entry.fields.depth >= depth)
            {

#endif
                if (__builtin_expect(entry.fields.flag & 1, 0))
                {
                    if (entry.fields.eval <= alpha) // if current alpha >= cached alpha then the alpha during evaluation wont change, thus we can return the current alpha
                        return alpha;
                    if (entry.fields.flag > 1) // if the cached alpha is exact && it is bigger than the current alpha (because of the condition above) then we can return it
                        return entry.fields.eval;
                    beta = (beta > entry.fields.eval) ? entry.fields.eval : beta;
                    // cached alpha is lower bound
                }
                else
                {
                    if (entry.fields.eval > alpha)
                    {
                        if (entry.fields.eval >= beta)
                            return entry.fields.eval;
                        alpha = entry.fields.eval;
                    }
                }
            }

            alphabeg = alpha;
        }

        if (__builtin_expect(fir_link_mask & 24ULL, 0))
        {
            if (fir_link_mask & 8ULL)
            {
                field_t temp_field = position;

                temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp_field.is_boosted_mask &= ~8ULL;
                temp_field.is_fir_mask &= ~8ULL;
                temp_field.is_link_mask &= ~8ULL;
                ++temp_field.fir_link;

                BRANCH_ENTER_MAX("deposit close");
                int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field) : temp_field.evaluate();
                BRANCH_EXIT_MAX();

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MAX();
                    return alpha;
                }
            }
            if (fir_link_mask & 16ULL)
            {
                field_t temp_field = position;

                temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(16ULL) >> 3);
                temp_field.is_boosted_mask &= ~16ULL;
                temp_field.is_fir_mask &= ~16ULL;
                temp_field.is_link_mask &= ~16ULL;
                ++temp_field.fir_link;

                BRANCH_ENTER_MAX("deposit close");
                int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field) : temp_field.evaluate();
                BRANCH_EXIT_MAX();

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MAX();
                    return alpha;
                }
            }
        }
        else if (__builtin_expect((((fir_link_mask & 2052ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 8ULL) == 0) || (((fir_link_mask & 4128ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 16ULL) == 0), 0))
        {
            field_t temp_field = position;

            temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(cur_boosted_mask) >> 3);
            temp_field.is_boosted_mask &= ~cur_boosted_mask;
            temp_field.is_fir_mask &= ~cur_boosted_mask;
            temp_field.is_link_mask &= ~cur_boosted_mask;
            ++temp_field.fir_link;

            BRANCH_ENTER_MAX("deposit far");
            int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field) : temp_field.evaluate();
            BRANCH_EXIT_MAX();

            alpha = (reschild > alpha) ? reschild : alpha;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MAX();
                return alpha;
            }
        }

        if (cur_boosted_mask == 0)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;

                BRANCH_ENTER_MAX("boost virus");
                int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field) : temp_field.evaluate();
                BRANCH_EXIT_MAX();

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MAX();
                    return alpha;
                }

                temp ^= pos;
            }
        }
        else
        {
            const uint64_t cur_pos_bitboard = cur_boosted_mask;

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8); // double forward
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 16, 2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // forward right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // forward left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // double right
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // double left
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 2, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // backwards right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // backwards left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard << 8); // double backwards
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 16, -2, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                }
            }
        }

        uint64_t temp = fir_virus_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & secmask)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = fir_virus_mask & (~cur_boosted_mask);

        const uint64_t secmask_inv = ~secmask;

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & secmask_inv)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = fir_link_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_LINK)
            }

            temp ^= cur_pos_bitboard;
        }

        if (depth > SIMD_NAME(cur_search_depth) - COSTLY_POWERUPS_LOOKAHEAD)
        {
            if (position.firewall_fir == 0)
            {
                uint64_t temp = fir_link_mask & 16717361816799281127ULL;

                while (temp)
                {
                    const int bit_pos = __builtin_ctzll(temp);
                    const uint64_t pos = (1ULL << bit_pos); // front -> back

                    field_t temp_field = position;

                    temp_field.firewall_fir = (bit_pos << 1) | 1;

                    BRANCH_ENTER_MAX("firewall link");
                    int reschild;
                    if (depth > 0)
                    {
                        reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);
                        if (reschild > alpha && beta > alpha + 1)
                            reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);
                    }
                    else
                        reschild = temp_field.evaluate();
                    BRANCH_EXIT_MAX();

                    alpha = (reschild > alpha) ? reschild : alpha;
                    if (beta <= alpha)
                    {
                        if (depth > MIN_CACHE_DEPTH)
                            TRACK_ENTRY_MAX();
                        return alpha;
                    }

                    temp ^= pos;
                }

                temp = (fir_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

                if (temp)
                {
                    const int bit_pos = __builtin_ctzll(temp);

                    field_t temp_field = position;

                    temp_field.firewall_fir = (bit_pos << 1) | 1;

                    BRANCH_ENTER_MAX("firewall boosted virus");
                    int reschild;
                    if (depth > 0)
                    {
                        reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);
                        if (reschild > alpha && beta > alpha + 1)
                            reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);
                    }
                    else
                        reschild = temp_field.evaluate();
                    BRANCH_EXIT_MAX();

                    alpha = (reschild > alpha) ? reschild : alpha;
                    if (beta <= alpha)
                    {
                        if (depth > MIN_CACHE_DEPTH)
                            TRACK_ENTRY_MAX();
                        return alpha;
                    }
                }
            }
            else
            {
                field_t temp_field = position;

                temp_field.firewall_fir = 0;

                BRANCH_ENTER_MAX("un-firewall");
                int reschild;
                if (depth > 0)
                {
                    reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);
                    if (reschild > alpha && beta > alpha + 1)
                        reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);
                }
                else
                    reschild = temp_field.evaluate();
                BRANCH_EXIT_MAX();

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MAX();
                    return alpha;
                }
            }

            if (position.is_swap_available_fir)
            {
                uint64_t link_mask = fir_link_mask;

                while (link_mask)
                {
                    const uint64_t link_pos = (1ULL << __builtin_ctzll(link_mask)); // front -> back

                    uint64_t virus_mask = fir_virus_mask;

                    while (virus_mask)
                    {
                        const uint64_t virus_pos = (1ULL << __builtin_ctzll(virus_mask)); // front -> back

                        field_t temp_field = position;

                        temp_field.is_swap_available_fir = 0;
                        temp_field.is_link_mask ^= (link_pos | virus_pos);

                        BRANCH_ENTER_MAX("swap");
                        int reschild;
                        if (depth > 0)
                        {
                            reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);
                            if (reschild > alpha && beta > alpha + 1)
                                reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);
                        }
                        else
                            reschild = temp_field.evaluate();
                        BRANCH_EXIT_MAX();

                        alpha = (reschild > alpha) ? reschild : alpha;
                        if (beta <= alpha)
                        {
                            if (depth > MIN_CACHE_DEPTH)
                                TRACK_ENTRY_MAX();
                            return alpha;
                        }

                        virus_mask ^= virus_pos;
                    }

                    link_mask ^= link_pos;
                }
            }
        }

        if (cur_boosted_mask == 0)
        {
            uint64_t temp = fir_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;

                BRANCH_ENTER_MAX("boost link");
                int reschild;
                if (depth > 0)
                {
                    reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);
                    if (reschild > alpha && beta > alpha + 1)
                        reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);
                }
                else
                    reschild = temp_field.evaluate();
                BRANCH_EXIT_MAX();

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MAX();
                    return alpha;
                }

                temp ^= pos;
            }
        }
        else
        {
            field_t temp_field = position;

            temp_field.is_boosted_mask &= temp_field.is_sec_mask;

            BRANCH_ENTER_MAX("un-boost");
            int reschild;
            if (depth > 0)
            {
                reschild = SIMD_NAME(minimax)(depth, alpha, alpha + 1, false, temp_field);
                if (reschild > alpha && beta > alpha + 1)
                    reschild = SIMD_NAME(minimax)(depth, alpha, beta, false, temp_field);
            }
            else
                reschild = temp_field.evaluate();
            BRANCH_EXIT_MAX();

            alpha = (reschild > alpha) ? reschild : alpha;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MAX();
                return alpha;
            }
        }
        if (depth > MIN_CACHE_DEPTH)
        {
#ifdef BOOST_CACHE_BACKEND
            SIMD_NAME(cache)
            [depth][position] = {.fields = {alpha, (uint16_t)depth, (alpha > alphabeg) ? (uint16_t)3 : (uint16_t)1}};
#else
            uint64_t this_hash = position.hash();
            tt_store(&transposition_table[this_hash & (TABLE_SIZE - 1)], this_hash, alpha, (uint16_t)depth, (alpha > alphabeg) ? (uint16_t)3 : (uint16_t)1);
#endif
        }
        return alpha;
    }
    else
    {
        const uint64_t fir_link_mask = position.is_link_mask & firmask;
        const uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
        const uint64_t sec_virus_mask = secmask ^ sec_link_mask;

        const uint64_t cur_boosted_mask = position.is_boosted_mask & secmask;
        const uint64_t enemy_firewall_mask = (uint64_t)(position.firewall_fir & 1) << (position.firewall_fir >> 1);
        const uint64_t unmoveable_mask = firmask | secmask | enemy_firewall_mask;

        if (position.sec_link == 3) // fast path if we are about to win
        {
            if ((sec_link_mask & 1729382256910270464ULL) || (((sec_link_mask & 2310346608841064448ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 1152921504606846976ULL) == 0) ||
                (((sec_link_mask & 290482175965396992ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 576460752303423488ULL) == 0)) // bosted link can be deposited and nothing is blocking it
                return (-32768 * depth);
            if (fir_link_mask)
            {
                const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask;
                if (((links_masked_out >> 8) & secmask) ||                             // up
                    ((links_masked_out << 8) & secmask) ||                             // down
                    (((links_masked_out & 18374403900871474942ULL) >> 1) & secmask) || // left
                    (((links_masked_out & 9187201950435737471ULL) << 1) & secmask))    // right
                    return (-32768 * depth);

                if (cur_boosted_mask && links_masked_out)
                {
                    // unmoveable_mask should technically be enemy_virus_mask | ally_mask | enemy_firewall_mask but since enemy_link_mask is unreachable unmoveable_mask is used here

                    if ((((links_masked_out >> 16) & cur_boosted_mask) && ((unmoveable_mask >> 8) & cur_boosted_mask) == 0) ||                                                                                                              // up and not blocked
                        ((links_masked_out << 16) & cur_boosted_mask && ((unmoveable_mask << 8) & cur_boosted_mask) == 0) ||                                                                                                                // down and not blocked
                        ((((links_masked_out & 18229723555195321596ULL) >> 2) & cur_boosted_mask) && (((unmoveable_mask & 18374403900871474942ULL) >> 1) & cur_boosted_mask) == 0) ||                                                       // left and not blocked
                        ((((links_masked_out & 4557430888798830399ULL) << 2) & cur_boosted_mask) && (((unmoveable_mask & 9187201950435737471ULL) << 1) & cur_boosted_mask) == 0) ||                                                         // right and not blocked
                        ((((links_masked_out & 18374403900871474942ULL) >> 9) & cur_boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & cur_boosted_mask) == 0 || ((unmoveable_mask >> 8) & cur_boosted_mask) == 0)) || // up left
                        ((((links_masked_out & 18374403900871474942ULL) << 7) & cur_boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & cur_boosted_mask) == 0 || ((unmoveable_mask << 8) & cur_boosted_mask) == 0)) || // down left
                        ((((links_masked_out & 9187201950435737471ULL) << 9) & cur_boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & cur_boosted_mask) == 0 || ((unmoveable_mask << 8) & cur_boosted_mask) == 0)) ||   // down right
                        ((((links_masked_out & 9187201950435737471ULL) >> 7) & cur_boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & cur_boosted_mask) == 0 || ((unmoveable_mask >> 8) & cur_boosted_mask) == 0)))     // up right
                        return (-32768 * depth);
                }
            }
        }

        --depth;

        int betabeg;
        if (depth > MIN_CACHE_DEPTH)
        {
#ifdef BOOST_CACHE_BACKEND
            auto it = SIMD_NAME(cache)[depth].find(position);

            if (it != SIMD_NAME(cache)[depth].end() && (int)it->second.fields.depth >= depth)
            {
                tt_payload_t entry = it->second;
#else
            uint64_t this_hash = position.hash();
            tt_bucket_t *bucket = &transposition_table[this_hash & (TABLE_SIZE - 1)];
            tt_payload_t entry;

            if (tt_probe(bucket, this_hash, &entry) && entry.fields.depth >= depth)
            {

#endif
                if (__builtin_expect(entry.fields.flag & 1, 0))
                {
                    if (entry.fields.eval >= beta) // if current beta <= cached beta then the beta during evaluation wont change, thus we can return the current beta
                        return beta;
                    if (entry.fields.flag > 1) // if the cached beta is exact && it is smaller than the current beta (because of the condition above) then we can return it
                        return entry.fields.eval;
                    alpha = (alpha < entry.fields.eval) ? entry.fields.eval : alpha;
                    // cached beta is upper bound
                }
                else
                {
                    if (entry.fields.eval < beta)
                    {
                        if (entry.fields.eval <= alpha)
                            return entry.fields.eval;
                        beta = entry.fields.eval;
                    }
                }
            }
            betabeg = beta;
        }
        if (__builtin_expect(sec_link_mask & 1729382256910270464ULL, 0))
        {
            if (sec_link_mask & 576460752303423488ULL)
            {
                field_t temp_field = position;

                temp_field.forward_adv_sec -= (__builtin_ctzll(576460752303423488ULL) >> 3);
                temp_field.is_boosted_mask &= ~576460752303423488ULL;
                temp_field.is_sec_mask &= ~576460752303423488ULL;
                temp_field.is_link_mask &= ~576460752303423488ULL;
                ++temp_field.sec_link;

                BRANCH_ENTER_MIN("deposit close");
                int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field) : temp_field.evaluate();
                BRANCH_EXIT_MIN();

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MIN();
                    return beta;
                }
            }
            if (sec_link_mask & 1152921504606846976ULL)
            {
                field_t temp_field = position;

                temp_field.forward_adv_sec -= (__builtin_ctzll(1152921504606846976ULL) >> 3);
                temp_field.is_boosted_mask &= ~1152921504606846976ULL;
                temp_field.is_sec_mask &= ~1152921504606846976ULL;
                temp_field.is_link_mask &= ~1152921504606846976ULL;
                ++temp_field.sec_link;

                BRANCH_ENTER_MIN("deposit close");
                int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field) : temp_field.evaluate();
                BRANCH_EXIT_MIN();

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MIN();
                    return beta;
                }
            }
        }
        else if (__builtin_expect((((sec_link_mask & 2310346608841064448ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 1152921504606846976ULL) == 0) || (((sec_link_mask & 290482175965396992ULL) & cur_boosted_mask) != 0 && (unmoveable_mask & 576460752303423488ULL) == 0), 0))
        {
            field_t temp_field = position;

            temp_field.forward_adv_sec -= (__builtin_ctzll(cur_boosted_mask) >> 3);
            temp_field.is_boosted_mask &= ~cur_boosted_mask;
            temp_field.is_sec_mask &= ~cur_boosted_mask;
            temp_field.is_link_mask &= ~cur_boosted_mask;
            ++temp_field.sec_link;

            BRANCH_ENTER_MIN("deposit far");
            int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field) : temp_field.evaluate();
            BRANCH_EXIT_MIN();

            beta = (reschild < beta) ? reschild : beta;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MIN();
                return beta;
            }
        }

        if (cur_boosted_mask == 0)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;

                BRANCH_ENTER_MIN("boost virus");
                int reschild = (depth > 0) ? SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field) : temp_field.evaluate();
                BRANCH_EXIT_MIN();

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MIN();
                    return beta;
                }

                temp ^= pos;
            }
        }
        else
        {
            const uint64_t cur_pos_bitboard = cur_boosted_mask;

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8); // double forward
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 16, 2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // forward right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // forward left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard << 8)) == 0))
            {
                new_pos_bitboard <<= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // double right
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // double left
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard = ((new_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 2, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // backwards right
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // backwards left
            if (new_pos_bitboard && ((unmoveable_mask & new_pos_bitboard) == 0 || (unmoveable_mask & (cur_pos_bitboard >> 8)) == 0))
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8); // double backwards
            if (new_pos_bitboard && (unmoveable_mask & new_pos_bitboard) == 0)
            {
                new_pos_bitboard >>= 8;
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 16, -2, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                }
            }
        }

        uint64_t temp = sec_virus_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & firmask)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = sec_virus_mask & (~cur_boosted_mask);

        const uint64_t firmask_inv = ~firmask;

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard & firmask_inv)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = sec_link_mask & (~cur_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_LINK)
            }

            temp ^= cur_pos_bitboard;
        }

        if (depth > SIMD_NAME(cur_search_depth) - COSTLY_POWERUPS_LOOKAHEAD)
        {
            if (position.firewall_sec == 0)
            {
                uint64_t temp = sec_link_mask & 16717361816799281127ULL;

                while (temp)
                {
                    const int bit_pos = 63 - __builtin_clzll(temp);
                    const uint64_t pos = (1ULL << bit_pos); // front -> back

                    field_t temp_field = position;

                    temp_field.firewall_sec = (bit_pos << 1) | 1;

                    BRANCH_ENTER_MIN("firewall link");
                    int reschild;
                    if (depth > 0)
                    {
                        reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);
                        if (reschild < beta && alpha < beta - 1)
                            reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);
                    }
                    else
                        reschild = temp_field.evaluate();
                    BRANCH_EXIT_MIN();

                    beta = (reschild < beta) ? reschild : beta;
                    if (beta <= alpha)
                    {
                        if (depth > MIN_CACHE_DEPTH)
                            TRACK_ENTRY_MIN();
                        return beta;
                    }

                    temp ^= pos;
                }

                temp = (sec_virus_mask & 16717361816799281127ULL) & cur_boosted_mask;

                if (temp)
                {
                    const int bit_pos = 63 - __builtin_clzll(temp);

                    field_t temp_field = position;

                    temp_field.firewall_sec = (bit_pos << 1) | 1;

                    BRANCH_ENTER_MIN("firewall virus");
                    int reschild;
                    if (depth > 0)
                    {
                        reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);
                        if (reschild < beta && alpha < beta - 1)
                            reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);
                    }
                    else
                        reschild = temp_field.evaluate();
                    BRANCH_EXIT_MIN();

                    beta = (reschild < beta) ? reschild : beta;
                    if (beta <= alpha)
                    {
                        if (depth > MIN_CACHE_DEPTH)
                            TRACK_ENTRY_MIN();
                        return beta;
                    }
                }
            }
            else
            {
                field_t temp_field = position;

                temp_field.firewall_sec = 0;

                BRANCH_ENTER_MIN("un-firewall");
                int reschild;
                if (depth > 0)
                {
                    reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);
                    if (reschild < beta && alpha < beta - 1)
                        reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);
                }
                else
                    reschild = temp_field.evaluate();
                BRANCH_EXIT_MIN();

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MIN();
                    return beta;
                }
            }

            if (position.is_swap_available_sec)
            {
                uint64_t link_mask = sec_link_mask;

                while (link_mask)
                {
                    const uint64_t link_pos = (1ULL << (63 - __builtin_clzll(link_mask))); // front -> back

                    uint64_t virus_mask = sec_virus_mask;

                    while (virus_mask)
                    {
                        const uint64_t virus_pos = (1ULL << (63 - __builtin_clzll(virus_mask))); // front -> back

                        field_t temp_field = position;

                        temp_field.is_swap_available_sec = 0;
                        temp_field.is_link_mask ^= (link_pos | virus_pos);

                        BRANCH_ENTER_MIN("swap");
                        int reschild;
                        if (depth > 0)
                        {
                            reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);
                            if (reschild < beta && alpha < beta - 1)
                                reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);
                        }
                        else
                            reschild = temp_field.evaluate();
                        BRANCH_EXIT_MIN();

                        beta = (reschild < beta) ? reschild : beta;
                        if (beta <= alpha)
                        {
                            if (depth > MIN_CACHE_DEPTH)
                                TRACK_ENTRY_MIN();
                            return beta;
                        }

                        virus_mask ^= virus_pos;
                    }

                    link_mask ^= link_pos;
                }
            }
        }

        if (cur_boosted_mask == 0)
        {
            uint64_t temp = sec_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;

                BRANCH_ENTER_MIN("boost link");
                int reschild;
                if (depth > 0)
                {
                    reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);
                    if (reschild < beta && alpha < beta - 1)
                        reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);
                }
                else
                    reschild = temp_field.evaluate();
                BRANCH_EXIT_MIN();

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        TRACK_ENTRY_MIN();
                    return beta;
                }

                temp ^= pos;
            }
        }
        else
        {
            field_t temp_field = position;

            temp_field.is_boosted_mask &= temp_field.is_fir_mask;

            BRANCH_ENTER_MIN("un-boost");
            int reschild;
            if (depth > 0)
            {
                reschild = SIMD_NAME(minimax)(depth, beta - 1, beta, true, temp_field);
                if (reschild < beta && alpha < beta - 1)
                    reschild = SIMD_NAME(minimax)(depth, alpha, beta, true, temp_field);
            }
            else
                reschild = temp_field.evaluate();
            BRANCH_EXIT_MIN();

            beta = (reschild < beta) ? reschild : beta;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MIN();
                return beta;
            }
        }
        if (depth > MIN_CACHE_DEPTH)
        {
#ifdef BOOST_CACHE_BACKEND
            SIMD_NAME(cache)
            [depth][position] = {.fields = {beta, (uint16_t)depth, (beta > betabeg) ? (uint16_t)3 : (uint16_t)1}};
#else
            uint64_t this_hash = position.hash();
            tt_store(&transposition_table[this_hash & (TABLE_SIZE - 1)], this_hash, beta, (uint16_t)depth, (beta < betabeg) ? (uint16_t)3 : (uint16_t)1);
#endif
        }
        return beta;
    }

__cache_beta:
{
#ifdef BOOST_CACHE_BACKEND
    SIMD_NAME(cache)
    [depth][position] = {.fields = {beta, (uint16_t)depth, 0}};
#else
    uint64_t this_hash = position.hash();
    tt_store(&transposition_table[this_hash & (TABLE_SIZE - 1)], this_hash, beta, (uint16_t)depth, 0);
#endif
}
    return beta;

__cache_alpha:
{
#ifdef BOOST_CACHE_BACKEND
    SIMD_NAME(cache)
    [depth][position] = {.fields = {alpha, (uint16_t)depth, 0}};
#else
    uint64_t this_hash = position.hash();
    tt_store(&transposition_table[this_hash & (TABLE_SIZE - 1)], this_hash, alpha, (uint16_t)depth, 0);
#endif
}
    return alpha;
}

#undef PERFORM_ITERATION

minimax_main_result_t
SIMD_NAME(minimax_main)(const int depth, int alpha, int beta, const bool player, field_t *position)
{
    SIMD_NAME(cur_search_depth) = depth;
    struct timespec start, stop;

    assert(depth < MAX_DEPTH);

#ifdef RNAB_DEBUG
    printf("Calling minimax_main %s\n", STRINGIFY(SIMD_SUFFIX));
#endif

    if (player)
    {
        possible_moves_t
            all_moves = SIMD_NAME(possible_moves)(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * depth)};

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
                childres = SIMD_NAME(minimax)(depth - 1, alpha, beta, false, pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                debug_printf("Maximize first SIMD_NAME(minimax) call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
            }
            else
            {
                childres = SIMD_NAME(minimax)(depth - 1, alpha, alpha + 1, false, pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                if (childres > alpha)
                {
                    debug_printf("Maximize first SIMD_NAME(minimax) call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    childres = SIMD_NAME(minimax)(depth - 1, alpha, beta, false, pos);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    debug_printf("Maximize second SIMD_NAME(minimax) call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                }
                else
                {
                    debug_printf("Maximize first SIMD_NAME(minimax) call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                }
            }

            best_field = (childres > alpha) ? pos : best_field;
            alpha = (childres > alpha) ? childres : alpha;
            is_first_move = false;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = alpha};
    }
    else
    {
        possible_moves_t
            all_moves = SIMD_NAME(possible_moves)(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-32768 * depth)};

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
                childres = SIMD_NAME(minimax)(depth - 1, alpha, beta, true, pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                debug_printf("Minimize first SIMD_NAME(minimax) call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
            }
            else
            {
                childres = SIMD_NAME(minimax)(depth - 1, beta - 1, beta, true, pos);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                if (childres < beta)
                {
                    debug_printf("Minimize first SIMD_NAME(minimax) call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    childres = SIMD_NAME(minimax)(depth - 1, alpha, beta, true, pos);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    debug_printf("Minimize second SIMD_NAME(minimax) call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                }
                else
                {
                    debug_printf("Minimize first SIMD_NAME(minimax) call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                }
            }

            best_field = (childres < beta) ? pos : best_field;
            beta = (childres < beta) ? childres : beta;
            is_first_move = false;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = beta};
    }
}

minimax_main_result_t
SIMD_NAME(minimax_iteration_main)(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position)
{
    SIMD_NAME(cur_search_depth) = max_depth;
    assert(max_depth >= 2 && max_depth % 2 == 0 && "Depth must be at least 2 and even (divisible by 2) for iterative deepening");
    assert(max_search_time >= 100 && "max_search_time must be at least 100 milliseconds");
    assert(max_depth < MAX_DEPTH);

#ifdef RNAB_DEBUG
    printf("Calling minimax_iteration_main %s\n", STRINGIFY(SIMD_SUFFIX));
#endif

    struct timespec start, stop, global_start;
    minimax_main_result_t
        best_result;

    struct move_scores_wrapper
    {
        int move_id;
        int move_eval;
    };

    clock_gettime(CLOCK_MONOTONIC, &global_start);

    if (player)
    {
        possible_moves_t
            all_moves = SIMD_NAME(possible_moves)(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * max_depth), .has_timed_out = false};

        field_t best_field = all_moves.moves[0];
        int prev_alpha = alpha;

        move_scores_wrapper move_scores[MAX_MOVES];
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = {i, MIN};

        for (int current_depth = 2; current_depth <= max_depth; current_depth += 2)
        {
            int best_move_idx = -1;

            int64_t cur_rec_count = SIMD_NAME(rec_counter);
            clock_gettime(CLOCK_MONOTONIC, &start);

            CLEAR_TT();

            if (current_depth > 2)
            {
                for (int i = 1; i < all_moves.moves_count; ++i)
                {
                    auto key = move_scores[i];
                    int j = i - 1;
                    while (j >= 0)
                    {
                        if (move_scores[j].move_eval > key.move_eval)
                            break;
                        if (move_scores[j].move_eval == key.move_eval && move_scores[j].move_id <= key.move_id)
                            break;
                        move_scores[j + 1] = move_scores[j];
                        --j;
                    }
                    move_scores[j + 1] = key;
                }
            }

            int iteration_alpha = prev_alpha - 56;
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                int move_idx = move_scores[i].move_id;

                field_t pos = all_moves.moves[move_idx];

                int childres;
                if (i == 0) // very likely for the score to be higher
                {
                    childres = SIMD_NAME(minimax)(current_depth - 1, iteration_alpha, beta, false, pos);
                }
                else
                {
                    childres = SIMD_NAME(minimax)(current_depth - 1, iteration_alpha, iteration_alpha + 1, false, pos);
                    if (childres > iteration_alpha)
                        childres = SIMD_NAME(minimax)(current_depth - 1, iteration_alpha, beta, false, pos);
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
                    for (int i = 0; i < all_moves.moves_count; ++i)
                        debug_printf("%d, ", move_scores[i].move_id);
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
                    int move_idx = move_scores[i].move_id;

                    field_t pos = all_moves.moves[move_idx];

                    int childres;
                    if (i == 0)
                    {
                        childres = SIMD_NAME(minimax)(current_depth - 1, iteration_alpha, beta, false, pos);
                    }
                    else
                    {
                        childres = SIMD_NAME(minimax)(current_depth - 1, iteration_alpha, iteration_alpha + 1, false, pos);
                        if (childres > iteration_alpha)
                            childres = SIMD_NAME(minimax)(current_depth - 1, iteration_alpha, beta, false, pos);
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
                        for (int i = 0; i < all_moves.moves_count; ++i)
                            debug_printf("%d, ", move_scores[i].move_id);
                        debug_printf("\n");
                        debug_printf("timed out p2 %d/%d, best_move_idx=%d, eval=%d, off = %ld\n", i, all_moves.moves_count, best_move_idx, iteration_alpha, elapsed_time - max_search_time);
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
            debug_printf("Depth %d completed in %ld ms (best_move_idx = %d), evaluation: %d, checked_pos: %ld, pos/ms: %f\n",
                         current_depth,
                         (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000),
                         best_move_idx,
                         iteration_alpha,
                         SIMD_NAME(rec_counter) - cur_rec_count,
                         (double)(SIMD_NAME(rec_counter) - cur_rec_count) / ((double)(stop.tv_sec * 1000000000 + stop.tv_nsec - start.tv_sec * 1000000000 - start.tv_nsec) / 1000000.0));

            prev_alpha = iteration_alpha;
            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_alpha, .has_timed_out = false};
        }

        return best_result;
    }
    else
    {
        possible_moves_t
            all_moves = SIMD_NAME(possible_moves)(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-32768 * max_depth), .has_timed_out = false};

        field_t best_field = all_moves.moves[0];
        int prev_beta = beta;

        move_scores_wrapper move_scores[MAX_MOVES];
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = {i, MAX};

        for (int current_depth = 2; current_depth <= max_depth; current_depth += 2)
        {
            int best_move_idx = -1;

            int64_t cur_rec_count = SIMD_NAME(rec_counter);
            clock_gettime(CLOCK_MONOTONIC, &start);

            CLEAR_TT();

            if (current_depth > 2)
            {
                for (int i = 1; i < all_moves.moves_count; ++i)
                {
                    auto key = move_scores[i];
                    int j = i - 1;
                    while (j >= 0)
                    {
                        if (move_scores[j].move_eval < key.move_eval)
                            break;
                        if (move_scores[j].move_eval == key.move_eval && move_scores[j].move_id <= key.move_id)
                            break;
                        move_scores[j + 1] = move_scores[j];
                        --j;
                    }
                    move_scores[j + 1] = key;
                }
            }

            int iteration_beta = prev_beta + 56;
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                int move_idx = move_scores[i].move_id;

                int childres;

                if (i == 0) // very likely for the score to be higher
                {
                    childres = SIMD_NAME(minimax)(current_depth - 1, alpha, iteration_beta, true, all_moves.moves[move_idx]);
                }
                else
                {
                    childres = SIMD_NAME(minimax)(current_depth - 1, iteration_beta - 1, iteration_beta, true, all_moves.moves[move_idx]);
                    if (childres < iteration_beta)
                        childres = SIMD_NAME(minimax)(current_depth - 1, alpha, iteration_beta, true, all_moves.moves[move_idx]);
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
                    for (int i = 0; i < all_moves.moves_count; ++i)
                        debug_printf("%d, ", move_scores[i].move_id);
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
                    int childres;

                    if (i == 0)
                    {
                        childres = SIMD_NAME(minimax)(current_depth - 1, alpha, iteration_beta, true, all_moves.moves[move_idx]);
                    }
                    else
                    {
                        childres = SIMD_NAME(minimax)(current_depth - 1, iteration_beta - 1, iteration_beta, true, all_moves.moves[move_idx]);
                        if (childres < iteration_beta)
                            childres = SIMD_NAME(minimax)(current_depth - 1, alpha, iteration_beta, true, all_moves.moves[move_idx]);
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
                        for (int i = 0; i < all_moves.moves_count; ++i)
                            debug_printf("%d, ", move_scores[i].move_id);
                        debug_printf("\n");
                        debug_printf("timed out p2 %d/%d, best_move_idx=%d, eval=%d, off = %ld\n", i, all_moves.moves_count, best_move_idx, iteration_beta, elapsed_time - max_search_time);
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
            debug_printf("Depth %d completed in %ld ms (best_move_idx = %d), evaluation: %d, checked_pos: %ld, pos/ms: %f\n",
                         current_depth,
                         (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000),
                         best_move_idx,
                         iteration_beta,
                         SIMD_NAME(rec_counter) - cur_rec_count,
                         (double)(SIMD_NAME(rec_counter) - cur_rec_count) / ((double)(stop.tv_sec * 1000000000 + stop.tv_nsec - start.tv_sec * 1000000000 - start.tv_nsec) / 1000000.0));

            prev_beta = iteration_beta;
            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_beta, .has_timed_out = false};
        }

        return best_result;
    }
}

#endif
