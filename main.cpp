#include <boost/unordered/unordered_flat_map.hpp>
#include <pthread.h>
#include <time.h>
#include "amalgamation.h"

using namespace std;

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

const int init_pos_fir[8] = {63, 62, 61, 52, 51, 58, 57, 56};
const int init_pos_sec[8] = {0, 1, 2, 11, 12, 5, 6, 7};

#define MIN -1000000
#define MAX 1000000

#define MIN_CACHE_DEPTH 1
#define HIDE_ENEMY_CARDS false

#define ITERATION_CURRENT_IS_FIRST_UNKNOWN 0
#define ITERATION_CURRENT_IS_FIRST_LINK 1
#define ITERATION_CURRENT_IS_FIRST_VIRUS 2
#define ITERATION_CURRENT_IS_SECOND_UNKNOWN 3
#define ITERATION_CURRENT_IS_SECOND_LINK 4
#define ITERATION_CURRENT_IS_SECOND_VIRUS 5

#define COSTLY_POWERUPS_LOOKAHEAD 6

#ifdef ENGINE_DEBUG
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) ((void)0)
#endif

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

int cur_search_depth = 0;

struct field_t
{
    uint64_t is_fir_mask;
    uint64_t is_sec_mask;
    uint64_t is_link_mask;
    uint64_t is_boosted_mask;

    uint8_t fir_link : 4;
    uint8_t sec_link : 4;
    uint8_t fir_virus : 4;
    uint8_t sec_virus : 4;

    uint8_t is_boost_available_fir : 1;
    uint8_t is_boost_available_sec : 1;
    uint8_t is_checker_available_fir : 1;
    uint8_t is_checker_available_sec : 1;
    uint8_t is_swap_available_fir : 1;
    uint8_t is_swap_available_sec : 1;
    uint8_t is_firewall_available_fir : 1;
    uint8_t is_firewall_available_sec : 1;

    uint8_t forward_adv_fir;
    uint8_t forward_adv_sec;

    uint8_t firewall_fir;
    uint8_t firewall_sec;

    int evaluate() const // maximize
    {
        return ((1024 << fir_link) - (2048 << fir_virus) - (1024 << sec_link) + (2048 << sec_virus)) + (int)forward_adv_fir - (int)forward_adv_sec + 2048 * (int)is_swap_available_fir - 2048 * (int)is_swap_available_sec;
    }

    size_t operator()(const field_t &s) const
    {
        return s.is_fir_mask | s.is_sec_mask;
    }

    inline __attribute__((always_inline)) bool operator!=(const field_t &other) const
    {
        return !(*this == other);
    }
#if defined(__AVX512F__) && defined(__AVX512BW__)
    inline __attribute__((always_inline)) field_t &operator=(const field_t &other)
    {
        _mm512_mask_storeu_epi8(reinterpret_cast<void *>(this), 0xFFFFFFFFFFULL, _mm512_maskz_loadu_epi8(0xFFFFFFFFFFULL, reinterpret_cast<const void *>(&other)));
        return *this;
    }

    inline __attribute__((always_inline)) bool operator==(const field_t &other) const
    {
        return _mm512_cmpeq_epi8_mask(_mm512_maskz_loadu_epi8(0xFFFFFFFFFFULL, reinterpret_cast<const void *>(this)), _mm512_maskz_loadu_epi8(0xFFFFFFFFFFULL, reinterpret_cast<const void *>(&other))) == 0xFFFFFFFFFFFFFFFFULL;
    }

#elif defined(__AVX2__)
    inline __attribute__((always_inline)) field_t &operator=(const field_t &other)
    {
        _mm256_storeu_si256((__m256i *)(this), _mm256_loadu_si256((const __m256i *)(&other)));
        *((uint64_t *)this + 4) = *((uint64_t *)&other + 4);
        return *this;
    }

    inline __attribute__((always_inline)) bool operator==(const field_t &other) const
    {
        return _mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256((const __m256i *)(this)), _mm256_loadu_si256((const __m256i *)(&other)))) == 0xFFFFFFFF && *(uint64_t *)((uint8_t *)this + 32) == *(uint64_t *)((uint8_t *)&other + 32);
    }

#elif defined(__SSE2__)
    inline __attribute__((always_inline)) field_t &operator=(const field_t &other)
    {
        _mm_storeu_si128((__m128i *)((uint8_t *)this + 0), _mm_loadu_si128((const __m128i *)((uint8_t *)&other + 0)));
        _mm_storeu_si128((__m128i *)((uint8_t *)this + 16), _mm_loadu_si128((const __m128i *)((uint8_t *)&other + 16)));
        *((uint64_t *)this + 4) = *((uint64_t *)&other + 4);
        return *this;
    }

    inline __attribute__((always_inline)) bool operator==(const field_t &other) const
    {
        return _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i_u *)((uint8_t *)this + 0)), _mm_loadu_si128((const __m128i_u *)((uint8_t *)&other + 0)))) == 0xFFFF && _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i_u *)((uint8_t *)this + 16)), _mm_loadu_si128((const __m128i_u *)((uint8_t *)&other + 16)))) == 0xFFFF && *(uint64_t *)((uint8_t *)this + 32) == *(uint64_t *)((uint8_t *)&other + 32);
    }

#else
    inline __attribute__((always_inline)) field_t &operator=(const field_t &) = default;
    inline __attribute__((always_inline)) bool operator==(const field_t &) const = default;
#endif

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

size_t hash(field_t f)
{
    return f.is_fir_mask | f.is_sec_mask;
}

typedef struct
{
    int score;
    int flag;
} ttentry_t;

typedef struct
{
    field_t moves[80];
    int moves_count;
} possible_moves_t;

typedef struct
{
    field_t best_field;
    int evaluation;
} minimax_main_result_t;

typedef struct
{
    field_t *field_t;
    int score;
    int depth;
    int alpha;
    int beta;
} worker_t;

void field_construct(field_t &f, uint8_t pos_fir, uint8_t pos_sec)
{
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

    f.is_boost_available_fir = 1;
    f.is_boost_available_sec = 1;
    f.is_checker_available_fir = 1;
    f.is_checker_available_sec = 1;
    f.is_swap_available_fir = 1;
    f.is_swap_available_sec = 1;
    f.is_firewall_available_fir = 1;
    f.is_firewall_available_sec = 1;

    for (int i = 0; i < 8; ++i)
    {
        f.is_sec_mask |= (1ULL << init_pos_sec[i]);
        f.is_fir_mask |= (1ULL << init_pos_fir[i]);
        f.is_link_mask |= ((uint64_t)(((~pos_fir) >> i) & 1) << init_pos_fir[i]) | ((uint64_t)(((~pos_sec) >> i) & 1) << init_pos_sec[i]);
    }
}

int ai_level = 12;
int adaptive_ai_level = 12;

#define PERFORM_ITERATION(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                              \
    if ((new_pos_bitboard & enemy_firewall_mask) == 0)                                                                                                                 \
    {                                                                                                                                                                  \
        if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN || current_card == ITERATION_CURRENT_IS_FIRST_LINK || current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) \
        {                                                                                                                                                              \
            if (secmask & new_pos_bitboard)                                                                                                                            \
            {                                                                                                                                                          \
                if (sec_link_mask & new_pos_bitboard)                                                                                                                  \
                {                                                                                                                                                      \
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_sec |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                      \
                    temp.forward_adv_fir += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    temp.is_link_mask &= ~new_pos_bitboard;                                                                                                            \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.fir_link;                                                                                                                                   \
                                                                                                                                                                       \
                    res.moves[res.moves_count++] = temp;                                                                                                               \
                }                                                                                                                                                      \
                else if (position.fir_virus < 3)                                                                                                                       \
                {                                                                                                                                                      \
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_sec |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                      \
                    temp.forward_adv_fir += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.fir_virus;                                                                                                                                  \
                                                                                                                                                                       \
                    res.moves[res.moves_count++] = temp;                                                                                                               \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
            else if ((firmask & new_pos_bitboard) == 0)                                                                                                                \
            {                                                                                                                                                          \
                field_t temp = position;                                                                                                                               \
                                                                                                                                                                       \
                temp.forward_adv_fir += forward_adv;                                                                                                                   \
                if (is_boosted)                                                                                                                                        \
                    temp.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                     \
                temp.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                             \
                if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                                \
                {                                                                                                                                                      \
                    uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                              \
                    temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                       \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                              \
                {                                                                                                                                                      \
                    temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                        \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                res.moves[res.moves_count++] = temp;                                                                                                                   \
            }                                                                                                                                                          \
        }                                                                                                                                                              \
        else                                                                                                                                                           \
        {                                                                                                                                                              \
            if (firmask & new_pos_bitboard)                                                                                                                            \
            {                                                                                                                                                          \
                if (fir_link_mask & new_pos_bitboard)                                                                                                                  \
                {                                                                                                                                                      \
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_fir |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                                  \
                    temp.forward_adv_sec += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    temp.is_link_mask &= ~new_pos_bitboard;                                                                                                            \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.sec_link;                                                                                                                                   \
                                                                                                                                                                       \
                    res.moves[res.moves_count++] = temp;                                                                                                               \
                }                                                                                                                                                      \
                else if (position.sec_virus < 3)                                                                                                                       \
                {                                                                                                                                                      \
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_fir |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                                  \
                    temp.forward_adv_sec += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.sec_virus;                                                                                                                                  \
                                                                                                                                                                       \
                    res.moves[res.moves_count++] = temp;                                                                                                               \
                }                                                                                                                                                      \
            }                                                                                                                                                          \
            else if ((secmask & new_pos_bitboard) == 0)                                                                                                                \
            {                                                                                                                                                          \
                field_t temp = position;                                                                                                                               \
                                                                                                                                                                       \
                temp.forward_adv_sec += forward_adv;                                                                                                                   \
                if (is_boosted)                                                                                                                                        \
                    temp.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                     \
                temp.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                             \
                if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                               \
                {                                                                                                                                                      \
                    uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                              \
                    temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                       \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                             \
                {                                                                                                                                                      \
                    temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                        \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                res.moves[res.moves_count++] = temp;                                                                                                                   \
            }                                                                                                                                                          \
        }                                                                                                                                                              \
    }

possible_moves_t possiblemoves(const field_t &position, const bool player)
{
    possible_moves_t res;
    res.moves_count = 0;

    const uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
    const uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
    const uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
    const uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;

    if (player)
    {
        const uint64_t enemy_firewall_mask = ((position.is_firewall_available_sec) ? 0 : (1ULL << position.firewall_sec));
        const uint64_t unmoveable_mask = position.is_fir_mask | position.is_sec_mask | enemy_firewall_mask;

        if (__builtin_expect(fir_link_mask & 24ULL, 0))
        {
            if (fir_link_mask & 8ULL)
            {
                field_t temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp.is_boost_available_fir |= ((temp.is_boosted_mask >> __builtin_ctzll(8ULL)) & 1);
                temp.is_boosted_mask &= ~8ULL;
                temp.is_fir_mask &= ~8ULL;
                temp.is_link_mask &= ~8ULL;
                ++temp.fir_link;

                res.moves[res.moves_count++] = temp;
            }
            if (fir_link_mask & 16ULL)
            {
                field_t temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp.is_boost_available_fir |= ((temp.is_boosted_mask >> __builtin_ctzll(16ULL)) & 1);
                temp.is_boosted_mask &= ~16ULL;
                temp.is_fir_mask &= ~16ULL;
                temp.is_link_mask &= ~16ULL;
                ++temp.fir_link;

                res.moves[res.moves_count++] = temp;
            }
        }
        else if (__builtin_expect((((fir_link_mask & 2052ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 8ULL) == 0) || (((fir_link_mask & 4128ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 16ULL) == 0), 0))
        {
            field_t temp = position;

            uint64_t boosted_card = position.is_fir_mask & position.is_boosted_mask;

            temp.forward_adv_fir -= 7 - (__builtin_ctzll(boosted_card) >> 3);
            temp.is_boost_available_fir = 1;
            temp.is_boosted_mask &= ~boosted_card;
            temp.is_fir_mask &= ~boosted_card;
            temp.is_link_mask &= ~boosted_card;
            ++temp.fir_link;

            res.moves[res.moves_count++] = temp;
        }

        const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;

        if (position.is_boost_available_fir)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                uint64_t pos = (1ULL << __builtin_ctzll(temp));

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_fir = 0;
                res.moves[res.moves_count++] = temp_field;

                temp ^= pos;
            }
        }
        else
        {
            const uint64_t cur_pos_bitboard = firmask & position.is_boosted_mask;

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

        uint64_t temp = fir_virus_mask & (~position.is_boosted_mask);

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

        temp = fir_virus_mask & (~position.is_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = fir_link_mask & (~position.is_boosted_mask);

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

        if (position.is_firewall_available_fir)
        {
            uint64_t temp = fir_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = __builtin_ctzll(temp);
                const uint64_t pos = (1ULL << bit_pos); // front -> back

                field_t temp_field = position;

                temp_field.is_firewall_available_fir = 0;
                temp_field.firewall_fir = bit_pos;

                res.moves[res.moves_count++] = temp_field;

                temp ^= pos;
            }
        }
        else
        {
            field_t temp_field = position;

            temp_field.is_firewall_available_fir = 1;

            res.moves[res.moves_count++] = temp_field;
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

                    res.moves[res.moves_count++] = temp_field;

                    virus_mask ^= virus_pos;
                }

                link_mask ^= link_pos;
            }
        }

        if (position.is_boost_available_fir)
        {
            uint64_t temp = fir_link_mask;

            while (temp)
            {
                uint64_t pos = (1ULL << __builtin_ctzll(temp));

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_fir = 0;
                res.moves[res.moves_count++] = temp_field;

                temp ^= pos;
            }
        }
        else
        {
            field_t temp = position;

            temp.is_boost_available_fir = 1;
            temp.is_boosted_mask &= temp.is_sec_mask;

            res.moves[res.moves_count++] = temp;
        }
    }
    else
    {
        const uint64_t enemy_firewall_mask = ((position.is_firewall_available_fir) ? 0 : (1ULL << position.firewall_fir));
        const uint64_t unmoveable_mask = position.is_fir_mask | position.is_sec_mask | enemy_firewall_mask;

        if (__builtin_expect(sec_link_mask & 1729382256910270464ULL, 0))
        {
            if (sec_link_mask & 576460752303423488ULL)
            {
                field_t temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(576460752303423488ULL) >> 3);
                temp.is_boost_available_sec |= ((temp.is_boosted_mask >> __builtin_ctzll(576460752303423488ULL)) & 1);
                temp.is_boosted_mask &= ~576460752303423488ULL;
                temp.is_sec_mask &= ~576460752303423488ULL;
                temp.is_link_mask &= ~576460752303423488ULL;
                ++temp.sec_link;

                res.moves[res.moves_count++] = temp;
            }
            if (sec_link_mask & 1152921504606846976ULL)
            {
                field_t temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(1152921504606846976ULL) >> 3);
                temp.is_boost_available_sec |= ((temp.is_boosted_mask >> __builtin_ctzll(1152921504606846976ULL)) & 1);
                temp.is_boosted_mask &= ~1152921504606846976ULL;
                temp.is_sec_mask &= ~1152921504606846976ULL;
                temp.is_link_mask &= ~1152921504606846976ULL;
                ++temp.sec_link;

                res.moves[res.moves_count++] = temp;
            }
        }
        else if (__builtin_expect((((sec_link_mask & 2310346608841064448ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 1152921504606846976ULL) == 0) || (((sec_link_mask & 290482175965396992ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 576460752303423488ULL) == 0), 0))
        {
            field_t temp = position;

            uint64_t boosted_card = position.is_sec_mask & position.is_boosted_mask;

            temp.forward_adv_sec -= (__builtin_ctzll(boosted_card) >> 3);
            temp.is_boost_available_sec = 1;
            temp.is_boosted_mask &= ~boosted_card;
            temp.is_sec_mask &= ~boosted_card;
            temp.is_link_mask &= ~boosted_card;
            ++temp.sec_link;

            res.moves[res.moves_count++] = temp;
        }

        const uint64_t firmask = (fir_link_mask | fir_virus_mask), secmask = (sec_link_mask | sec_virus_mask);

        if (position.is_boost_available_sec)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_sec = 0;
                res.moves[res.moves_count++] = temp_field;

                temp ^= pos;
            }
        }
        else
        {
            const uint64_t cur_pos_bitboard = secmask & position.is_boosted_mask;

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

        uint64_t temp = sec_virus_mask & (~position.is_boosted_mask);

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

        temp = sec_virus_mask & (~position.is_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = sec_link_mask & (~position.is_boosted_mask);

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

        if (position.is_firewall_available_sec)
        {
            uint64_t temp = sec_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = __builtin_clzll(temp);
                const uint64_t pos = (1ULL << (63 - bit_pos)); // front -> back

                field_t temp_field = position;

                temp_field.is_firewall_available_sec = 0;
                temp_field.firewall_sec = (63 - bit_pos);

                res.moves[res.moves_count++] = temp_field;

                temp ^= pos;
            }
        }
        else
        {
            field_t temp_field = position;

            temp_field.is_firewall_available_sec = 1;

            res.moves[res.moves_count++] = temp_field;
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

                    res.moves[res.moves_count++] = temp_field;

                    virus_mask ^= virus_pos;
                }

                link_mask ^= link_pos;
            }
        }

        if (position.is_boost_available_sec)
        {
            uint64_t temp = sec_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_sec = 0;
                res.moves[res.moves_count++] = temp_field;

                temp ^= pos;
            }
        }
        else
        {
            field_t temp = position;

            temp.is_boost_available_sec = 1;
            temp.is_boosted_mask &= temp.is_fir_mask;

            res.moves[res.moves_count++] = temp;
        }
    }

    return res;
}

#define PERFORM_ITERATION(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                              \
    if ((new_pos_bitboard & enemy_firewall_mask) == 0)                                                                                                                 \
    {                                                                                                                                                                  \
        if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN || current_card == ITERATION_CURRENT_IS_FIRST_LINK || current_card == ITERATION_CURRENT_IS_FIRST_VIRUS) \
        {                                                                                                                                                              \
            if (secmask & new_pos_bitboard)                                                                                                                            \
            {                                                                                                                                                          \
                if (sec_link_mask & new_pos_bitboard)                                                                                                                  \
                {                                                                                                                                                      \
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_sec |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                      \
                    temp.forward_adv_fir += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    temp.is_link_mask &= ~new_pos_bitboard;                                                                                                            \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.fir_link;                                                                                                                                   \
                                                                                                                                                                       \
                    BRANCH_ENTER_MAX();                                                                                                                                \
                    int reschild = minimax(depth, alpha, beta, false, temp, cache);                                                                                    \
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
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_sec |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_sec -= (new_pos_coord >> 3);                                                                                                      \
                    temp.forward_adv_fir += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                            \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                          \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.fir_virus;                                                                                                                                  \
                                                                                                                                                                       \
                    BRANCH_ENTER_MAX();                                                                                                                                \
                    int reschild = minimax(depth, alpha, beta, false, temp, cache);                                                                                    \
                    BRANCH_EXIT_MAX();                                                                                                                                 \
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
                field_t temp = position;                                                                                                                               \
                                                                                                                                                                       \
                temp.forward_adv_fir += forward_adv;                                                                                                                   \
                if (is_boosted)                                                                                                                                        \
                    temp.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                     \
                temp.is_fir_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                             \
                if (current_card == ITERATION_CURRENT_IS_FIRST_UNKNOWN)                                                                                                \
                {                                                                                                                                                      \
                    uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                              \
                    temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                       \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_FIRST_LINK)                                                                                              \
                {                                                                                                                                                      \
                    temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                        \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                BRANCH_ENTER_MAX();                                                                                                                                    \
                int reschild = minimax(depth, alpha, beta, false, temp, cache);                                                                                        \
                BRANCH_EXIT_MAX();                                                                                                                                     \
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
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_fir |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                                  \
                    temp.forward_adv_sec += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    temp.is_link_mask &= ~new_pos_bitboard;                                                                                                            \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.sec_link;                                                                                                                                   \
                                                                                                                                                                       \
                    BRANCH_ENTER_MIN();                                                                                                                                \
                    int reschild = minimax(depth, alpha, beta, true, temp, cache);                                                                                     \
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
                    field_t temp = position;                                                                                                                           \
                                                                                                                                                                       \
                    int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                    temp.is_boost_available_fir |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                                 \
                    temp.forward_adv_fir -= 7 - (new_pos_coord >> 3);                                                                                                  \
                    temp.forward_adv_sec += forward_adv;                                                                                                               \
                    if (is_boosted)                                                                                                                                    \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask ^= (cur_pos_bitboard);                                                                                                    \
                        temp.is_boosted_mask |= (new_pos_bitboard);                                                                                                    \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        temp.is_boosted_mask &= ~(new_pos_bitboard);                                                                                                   \
                    }                                                                                                                                                  \
                    temp.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                         \
                    if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                           \
                    {                                                                                                                                                  \
                        uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                          \
                        temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                   \
                    }                                                                                                                                                  \
                    else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                         \
                    {                                                                                                                                                  \
                        temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                    \
                    }                                                                                                                                                  \
                    temp.is_fir_mask ^= new_pos_bitboard;                                                                                                              \
                    ++temp.sec_virus;                                                                                                                                  \
                                                                                                                                                                       \
                    BRANCH_ENTER_MIN();                                                                                                                                \
                    int reschild = minimax(depth, alpha, beta, true, temp, cache);                                                                                     \
                    BRANCH_EXIT_MIN();                                                                                                                                 \
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
                field_t temp = position;                                                                                                                               \
                                                                                                                                                                       \
                temp.forward_adv_sec += forward_adv;                                                                                                                   \
                if (is_boosted)                                                                                                                                        \
                    temp.is_boosted_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                     \
                temp.is_sec_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                             \
                if (current_card == ITERATION_CURRENT_IS_SECOND_UNKNOWN)                                                                                               \
                {                                                                                                                                                      \
                    uint64_t mask = temp.is_link_mask & cur_pos_bitboard;                                                                                              \
                    temp.is_link_mask ^= (mask | (mask shift_func shift_count));                                                                                       \
                }                                                                                                                                                      \
                else if (current_card == ITERATION_CURRENT_IS_SECOND_LINK)                                                                                             \
                {                                                                                                                                                      \
                    temp.is_link_mask ^= (cur_pos_bitboard | new_pos_bitboard);                                                                                        \
                }                                                                                                                                                      \
                                                                                                                                                                       \
                BRANCH_ENTER_MIN();                                                                                                                                    \
                int reschild = minimax(depth, alpha, beta, true, temp, cache);                                                                                         \
                BRANCH_EXIT_MIN();                                                                                                                                     \
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

typedef struct
{
    int64_t total_entries;
    int64_t cutoff_entries;
    int64_t improved_score;
    int temp_score;
    int pad;
} cutoff_tracker_t;

static cutoff_tracker_t cutoff_tracker[1000] = {0};

#ifdef BRANCH_DEBUG

#define BEGIN_BRANCH_TRACKING() \
    static constexpr int _branch_counter_base = __COUNTER__

#define BRANCH_ENTER_MAX()                                                                \
    static constexpr int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    cutoff_tracker[_branch_idx_##__LINE__].total_entries++;                               \
    cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries++;                              \
    cutoff_tracker[_branch_idx_##__LINE__].temp_score = alpha

#define BRANCH_ENTER_MIN()                                                                \
    static constexpr int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    cutoff_tracker[_branch_idx_##__LINE__].total_entries++;                               \
    cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries++;                              \
    cutoff_tracker[_branch_idx_##__LINE__].temp_score = beta

#define BRANCH_EXIT_MAX()                                             \
    if (beta > reschild)                                              \
        cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries--;      \
    if (reschild > cutoff_tracker[_branch_idx_##__LINE__].temp_score) \
    cutoff_tracker[_branch_idx_##__LINE__].improved_score++

#define BRANCH_EXIT_MIN()                                             \
    if (reschild > alpha)                                             \
        cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries--;      \
    if (reschild < cutoff_tracker[_branch_idx_##__LINE__].temp_score) \
    cutoff_tracker[_branch_idx_##__LINE__].improved_score++

#define GET_BRANCH_COUNT() \
    (__COUNTER__ - _branch_counter_base - 1)

#else

#define BEGIN_BRANCH_TRACKING()
#define BRANCH_ENTER_MAX()
#define BRANCH_ENTER_MIN()
#define BRANCH_EXIT_MAX()
#define BRANCH_EXIT_MIN()
#define GET_BRANCH_COUNT() 0

#endif

typedef struct
{
    int64_t total_entries;
    int64_t lookup_entries;
} cache_tracker_t;

static cache_tracker_t cache_entry_tracker[1000] = {0};

#ifdef CACHE_DEBUG

#define BEGIN_CACHE_TRACKING() \
    static constexpr int _cache_counter_base = __COUNTER__

#define TRACK_ENTRY_MAX()                                                                   \
    {                                                                                       \
        static constexpr int _entry_idx_##__LINE__ = __COUNTER__ - _cache_counter_base - 1; \
        cache_entry_tracker[_entry_idx_##__LINE__].total_entries++;                         \
        cache[depth][position] = {alpha, 0, _entry_idx_##__LINE__};                         \
    }

#define TRACK_ENTRY_MAX_END()                                                                \
    {                                                                                        \
        static constexpr int _entry_idx_##__LINE__ = __COUNTER__ - _cache_counter_base - 1;  \
        cache_entry_tracker[_entry_idx_##__LINE__].total_entries++;                          \
        cache[depth][position] = {alpha, (alpha > alphabeg) ? 3 : 1, _entry_idx_##__LINE__}; \
    }

#define TRACK_ENTRY_MIN()                                                                   \
    {                                                                                       \
        static constexpr int _entry_idx_##__LINE__ = __COUNTER__ - _cache_counter_base - 1; \
        cache_entry_tracker[_entry_idx_##__LINE__].total_entries++;                         \
        cache[depth][position] = {beta, 0, _entry_idx_##__LINE__};                          \
    }

#define TRACK_ENTRY_MIN_END()                                                               \
    {                                                                                       \
        static constexpr int _entry_idx_##__LINE__ = __COUNTER__ - _cache_counter_base - 1; \
        cache_entry_tracker[_entry_idx_##__LINE__].total_entries++;                         \
        cache[depth][position] = {beta, (beta < betabeg) ? 3 : 1, _entry_idx_##__LINE__};   \
    }

#define GET_CACHE_COUNT() \
    (__COUNTER__ - _cache_counter_base - 1)

#else

#define BEGIN_CACHE_TRACKING()
#define TRACK_ENTRY_MAX()   \
    {                       \
        goto __cache_alpha; \
    }
#define TRACK_ENTRY_MIN()  \
    {                      \
        goto __cache_beta; \
    }
#define TRACK_ENTRY_MAX_END()                                         \
    {                                                                 \
        cache[depth][position] = {alpha, (alpha > alphabeg) ? 3 : 1}; \
    }
#define TRACK_ENTRY_MIN_END()                                      \
    {                                                              \
        cache[depth][position] = {beta, (beta < betabeg) ? 3 : 1}; \
    }

#define GET_CACHE_COUNT() 0

#endif

BEGIN_BRANCH_TRACKING();
BEGIN_CACHE_TRACKING();

int minimax(int depth, int alpha, int beta, const bool player, const field_t &position, boost::unordered_flat_map<field_t, ttentry_t, field_t> *cache)
{
    if (player)
    {
        if (depth == 0)
            return position.evaluate();
        --depth;

        const uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
        const uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
        const uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
        const uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;
        const uint64_t enemy_firewall_mask = ((position.is_firewall_available_sec) ? 0 : (1ULL << position.firewall_sec));
        const uint64_t unmoveable_mask = position.is_fir_mask | position.is_sec_mask | enemy_firewall_mask;

        if (position.fir_link == 3)
        {
            if (fir_link_mask & 24)
                return (32768 * (depth + 1));
            if ((((fir_link_mask & 2052ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 8ULL) == 0) ||
                (((fir_link_mask & 4128ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 16ULL) == 0))
                return (32768 * (depth + 1));
            if (sec_link_mask)
            {
                const uint64_t firmask = position.is_fir_mask, links_masked_out = sec_link_mask & ~enemy_firewall_mask;
                if (((links_masked_out >> 8) & firmask) ||                             // up
                    ((links_masked_out << 8) & firmask) ||                             // down
                    (((links_masked_out & 18374403900871474942ULL) >> 1) & firmask) || // left
                    (((links_masked_out & 9187201950435737471ULL) << 1) & firmask))    // right
                    return (32768 * (depth + 1));
                if (position.is_boost_available_fir == 0)
                {
                    uint64_t boosted_mask = position.is_boosted_mask & firmask;

                    if ((((links_masked_out >> 16) & boosted_mask) && ((unmoveable_mask >> 8) & boosted_mask) == 0) ||                                                                                                          // up and not blocked
                        ((links_masked_out << 16) & boosted_mask && ((unmoveable_mask << 8) & boosted_mask) == 0) ||                                                                                                            // down and not blocked
                        ((((links_masked_out & 18229723555195321596ULL) >> 2) & boosted_mask) && (((unmoveable_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0) ||                                                   // left and not blocked
                        ((((links_masked_out & 4557430888798830399ULL) << 2) & boosted_mask) && (((unmoveable_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0) ||                                                     // right and not blocked
                        ((((links_masked_out & 18374403900871474942ULL) >> 9) & boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((unmoveable_mask >> 8) & boosted_mask) == 0)) || // up left
                        ((((links_masked_out & 18374403900871474942ULL) << 7) & boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((unmoveable_mask << 8) & boosted_mask) == 0)) || // down left
                        ((((links_masked_out & 9187201950435737471ULL) << 9) & boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((unmoveable_mask << 8) & boosted_mask) == 0)) ||   // down right
                        ((((links_masked_out & 9187201950435737471ULL) >> 7) & boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((unmoveable_mask >> 8) & boosted_mask) == 0)))     // up right
                        return (32768 * (depth + 1));
                }
            }
        }

        int alphabeg;
        if (depth > MIN_CACHE_DEPTH)
        {
            auto it = cache[depth].find(position);
            if (it != cache[depth].end())
            {
#ifdef CACHE_DEBUG
                cache_entry_tracker[it->second.cache_id].lookup_entries++;
#endif

                ttentry_t entry = it->second;
                if (__builtin_expect(entry.flag & 1, 0))
                {
                    if (entry.score <= alpha) // if current alpha >= cached alpha then the alpha during evaluation wont change, thus we can return the current alpha
                        return alpha;
                    if (entry.flag > 1) // if the cached alpha is exact && it is bigger than the current alpha (because of the condition above) then we can return it
                        return entry.score;
                    beta = (beta > entry.score) ? entry.score : beta;
                    // cached alpha is lower bound
                }
                else
                {
                    if (entry.score > alpha)
                    {
                        if (entry.score >= beta)
                            return entry.score;
                        alpha = entry.score;
                    }
                }
            }
            alphabeg = alpha;
        }

        if (__builtin_expect(fir_link_mask & 24ULL, 0))
        {
            if (fir_link_mask & 8ULL)
            {
                field_t temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp.is_boost_available_fir |= ((temp.is_boosted_mask >> __builtin_ctzll(8ULL)) & 1);
                temp.is_boosted_mask &= ~8ULL;
                temp.is_fir_mask &= ~8ULL;
                temp.is_link_mask &= ~8ULL;
                ++temp.fir_link;
                BRANCH_ENTER_MAX();
                int reschild = minimax(depth, alpha, beta, false, temp, cache);
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
                field_t temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(16ULL) >> 3);
                temp.is_boost_available_fir |= ((temp.is_boosted_mask >> __builtin_ctzll(16ULL)) & 1);
                temp.is_boosted_mask &= ~16ULL;
                temp.is_fir_mask &= ~16ULL;
                temp.is_link_mask &= ~16ULL;
                ++temp.fir_link;

                BRANCH_ENTER_MAX();
                int reschild = minimax(depth, alpha, beta, false, temp, cache);
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
        else if (__builtin_expect((((fir_link_mask & 2052ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 8ULL) == 0) || (((fir_link_mask & 4128ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 16ULL) == 0), 0))
        {
            field_t temp = position;

            uint64_t boosted_card = position.is_fir_mask & position.is_boosted_mask;

            temp.forward_adv_fir -= 7 - (__builtin_ctzll(boosted_card) >> 3);
            temp.is_boost_available_fir = 1;
            temp.is_boosted_mask &= ~boosted_card;
            temp.is_fir_mask &= ~boosted_card;
            temp.is_link_mask &= ~boosted_card;
            ++temp.fir_link;

            BRANCH_ENTER_MAX();
            int reschild = minimax(depth, alpha, beta, false, temp, cache);
            BRANCH_EXIT_MAX();
            alpha = (reschild > alpha) ? reschild : alpha;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MAX();
                return alpha;
            }
        }

        const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;

        // uint64_t boosted_enemy_mask = (position.is_sec_mask & position.is_boosted_mask);
        // uint64_t enemy_attack_mask = position.is_sec_mask |
        //                              (position.is_sec_mask >> 8) |                           // backward
        //                              (position.is_sec_mask << 8) |                           // forward
        //                              ((position.is_sec_mask & 0x7f7f7f7f7f7f7f7fULL) << 1) | // left
        //                              ((position.is_sec_mask & 0xfefefefefefefefeULL) >> 1) | // right
        //                              ((boosted_enemy_mask & 0x7f7f7f7f7f7f7f7fULL) << 9) |
        //                              ((boosted_enemy_mask & 0x7f7f7f7f7f7f7f7fULL) >> 7) |
        //                              ((boosted_enemy_mask & 0xfefefefefefefefeULL) << 7) |
        //                              ((boosted_enemy_mask & 0xfefefefefefefefeULL) >> 9) |
        //                              (boosted_enemy_mask >> 16) |
        //                              (boosted_enemy_mask << 16) |
        //                              ((boosted_enemy_mask & 0xfcfcfcfcfcfcfcfcULL) >> 2) |
        //                              ((boosted_enemy_mask & 0xcfcfcfcfcfcfcfcfULL) << 2);

        if (position.is_boost_available_fir)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_fir = 0;

                BRANCH_ENTER_MAX();
                int reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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
            const uint64_t cur_pos_bitboard = firmask & position.is_boosted_mask;

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

        uint64_t temp = fir_virus_mask & (~position.is_boosted_mask);

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

        temp = fir_virus_mask & (~position.is_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));

            uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard && (new_pos_bitboard & secmask) == 0)
            {
                PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = fir_link_mask & (~position.is_boosted_mask);

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

        if (position.is_firewall_available_fir && depth > cur_search_depth - COSTLY_POWERUPS_LOOKAHEAD)
        {
            uint64_t temp = fir_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = __builtin_ctzll(temp);
                const uint64_t pos = (1ULL << bit_pos); // front -> back

                field_t temp_field = position;

                temp_field.is_firewall_available_fir = 0;
                temp_field.firewall_fir = bit_pos;

                BRANCH_ENTER_MAX();
                int reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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
        else if (position.is_firewall_available_fir == 0)
        {
            field_t temp_field = position;

            temp_field.is_firewall_available_fir = 1;

            BRANCH_ENTER_MAX();
            int reschild = minimax(depth, alpha, beta, false, temp_field, cache);
            BRANCH_EXIT_MAX();

            alpha = (reschild > alpha) ? reschild : alpha;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MAX();
                return alpha;
            }
        }

        if (position.is_swap_available_fir && depth > cur_search_depth - COSTLY_POWERUPS_LOOKAHEAD)
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

                    BRANCH_ENTER_MAX();
                    int reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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

        if (position.is_boost_available_fir)
        {
            uint64_t temp = fir_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_fir = 0;

                BRANCH_ENTER_MAX();
                int reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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
            field_t temp = position;

            temp.is_boost_available_fir = 1;
            temp.is_boosted_mask &= temp.is_sec_mask;

            BRANCH_ENTER_MAX();
            int reschild = minimax(depth, alpha, beta, false, temp, cache);
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
            TRACK_ENTRY_MAX_END();
        return alpha;
    }
    else
    {
        if (depth == 0)
            return position.evaluate();
        --depth;

        const uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
        const uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
        const uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
        const uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;
        const uint64_t enemy_firewall_mask = ((position.is_firewall_available_fir) ? 0 : (1ULL << position.firewall_fir));
        const uint64_t unmoveable_mask = position.is_fir_mask | position.is_sec_mask | enemy_firewall_mask;

        if (position.sec_link == 3)
        {
            if (sec_link_mask & 1729382256910270464ULL)
                return (-32768 * (depth + 1));
            if ((((sec_link_mask & 2310346608841064448ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 1152921504606846976ULL) == 0) ||
                (((sec_link_mask & 290482175965396992ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 576460752303423488ULL) == 0))
                return (-32768 * (depth + 1));
            if (fir_link_mask)
            {
                const uint64_t links_masked_out = fir_link_mask & ~enemy_firewall_mask, secmask = position.is_sec_mask;
                if (((links_masked_out >> 8) & secmask) ||                             // up
                    ((links_masked_out << 8) & secmask) ||                             // down
                    (((links_masked_out & 18374403900871474942ULL) >> 1) & secmask) || // left
                    (((links_masked_out & 9187201950435737471ULL) << 1) & secmask))    // right
                    return (-32768 * (depth + 1));
                if (position.is_boost_available_sec == 0)
                {
                    uint64_t boosted_mask = position.is_boosted_mask & secmask;

                    // unmoveable_mask should technically be enemy_virus_mask | ally_mask | enemy_firewall_mask but since enemy_link_mask is unreachable unmoveable_mask is used here

                    if ((((links_masked_out >> 16) & boosted_mask) && ((unmoveable_mask >> 8) & boosted_mask) == 0) ||                                                                                                          // up and not blocked
                        ((links_masked_out << 16) & boosted_mask && ((unmoveable_mask << 8) & boosted_mask) == 0) ||                                                                                                            // down and not blocked
                        ((((links_masked_out & 18229723555195321596ULL) >> 2) & boosted_mask) && (((unmoveable_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0) ||                                                   // left and not blocked
                        ((((links_masked_out & 4557430888798830399ULL) << 2) & boosted_mask) && (((unmoveable_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0) ||                                                     // right and not blocked
                        ((((links_masked_out & 18374403900871474942ULL) >> 9) & boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((unmoveable_mask >> 8) & boosted_mask) == 0)) || // up left
                        ((((links_masked_out & 18374403900871474942ULL) << 7) & boosted_mask) && ((((unmoveable_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((unmoveable_mask << 8) & boosted_mask) == 0)) || // down left
                        ((((links_masked_out & 9187201950435737471ULL) << 9) & boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((unmoveable_mask << 8) & boosted_mask) == 0)) ||   // down right
                        ((((links_masked_out & 9187201950435737471ULL) >> 7) & boosted_mask) && ((((unmoveable_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((unmoveable_mask >> 8) & boosted_mask) == 0)))     // up right
                        return (-32768 * (depth + 1));
                }
            }
        }
        int betabeg;
        if (depth > MIN_CACHE_DEPTH)
        {
            auto it = cache[depth].find(position);
            if (it != cache[depth].end())
            {
#ifdef CACHE_DEBUG
                cache_entry_tracker[it->second.cache_id].lookup_entries++;
#endif

                ttentry_t entry = it->second;
                if (__builtin_expect(entry.flag & 1, 0))
                {
                    if (entry.score >= beta) // if current beta <= cached beta then the beta during evaluation wont change, thus we can return the current beta
                        return beta;
                    if (entry.flag > 1) // if the cached beta is exact && it is smaller than the current beta (because of the condition above) then we can return it
                        return entry.score;
                    alpha = (alpha < entry.score) ? entry.score : alpha;
                    // cached beta is upper bound
                }
                else
                {
                    if (entry.score < beta)
                    {
                        if (entry.score <= alpha)
                            return entry.score;
                        beta = entry.score;
                    }
                }
            }
            betabeg = beta;
        }
        if (__builtin_expect(sec_link_mask & 1729382256910270464ULL, 0))
        {
            if (sec_link_mask & 576460752303423488ULL)
            {
                field_t temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(576460752303423488ULL) >> 3);
                temp.is_boost_available_sec |= ((temp.is_boosted_mask >> __builtin_ctzll(576460752303423488ULL)) & 1);
                temp.is_boosted_mask &= ~576460752303423488ULL;
                temp.is_sec_mask &= ~576460752303423488ULL;
                temp.is_link_mask &= ~576460752303423488ULL;
                ++temp.sec_link;
                BRANCH_ENTER_MIN();
                int reschild = minimax(depth, alpha, beta, true, temp, cache);
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
                field_t temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(1152921504606846976ULL) >> 3);
                temp.is_boost_available_sec |= ((temp.is_boosted_mask >> __builtin_ctzll(1152921504606846976ULL)) & 1);
                temp.is_boosted_mask &= ~1152921504606846976ULL;
                temp.is_sec_mask &= ~1152921504606846976ULL;
                temp.is_link_mask &= ~1152921504606846976ULL;
                ++temp.sec_link;
                BRANCH_ENTER_MIN();
                int reschild = minimax(depth, alpha, beta, true, temp, cache);
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
        else if (__builtin_expect((((sec_link_mask & 2310346608841064448ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 1152921504606846976ULL) == 0) || (((sec_link_mask & 290482175965396992ULL) & position.is_boosted_mask) != 0 && (unmoveable_mask & 576460752303423488ULL) == 0), 0))
        {
            field_t temp = position;

            uint64_t boosted_card = position.is_sec_mask & position.is_boosted_mask;

            temp.forward_adv_sec -= (__builtin_ctzll(boosted_card) >> 3);
            temp.is_boost_available_sec = 1;
            temp.is_boosted_mask &= ~boosted_card;
            temp.is_sec_mask &= ~boosted_card;
            temp.is_link_mask &= ~boosted_card;
            ++temp.sec_link;
            BRANCH_ENTER_MIN();
            int reschild = minimax(depth, alpha, beta, true, temp, cache);
            BRANCH_EXIT_MIN();
            beta = (reschild < beta) ? reschild : beta;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MIN();
                return beta;
            }
        }

        // uint64_t boosted_enemy_mask = (position.is_fir_mask & position.is_boosted_mask);
        // uint64_t enemy_attack_mask = position.is_fir_mask |
        //                              (position.is_fir_mask >> 8) |                           // backward
        //                              (position.is_fir_mask << 8) |                           // forward
        //                              ((position.is_fir_mask & 0x7f7f7f7f7f7f7f7fULL) << 1) | // left
        //                              ((position.is_fir_mask & 0xfefefefefefefefeULL) >> 1) | // right
        //                              ((boosted_enemy_mask & 0x7f7f7f7f7f7f7f7fULL) << 9) |
        //                              ((boosted_enemy_mask & 0x7f7f7f7f7f7f7f7fULL) >> 7) |
        //                              ((boosted_enemy_mask & 0xfefefefefefefefeULL) << 7) |
        //                              ((boosted_enemy_mask & 0xfefefefefefefefeULL) >> 9) |
        //                              (boosted_enemy_mask >> 16) |
        //                              (boosted_enemy_mask << 16) |
        //                              ((boosted_enemy_mask & 0xfcfcfcfcfcfcfcfcULL) >> 2) |
        //                              ((boosted_enemy_mask & 0xcfcfcfcfcfcfcfcfULL) << 2);

        const uint64_t firmask = (fir_link_mask | fir_virus_mask), secmask = (sec_link_mask | sec_virus_mask);

        if (position.is_boost_available_sec)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_sec = 0;

                BRANCH_ENTER_MIN();
                int reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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
            const uint64_t cur_pos_bitboard = secmask & position.is_boosted_mask;

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

        uint64_t temp = sec_virus_mask & (~position.is_boosted_mask);

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

        temp = sec_virus_mask & (~position.is_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

            uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            new_pos_bitboard = (cur_pos_bitboard >> 8);
            if (new_pos_bitboard && (new_pos_bitboard & firmask) == 0)
            {
                PERFORM_ITERATION(>>, 8, -1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
            }

            temp ^= cur_pos_bitboard;
        }

        temp = sec_link_mask & (~position.is_boosted_mask);

        while (temp)
        {
            const uint64_t cur_pos_bitboard = (1ULL << (63 - __builtin_clzll(temp))); // front -> back
            const uint64_t other = firmask | secmask;

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

        if (position.is_firewall_available_sec && depth > cur_search_depth - COSTLY_POWERUPS_LOOKAHEAD)
        {
            uint64_t temp = sec_link_mask & 16717361816799281127ULL;

            while (temp)
            {
                const int bit_pos = __builtin_clzll(temp);
                const uint64_t pos = (1ULL << (63 - bit_pos)); // front -> back

                field_t temp_field = position;

                temp_field.is_firewall_available_sec = 0;
                temp_field.firewall_sec = (63 - bit_pos);

                BRANCH_ENTER_MIN();
                int reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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
        else if (position.is_firewall_available_sec == 0)
        {
            field_t temp_field = position;

            temp_field.is_firewall_available_sec = 1;

            BRANCH_ENTER_MIN();
            int reschild = minimax(depth, alpha, beta, true, temp_field, cache);
            BRANCH_EXIT_MIN();

            beta = (reschild < beta) ? reschild : beta;
            if (beta <= alpha)
            {
                if (depth > MIN_CACHE_DEPTH)
                    TRACK_ENTRY_MIN();
                return beta;
            }
        }

        if (position.is_swap_available_sec && depth > cur_search_depth - COSTLY_POWERUPS_LOOKAHEAD)
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

                    BRANCH_ENTER_MIN();
                    int reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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

        if (position.is_boost_available_sec)
        {
            uint64_t temp = sec_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_sec = 0;

                BRANCH_ENTER_MIN();
                int reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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
            field_t temp = position;

            temp.is_boost_available_sec = 1;
            temp.is_boosted_mask &= temp.is_fir_mask;

            BRANCH_ENTER_MIN();
            int reschild = minimax(depth, alpha, beta, true, temp, cache);
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
            TRACK_ENTRY_MIN_END();
        return beta;
    }

__cache_beta:
    cache[depth][position] = {beta, 0};
    return beta;

__cache_alpha:
    cache[depth][position] = {alpha, 0};
    return alpha;
}

int cutoffdepth;

#ifdef ENGINE_DEBUG
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
bool printing;
#endif

void *maximize_worker(void *arg)
{
    worker_t *worker_data = (worker_t *)arg;

    boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[worker_data->depth];
    for (int i = 0; i < worker_data->depth; ++i)
        newcache[i].reserve(1024);

#ifdef ENGINE_DEBUG

    struct timespec start, stop;

    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    worker_data->score = minimax(worker_data->depth - 1, worker_data->alpha, worker_data->alpha + 1, false, *worker_data->field_t, newcache);
#ifdef ENGINE_DEBUG
    clock_gettime(CLOCK_MONOTONIC, &stop);
#endif

    if (worker_data->score > worker_data->alpha)
    {
#ifdef ENGINE_DEBUG
        pthread_mutex_lock(&mtx);
        if (printing)
            debug_printf("Maximize first minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->alpha, worker_data->score);
        pthread_mutex_unlock(&mtx);
        clock_gettime(CLOCK_MONOTONIC, &start);
#endif
        worker_data->score = minimax(worker_data->depth - 1, worker_data->score, worker_data->beta, false, *worker_data->field_t, newcache);
#ifdef ENGINE_DEBUG
        clock_gettime(CLOCK_MONOTONIC, &stop);
        pthread_mutex_lock(&mtx);
        if (printing)
            debug_printf("Maximize second minimax call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->alpha, worker_data->score);
        pthread_mutex_unlock(&mtx);
#endif
    }
    else
    {
#ifdef ENGINE_DEBUG
        pthread_mutex_lock(&mtx);
        if (printing)
            debug_printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->alpha, worker_data->score);
        pthread_mutex_unlock(&mtx);
#endif
    }
    return NULL;
}

void *minimize_worker(void *arg)
{
    worker_t *worker_data = (worker_t *)arg;

    boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[worker_data->depth];
    for (int i = 0; i < worker_data->depth; ++i)
        newcache[i].reserve(1024);
#ifdef ENGINE_DEBUG
    struct timespec start, stop;

    clock_gettime(CLOCK_MONOTONIC, &start);
    worker_data->score = minimax(worker_data->depth - 1, worker_data->beta - 1, worker_data->beta, true, *worker_data->field_t, newcache);
    clock_gettime(CLOCK_MONOTONIC, &stop);
#endif
    if (worker_data->score < worker_data->beta)
    {
#ifdef ENGINE_DEBUG
        pthread_mutex_lock(&mtx);
        if (printing)
            printf("Minimize first minimax improv call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->beta, worker_data->score);
        pthread_mutex_unlock(&mtx);
        clock_gettime(CLOCK_MONOTONIC, &start);
#endif
        worker_data->score = minimax(worker_data->depth - 1, worker_data->alpha, worker_data->score, true, *worker_data->field_t, newcache);
#ifdef ENGINE_DEBUG
        clock_gettime(CLOCK_MONOTONIC, &stop);
        pthread_mutex_lock(&mtx);
        if (printing)
            printf("Minimize second minimax call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->beta, worker_data->score);
        pthread_mutex_unlock(&mtx);
#endif
    }
    else
    {
#ifdef ENGINE_DEBUG
        pthread_mutex_lock(&mtx);
        if (printing)
            printf("Minimize first minimax no-improv call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->beta, worker_data->score);
        pthread_mutex_unlock(&mtx);
#endif
    }
    return NULL;
}

int minimax_scout(const int depth, int alpha, int beta, const bool player, field_t &position)
{
    if (depth < cutoffdepth)
    {
        boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[depth];
        for (int i = 0; i < depth; ++i)
            newcache[i].reserve(1024);
        return minimax(depth, alpha, beta, player, position, newcache);
    }
    if (player)
    {
        possible_moves_t all_moves = possiblemoves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].fir_link > 3)
                return (32768 * depth);

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];
        alpha = minimax_scout(depth - 1, alpha, beta, false, all_moves.moves[0]);
#ifdef ENGINE_DEBUG
        printing = false;
#endif
        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];

            pthread_create(&threads[i], NULL, maximize_worker, (void *)&args[i]);
        }

        for (int i = 1; i < all_moves.moves_count; ++i)
            pthread_join(threads[i], NULL);

        for (int i = 1; i < all_moves.moves_count; ++i)
        {
            if (args[i].score > alpha)
            {
                alpha = args[i].score;
            }
        }
        return alpha;
    }
    else
    {
        possible_moves_t all_moves = possiblemoves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].sec_link > 3)
                return (-32768 * depth);

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];
        beta = minimax_scout(depth - 1, alpha, beta, true, all_moves.moves[0]);
#ifdef ENGINE_DEBUG
        printing = false;
#endif
        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];

            pthread_create(&threads[i], NULL, minimize_worker, (void *)&args[i]);
        }

        for (int i = 1; i < all_moves.moves_count; ++i)
            pthread_join(threads[i], NULL);

        for (int i = 1; i < all_moves.moves_count; ++i)
        {
            if (beta > args[i].score)
            {
                beta = args[i].score;
            }
        }
        return beta;
    }
}

minimax_main_result_t minimax_main(const int depth, int alpha, int beta, const bool player, field_t &position)
{
    cur_search_depth = depth;

    cutoffdepth = depth - 5;
    if (player)
    {
        possible_moves_t all_moves = possiblemoves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * depth)};

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];

        field_t bestfield = all_moves.moves[0];
        alpha = minimax_scout(depth - 1, alpha, beta, false, all_moves.moves[0]);

#ifdef ENGINE_DEBUG
        printing = true;
#endif

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];

            pthread_create(&threads[i], NULL, maximize_worker, (void *)&args[i]);
        }

        for (int i = 1; i < all_moves.moves_count; ++i)
            pthread_join(threads[i], NULL);

        for (int i = 1; i < all_moves.moves_count; ++i)
        {
            if (args[i].score > alpha)
            {
                alpha = args[i].score;
                bestfield = all_moves.moves[i];
            }
        }
        return (minimax_main_result_t){.best_field = bestfield, .evaluation = alpha};
    }
    else
    {
        possible_moves_t all_moves = possiblemoves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-32768 * depth)};

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];

        field_t bestfield = all_moves.moves[0];
        beta = minimax_scout(depth - 1, alpha, beta, true, all_moves.moves[0]);

#ifdef ENGINE_DEBUG
        printing = true;
#endif

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];

            pthread_create(&threads[i], NULL, minimize_worker, (void *)&args[i]);
        }

        for (int i = 1; i < all_moves.moves_count; ++i)
            pthread_join(threads[i], NULL);

        for (int i = 1; i < all_moves.moves_count; ++i)
        {
            if (beta > args[i].score)
            {
                beta = args[i].score;
                bestfield = all_moves.moves[i];
            }
        }
        return (minimax_main_result_t){.best_field = bestfield, .evaluation = beta};
    }
}

int64_t max_time = -1;

#define ENGINE_DEBUG

minimax_main_result_t minimax_single_main(const int depth, int alpha, int beta, const bool player, field_t &position)
{
    max_time = -1;
    cur_search_depth = depth;
#ifdef ENGINE_DEBUG
    struct timespec start, stop;
#endif
    if (player)
    {
        possible_moves_t all_moves = possiblemoves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * depth)};

        field_t best_field = all_moves.moves[0];
        boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[depth];
        for (int i = 0; i < depth; ++i)
            newcache[i].reserve(1024);

        bool is_first_move = true;

        for (int i = 0; i < all_moves.moves_count; ++i)
        {
#ifdef ENGINE_DEBUG
            clock_gettime(CLOCK_MONOTONIC, &start);
#endif
            int childres;
            if (is_first_move)
            {
                childres = minimax(depth - 1, alpha, beta, false, all_moves.moves[i], newcache);
#ifdef ENGINE_DEBUG
                clock_gettime(CLOCK_MONOTONIC, &stop);
                printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
#endif
            }
            else
            {
                childres = minimax(depth - 1, alpha, alpha + 1, false, all_moves.moves[i], newcache);
#ifdef ENGINE_DEBUG
                clock_gettime(CLOCK_MONOTONIC, &stop);
#endif
                if (childres > alpha)
                {
                    childres = minimax(depth - 1, alpha, beta, false, all_moves.moves[i], newcache);
#ifdef ENGINE_DEBUG
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    printf("Maximize second minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
#endif
                }
                else
                {
#ifdef ENGINE_DEBUG
                    printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
#endif
                }
            }

            int64_t cur_time = stop.tv_sec * 1000000000ll + stop.tv_nsec - start.tv_sec * 1000000000ll - start.tv_nsec;

            max_time = (cur_time > max_time) ? cur_time : max_time;
            best_field = (childres > alpha) ? all_moves.moves[i] : best_field;
            alpha = (childres > alpha) ? childres : alpha;
            is_first_move = false;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = alpha};
    }
    else
    {
        possible_moves_t all_moves = possiblemoves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-32768 * depth)};

        field_t best_field = all_moves.moves[0];
        boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[depth];
        for (int i = 0; i < depth; ++i)
            newcache[i].reserve(1024);

        bool is_first_move = true;

        for (int i = 0; i < all_moves.moves_count; ++i)
        {
#ifdef ENGINE_DEBUG
            clock_gettime(CLOCK_MONOTONIC, &start);
#endif
            int childres;

            if (is_first_move)
            {
                childres = minimax(depth - 1, alpha, beta, true, all_moves.moves[i], newcache);
#ifdef ENGINE_DEBUG
                clock_gettime(CLOCK_MONOTONIC, &stop);
                printf("Minimize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
#endif
            }
            else
            {
                childres = minimax(depth - 1, beta - 1, beta, true, all_moves.moves[i], newcache);
#ifdef ENGINE_DEBUG
                clock_gettime(CLOCK_MONOTONIC, &stop);
#endif
                if (childres < beta)
                {
#ifdef ENGINE_DEBUG
                    printf("Minimize first minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
                    childres = minimax(depth - 1, alpha, beta, true, all_moves.moves[i], newcache);
#ifdef ENGINE_DEBUG
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    printf("Minimize second minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
#endif
                }
                else
                {
#ifdef ENGINE_DEBUG
                    printf("Minimize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
#endif
                }
            }

            best_field = (childres < beta) ? all_moves.moves[i] : best_field;
            beta = (childres < beta) ? childres : beta;
            is_first_move = false;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = beta};
    }
}

minimax_main_result_t minimax_iteration_main(const int depth, int alpha, int beta, const bool player, field_t &position)
{
    cur_search_depth = depth;
    assert(depth >= 4 && depth % 2 == 0 && "Depth must be at least 4 and even (divisible by 2) for iterative deepening");

    struct timespec start, stop;
    minimax_main_result_t best_result;

    if (player)
    {
        possible_moves_t all_moves = possiblemoves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * depth)};

        field_t best_field = all_moves.moves[0];
        int prev_alpha = alpha;

        std::vector<std::pair<int, int>> move_scores(all_moves.moves_count);
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = {i, MIN};

        for (int current_depth = 2; current_depth <= depth; current_depth += 2)
        {
            clock_gettime(CLOCK_MONOTONIC, &start);

            boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[current_depth];
            for (int i = 0; i < current_depth; ++i)
                newcache[i].reserve(1024);

            if (current_depth > 2)
            {
                std::stable_sort(move_scores.begin(), move_scores.end(),
                                 [](const auto &a, const auto &b)
                                 {
                                     if (a.second != b.second)
                                         return a.second > b.second;
                                     return a.first < b.first;
                                 });
            }

            int iteration_alpha = prev_alpha - 1;
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                int move_idx = move_scores[i].first;

                int childres = minimax(current_depth - 1, iteration_alpha, iteration_alpha + 1, false, all_moves.moves[move_idx], newcache);
                if (childres > iteration_alpha)
                    childres = minimax(current_depth - 1, iteration_alpha, beta, false, all_moves.moves[move_idx], newcache);

                move_scores[i].second = childres;

                if (childres > iteration_alpha)
                {
                    best_field = all_moves.moves[move_idx];
                    iteration_alpha = childres;
                }
            }

            if (iteration_alpha == prev_alpha - 1)
            {
                debug_printf("Guess failed\n");
                iteration_alpha = MIN;
                for (int i = 0; i < all_moves.moves_count; ++i)
                {
                    int move_idx = move_scores[i].first;
                    int childres;
                    if (i == 0)
                    {
                        childres = minimax(current_depth - 1, iteration_alpha, beta, false, all_moves.moves[move_idx], newcache);
                    }
                    else
                    {
                        childres = minimax(current_depth - 1, iteration_alpha, iteration_alpha + 1, false, all_moves.moves[move_idx], newcache);
                        if (childres > iteration_alpha)
                            childres = minimax(current_depth - 1, iteration_alpha, beta, false, all_moves.moves[move_idx], newcache);
                    }

                    move_scores[i].second = childres;

                    if (childres > iteration_alpha)
                    {
                        best_field = all_moves.moves[move_idx];
                        iteration_alpha = childres;
                    }
                }
            }

            debug_printf("Search order: ");
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                debug_printf("%d, ", move_scores[i].first);
            }
            debug_printf("\n");

            clock_gettime(CLOCK_MONOTONIC, &stop);

            int64_t cur_time = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);

            debug_printf("Depth %d completed in %ld ms, evaluation: %d\n",current_depth,cur_time,iteration_alpha);

            if (current_depth == depth)
                max_time = cur_time;

            prev_alpha = iteration_alpha;

            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_alpha};
        }

        return best_result;
    }
    else
    {
        possible_moves_t all_moves = possiblemoves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-32768 * depth)};

        field_t best_field = all_moves.moves[0];
        int prev_beta = beta;

        std::vector<std::pair<int, int>> move_scores(all_moves.moves_count);
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = {i, MAX}; 

        for (int current_depth = 2; current_depth <= depth; current_depth += 2)
        {
            clock_gettime(CLOCK_MONOTONIC, &start);

            boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[current_depth];
            for (int i = 0; i < current_depth; ++i)
                newcache[i].reserve(1024);

            if (current_depth > 2)
            {
                std::stable_sort(move_scores.begin(), move_scores.end(),
                                 [](const auto &a, const auto &b)
                                 {
                                     if (a.second != b.second)
                                         return a.second < b.second;
                                     return a.first < b.first;
                                 });
            }

            int iteration_beta = prev_beta + 1;
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                int move_idx = move_scores[i].first;

                int childres = minimax(current_depth - 1, iteration_beta - 1, iteration_beta, true, all_moves.moves[move_idx], newcache);
                if (childres < iteration_beta)
                    childres = minimax(current_depth - 1, alpha, iteration_beta, true, all_moves.moves[move_idx], newcache);

                move_scores[i].second = childres;

                if (childres < iteration_beta)
                {
                    best_field = all_moves.moves[move_idx];
                    iteration_beta = childres;
                }
            }

            if (iteration_beta == prev_beta + 1)
            {
                debug_printf("Guess failed\n");

                iteration_beta = MAX;
                for (int i = 0; i < all_moves.moves_count; ++i)
                {
                    int move_idx = move_scores[i].first;
                    int childres;

                    if (i == 0)
                    {
                        childres = minimax(current_depth - 1, alpha, iteration_beta, true, all_moves.moves[move_idx], newcache);
                    }
                    else
                    {
                        childres = minimax(current_depth - 1, iteration_beta - 1, iteration_beta, true, all_moves.moves[move_idx], newcache);
                        if (childres < iteration_beta)
                            childres = minimax(current_depth - 1, alpha, iteration_beta, true, all_moves.moves[move_idx], newcache);
                    }

                    move_scores[i].second = childres;

                    if (childres < iteration_beta)
                    {
                        best_field = all_moves.moves[move_idx];
                        iteration_beta = childres;
                    }
                }
            }

            debug_printf("Search order: ");
            for (int i = 0; i < all_moves.moves_count; ++i)
            {
                debug_printf("%d, ", move_scores[i].first);
            }
            debug_printf("\n");

            clock_gettime(CLOCK_MONOTONIC, &stop);

            int64_t cur_time = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);
            debug_printf("Depth %d completed in %ld ms, evaluation: %d\n", current_depth, cur_time, iteration_beta);

            if (current_depth == depth)
                max_time = cur_time;

            prev_beta = iteration_beta;
            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_beta};
        }

        return best_result;
    }
}

GLFWwindow *window;

field_t pos;

void render_game_field(field_t &position, bool cover_enemy_cards, uint64_t enemy_reveal_mask, uint64_t ally_reveal_mask, uint8_t taken_first_links, uint8_t taken_first_viruses, uint8_t taken_second_links, uint8_t taken_second_viruses)
{
    uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
    uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
    uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
    uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;

    render_tex(&game_background_vis, 0, 0, 255, 255, 255, 255, 0);

    for (int i = 0; i < position.fir_virus; ++i)
    {
        if (taken_first_viruses & (1 << i)) // friendly
            render_tex(&virus_enemy, 54 * i, 0, 255, 255, 255, 255, 0);
        else
            render_tex(&virus_ally, 54 * i, 0, 255, 255, 255, 255, 0);
    }

    for (int i = 0; i < position.fir_link; ++i)
    {
        if (taken_first_links & (1 << i)) // friendly
            render_tex(&link_enemy, 54 * (i + 4), 0, 255, 255, 255, 255, 0);
        else
            render_tex(&link_ally, 54 * (i + 4), 0, 255, 255, 255, 255, 0);
    }

    for (int i = 0; i < position.sec_virus; ++i)
    {
        if (taken_second_viruses & (1 << i)) // friendly
            render_tex(&virus_ally, 54 * i, 54 * 11, 255, 255, 255, 255, 0);
        else
            render_tex(&virus_enemy, 54 * i, 54 * 11, 255, 255, 255, 255, 0);
    }

    for (int i = 0; i < position.sec_link; ++i)
    {
        if (taken_second_links & (1 << i)) // friendly
            render_tex(&link_ally, 54 * (i + 4), 54 * 11, 255, 255, 255, 255, 0);
        else
            render_tex(&link_enemy, 54 * (i + 4), 54 * 11, 255, 255, 255, 255, 0);
    }

    if (position.is_boost_available_fir)
        render_tex(&boost_enemy, 54 * 6, 54 * 1, 255, 255, 255, 255, 0);

    if (position.is_boost_available_sec)
        render_tex(&boost_ally, 54 * 6, 54 * 10, 255, 255, 255, 255, 0);

    if (position.is_swap_available_fir)
        render_tex(&not_found_enemy, 54 * 2, 54 * 1, 255, 255, 255, 255, 0);

    if (position.is_swap_available_sec)
        render_tex(&not_found_ally, 54 * 2, 54 * 10, 255, 255, 255, 255, 0);

    if (position.is_checker_available_fir)
        render_tex(&virus_check_enemy, 54 * 1, 54 * 1, 255, 255, 255, 255, 0);

    if (position.is_checker_available_sec)
        render_tex(&virus_check_ally, 54 * 1, 54 * 10, 255, 255, 255, 255, 0);

    uint64_t fir_boosted = position.is_fir_mask & position.is_boosted_mask;

    if (fir_boosted)
    {
        const int pos_id = __builtin_ctzll(fir_boosted);

        if (cover_enemy_cards)
        {
            if ((fir_boosted & enemy_reveal_mask) == 0)
            {
                render_tex(&unknown_boosted, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
            else
            {
                if (fir_boosted & fir_virus_mask)
                {
                    render_tex(&virus_enemy_boosted, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
                }
                else
                {
                    render_tex(&link_enemy_boosted, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
                }
            }
        }
        else
        {
            if (fir_boosted & fir_virus_mask)
            {
                render_tex(&virus_enemy_boosted, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
            else
            {
                render_tex(&link_enemy_boosted, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
        }
    }

    uint64_t sec_boosted = position.is_sec_mask & position.is_boosted_mask;

    if (sec_boosted)
    {
        const int pos_id = __builtin_ctzll(sec_boosted);

        if ((sec_boosted & ally_reveal_mask) == 0)
        {
            if (sec_boosted & sec_virus_mask)
            {
                render_tex(&virus_ally_boosted, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
            else
            {
                render_tex(&link_ally_boosted, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
        }
        else
        {
            if (sec_boosted & sec_virus_mask)
            {
                render_tex(&virus_ally_boosted_revealed, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
            else
            {
                render_tex(&link_ally_boosted_revealed, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
        }
    }

    fir_link_mask &= ~(fir_boosted);
    fir_virus_mask &= ~(fir_boosted);
    sec_link_mask &= ~(sec_boosted);
    sec_virus_mask &= ~(sec_boosted);

    while (fir_link_mask)
    {
        const int pos_id = __builtin_ctzll(fir_link_mask);
        const uint64_t cur_pos_bitboard = (1ULL << pos_id);

        if (cover_enemy_cards)
        {
            if ((cur_pos_bitboard & enemy_reveal_mask) == 0)
            {
                render_tex(&unknown_default, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
            else
            {
                render_tex(&link_enemy, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
        }
        else
        {
            render_tex(&link_enemy, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
        }

        fir_link_mask ^= cur_pos_bitboard;
    }

    while (fir_virus_mask)
    {
        const int pos_id = __builtin_ctzll(fir_virus_mask);
        const uint64_t cur_pos_bitboard = (1ULL << pos_id);

        if (cover_enemy_cards)
        {
            if ((cur_pos_bitboard & enemy_reveal_mask) == 0)
            {
                render_tex(&unknown_default, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
            else
            {
                render_tex(&virus_enemy, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
            }
        }
        else
        {
            render_tex(&virus_enemy, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
        }

        fir_virus_mask ^= cur_pos_bitboard;
    }

    while (sec_link_mask)
    {
        const int pos_id = __builtin_ctzll(sec_link_mask);
        const uint64_t cur_pos_bitboard = (1ULL << pos_id);

        if ((cur_pos_bitboard & ally_reveal_mask) == 0)
        {
            render_tex(&link_ally, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
        }
        else
        {
            render_tex(&link_ally_revealed, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
        }

        sec_link_mask ^= cur_pos_bitboard;
    }

    while (sec_virus_mask)
    {
        const int pos_id = __builtin_ctzll(sec_virus_mask);
        const uint64_t cur_pos_bitboard = (1ULL << pos_id);

        if ((cur_pos_bitboard & ally_reveal_mask) == 0)
        {
            render_tex(&virus_ally, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
        }
        else
        {
            render_tex(&virus_ally_revealed, 54 * (7 - (pos_id & 7)), 54 * (9 - (pos_id >> 3)), 255, 255, 255, 255, 0);
        }

        sec_virus_mask ^= cur_pos_bitboard;
    }

    // render with enemy color for clarity

    if (position.is_firewall_available_fir)
    {
        render_tex(&firewall_enemy, 54 * 5, 54 * 1, 255, 255, 255, 255, 0);
    }
    else
    {
        render_tex(&firewall_vis_ally, (7 - (int)(position.firewall_fir & 7)) * 54, (9 - (int)(position.firewall_fir >> 3)) * 54, 255, 255, 255, 255, 0);
    }

    if (position.is_firewall_available_sec)
    {
        render_tex(&firewall_ally, 54 * 5, 54 * 10, 255, 255, 255, 255, 0);
    }
    else
    {
        render_tex(&firewall_vis_enemy, (7 - (int)(position.firewall_sec & 7)) * 54, (9 - (int)(position.firewall_sec >> 3)) * 54, 255, 255, 255, 255, 0);
    }
}

void render_button(struct button_t *self)
{
    if (self->visible == false || self->a == 0)
        return;

    texture_t *corner = self->button_3_slice_text.corner;

    int corner_width = (int)corner->width;
    int corner_height = (int)corner->height;
    int side_length = self->padding_top + self->padding_bottom + self->height;
    int top_side_length = self->padding_left + self->padding_right + self->width;

    render_tex(corner, (float)self->x, (float)self->y, self->r, self->g, self->b, self->a, ROTATE_0);                                                                      // top left
    render_tex(corner, (float)(self->x + top_side_length - corner_width), (float)self->y, self->r, self->g, self->b, self->a, ROTATE_90);                                  // top right
    render_tex(corner, (float)self->x, (float)(self->y + side_length - corner_height), self->r, self->g, self->b, self->a, ROTATE_180);                                    // bottom left
    render_tex(corner, (float)(self->x + top_side_length - corner_width), (float)(self->y + side_length - corner_height), self->r, self->g, self->b, self->a, ROTATE_270); // bottom right

    texture_t *side = self->button_3_slice_text.side;

    int side_height = (int)side->height;

    render_tex_custom_scale(side, (float)(self->x + corner_width), (float)self->y, (float)(top_side_length - corner_width * 2), (float)side_height, self->r, self->g, self->b, self->a, ROTATE_0);                                  // top
    render_tex_custom_scale(side, (float)self->x, (float)(self->y + corner_height), (float)side_height, (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_270);                                  // left
    render_tex_custom_scale(side, (float)(self->x + top_side_length - side_height), (float)(self->y + corner_height), (float)side_height, (float)(side_length - corner_height * 2), self->r, self->g, self->b, self->a, ROTATE_90); // right
    render_tex_custom_scale(side, (float)(self->x + corner_width), (float)(self->y + side_length - side_height), (float)(top_side_length - corner_width * 2), (float)side_height, self->r, self->g, self->b, self->a, ROTATE_180);  // bottom

    int vis_x = self->x + self->padding_left;
    int vis_y = self->y + self->padding_top;

    if (self->is_interacting || current_time - self->interation_time < self->fade_time)
    {
        uint8_t value = perform_texture_alpha_operation_single(current_time, self->interation_time, (int64_t)self->fade_time, 128, self->is_interacting);

        texture_t *center = self->button_3_slice_text.center;
        render_tex_custom_scale(center, (float)(self->x + corner_width), (float)(self->y + corner_height), (float)(top_side_length - corner_width * 2), (float)(side_length - corner_height * 2), 0, ((uint32_t)self->g * (uint32_t)value) / 255, 0, self->a, ROTATE_0); // center
    }

    render_text_custom_max_height((const font_t *)self->button_3_slice_text.font_ptr, self->button_3_slice_text.font_size, (float)self->height, self->button_3_slice_text.text_ptr, (float)vis_x, (float)vis_y, self->button_3_slice_text.r, self->button_3_slice_text.g, self->button_3_slice_text.b, ((uint32_t)self->button_3_slice_text.a * (uint32_t)self->a) / 255);
}

void ai_move();

typedef enum
{
    GAME_STATE_MENU = 0,                 // main menu
    GAME_STATE_PLACE_CARDS,              // placing cards
    GAME_STATE_PLAYER_IDLE,              // wait for the player input
    GAME_STATE_PLAYER_SWAP_P1,           // player clicked on the swap powerup
    GAME_STATE_PLAYER_SWAP_P2,           // player selected the first card
    GAME_STATE_PLAYER_SWAP_P3,           // player selected the second card
    GAME_STATE_PLAYER_SWAP_ANIM,         // swap animation
    GAME_STATE_PLAYER_FIREWALL_P1,       // player clicked on the firewall powerup
    GAME_STATE_PLAYER_FIREWALL_ANIM,     // firewall animation
    GAME_STATE_PLAYER_UNFIREWALL_P1,     // player clicked on the firewall powerup while firewalled
    GAME_STATE_PLAYER_UNFIREWALL_ANIM,   // un-firewall animation
    GAME_STATE_PLAYER_BOOST_P1,          // player clicked on the boost powerup
    GAME_STATE_PLAYER_BOOST_ANIM,        // card boosing animation
    GAME_STATE_PLAYER_UNBOOST_P1,        // player clicked on the boost powerup while boosted
    GAME_STATE_PLAYER_UNBOOST_ANIM,      // card un-boosing animation
    GAME_STATE_PLAYER_MOVE_CARD,         // player clicked on the card
    GAME_STATE_PLAYER_MOVE_CARD_ANIM,    // card move animation
    GAME_STATE_PLAYER_CAPTURE_CARD_ANIM, // capture card animation
    GAME_STATE_PLAYER_DEPOSIT_CARD_ANIM, // depo card animation
    GAME_STATE_PLAYER_CHECKER_P1,        // player clicked on the checker powerup
    GAME_STATE_PLAYER_LOSE,              // player lose anim
    GAME_STATE_PLAYER_LOST,              // player lose screen
    GAME_STATE_PLAYER_WIN,               // player wins
    GAME_STATE_PLAYER_WON,               // player wins
    GAME_STATE_AI_IDLE,
    GAME_STATE_AI_MOVE,
    GAME_STATE_AI_CAPTURE,
    GAME_STATE_AI_SWAP,
    GAME_STATE_AI_FIREWALL,
    GAME_STATE_AI_UNFIREWALL,
    GAME_STATE_AI_DEPOSIT,
    GAME_STATE_AI_BOOST,
    GAME_STATE_AI_UNBOOST,
} game_state;

pthread_mutex_t ai_move_mtx = PTHREAD_MUTEX_INITIALIZER;

bool player_begins = true;

game_state state = GAME_STATE_MENU;
button_t *player_starts_switch;
button_t *ai_starts_switch;
button_t *ai_increase_diff;
button_t *ai_decrease_diff;
button_t *start_game;
button_t *controls[12];
int decals_size = 0;
button_t *decals[80];
button_t *surface;
const texture_t *texture_ptr;
const texture_t *sec_texture_ptr;

field_t old_state;

bool is_ai_turn;
int64_t last_time_appear = 0;

uint64_t enemy_reveal_mask = 0;
uint64_t ally_reveal_mask = 0;
uint8_t taken_first_links = 0;
uint8_t taken_first_viruses = 0;
uint8_t taken_second_links = 0;
uint8_t taken_second_viruses = 0;
uint8_t player_field_id = 0;
uint8_t player_field_place_id = 0;

uint64_t interacting_card = 0;
uint64_t moved_card = 0;

const texture_t *card_to_texture(uint64_t mask, field_t &field, bool player)
{
    if (player)
    {
        if ((enemy_reveal_mask & mask) || (HIDE_ENEMY_CARDS == false))
        {
            if (field.is_link_mask & mask)
            {
                if (field.is_boosted_mask & mask)
                {
                    return &link_enemy_boosted;
                }
                else
                {
                    return &link_enemy;
                }
            }
            else
            {
                if (field.is_boosted_mask & mask)
                {
                    return &virus_enemy_boosted;
                }
                else
                {
                    return &virus_enemy;
                }
            }
        }
        else
        {
            if (field.is_boosted_mask & mask)
            {
                return &unknown_boosted;
            }
            else
            {
                return &unknown_default;
            }
        }
    }
    else
    {
        if (ally_reveal_mask & mask)
        {
            if (field.is_link_mask & mask)
            {
                if (field.is_boosted_mask & mask)
                {
                    return &link_ally_boosted_revealed;
                }
                else
                {
                    return &link_ally_revealed;
                }
            }
            else
            {
                if (field.is_boosted_mask & mask)
                {
                    return &virus_ally_boosted_revealed;
                }
                else
                {
                    return &virus_ally_revealed;
                }
            }
        }
        else
        {
            if (field.is_link_mask & mask)
            {
                if (field.is_boosted_mask & mask)
                {
                    return &link_ally_boosted;
                }
                else
                {
                    return &link_ally;
                }
            }
            else
            {
                if (field.is_boosted_mask & mask)
                {
                    return &virus_ally_boosted;
                }
                else
                {
                    return &virus_ally;
                }
            }
        }
    }
}

void switch_player(struct button_t *self)
{
    ai_starts_switch->visible = !ai_starts_switch->visible;
    player_starts_switch->visible = !player_starts_switch->visible;
    player_begins = !player_begins;
}

void decrease_diff(struct button_t *self)
{
    --ai_level;
    if (ai_level < 6)
        ai_level = 6;
}

void increase_diff(struct button_t *self)
{
    ++ai_level;
    if (ai_level > 16)
        ai_level = 16;
}

void add_card_controls();

void decal_move_callback(struct button_t *self)
{
    debug_printf("called decal_move_callback\n");
    const uint64_t move_dest = 1ULL << ((7 - (self->x / 54)) + 8 * (9 - (self->y / 54)));

    if (interacting_card == ally_reveal_mask)
    {
        ally_reveal_mask ^= (interacting_card | move_dest);
    }

    old_state = pos;

    moved_card = move_dest;
    const uint64_t moved_card_pos = interacting_card;

    texture_ptr = card_to_texture(moved_card_pos, pos, false);
    sec_texture_ptr = card_to_texture(move_dest, pos, true);

    old_state.is_sec_mask &= ~moved_card_pos;
    old_state.is_link_mask &= ~moved_card_pos;
    old_state.is_boosted_mask &= ~moved_card_pos;

    state = GAME_STATE_PLAYER_MOVE_CARD_ANIM;

    if (pos.is_fir_mask & move_dest)
    {
        old_state.is_fir_mask &= ~move_dest;
        old_state.is_link_mask &= ~move_dest;
        old_state.is_boosted_mask &= ~move_dest;

        enemy_reveal_mask &= ~move_dest;

        state = GAME_STATE_PLAYER_CAPTURE_CARD_ANIM;

        if (pos.is_link_mask & move_dest)
        {
            ++pos.sec_link;
            pos.is_link_mask &= ~move_dest;
            pos.is_fir_mask &= ~move_dest;
        }
        else
        {
            ++pos.sec_virus;
            pos.is_fir_mask &= ~move_dest;
        }

        pos.forward_adv_fir -= 7 - (__builtin_ctzll(move_dest) >> 3);

        if (pos.is_boosted_mask & move_dest)
        {
            pos.is_boost_available_fir = 1;
            pos.is_boosted_mask &= ~move_dest;
        }
    }

    if (pos.is_boosted_mask & interacting_card)
        pos.is_boosted_mask ^= (interacting_card | move_dest);
    if (pos.is_link_mask & interacting_card)
        pos.is_link_mask ^= (interacting_card | move_dest);
    pos.is_sec_mask ^= (interacting_card | move_dest);

    pos.forward_adv_sec -= __builtin_ctzll(interacting_card) >> 3;
    pos.forward_adv_sec += __builtin_ctzll(move_dest) >> 3;

    for (int i = 0; i < decals_size; ++i)
        decals[i]->on_click = NULL;
    decals_size = 0;

    surface->on_click = NULL;

    last_time_appear = current_time;
}

void decal_depo_callback(struct button_t *self)
{
    debug_printf("called decal_depo_callback\n");

    old_state = pos;

    state = GAME_STATE_PLAYER_DEPOSIT_CARD_ANIM;

    texture_ptr = card_to_texture(interacting_card, pos, false);
    if (pos.is_link_mask & interacting_card)
    {
        sec_texture_ptr = &link_ally;
    }
    else
    {
        sec_texture_ptr = &virus_ally;
    }

    if (interacting_card == ally_reveal_mask)
    {
        ally_reveal_mask = 0;
    }

    if (pos.is_link_mask & interacting_card)
    {
        taken_second_links |= (1 << pos.sec_link);
        ++pos.sec_link;
    }
    else // deposit a virus huh
    {
        taken_second_viruses |= (1 << pos.sec_virus);
        ++pos.sec_virus;
    }

    if (pos.is_boosted_mask & interacting_card)
    {
        pos.is_boost_available_sec = 1;
    }

    if (pos.is_boosted_mask & interacting_card)
        pos.is_boosted_mask ^= interacting_card;
    if (pos.is_link_mask & interacting_card)
        pos.is_link_mask ^= interacting_card;
    pos.is_sec_mask ^= interacting_card;

    pos.forward_adv_sec -= __builtin_ctzll(interacting_card) >> 3;

    for (int i = 0; i < decals_size; ++i)
        decals[i]->on_click = NULL;
    decals_size = 0;

    surface->on_click = NULL;

    old_state.is_sec_mask &= ~interacting_card;
    old_state.is_link_mask &= ~interacting_card;
    old_state.is_boosted_mask &= ~interacting_card;

    last_time_appear = current_time;
}

void put_new_decal(int x, int y)
{
    const uint64_t piece_mask = 1ULL << (x + (y << 3));
    if (pos.is_fir_mask & piece_mask)
        decals[decals_size]->button_image.texture_ptr = (texture_t *)&move_capture_vis;
    else
        decals[decals_size]->button_image.texture_ptr = (texture_t *)&move_default_vis;
    decals[decals_size]->x = (7 - x) * 54;
    decals[decals_size]->y = (9 - y) * 54;
    decals[decals_size]->width = 54;
    decals[decals_size]->height = 54;
    decals[decals_size]->on_click = decal_move_callback;

    ++decals_size;
}

void undo_move(struct button_t *self)
{
    debug_printf("called undo_move\n");

    state = GAME_STATE_PLAYER_IDLE;

    for (int i = 0; i < decals_size; ++i)
        decals[i]->on_click = NULL;
    decals_size = 0;

    surface->on_click = NULL;
    add_card_controls();
}

void card_move_p1(struct button_t *self)
{
    debug_printf("called card_move_p1\n");

    state = GAME_STATE_PLAYER_MOVE_CARD;

    last_time_appear = current_time;

    const uint64_t piece_mask = 1ULL << ((7 - (self->x / 54)) + 8 * (9 - (self->y / 54)));
    const int piece_pos = __builtin_ctzll(piece_mask);
    const int piece_x = piece_pos & 7;
    const int piece_y = piece_pos >> 3;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;
    surface->on_click = undo_move;

    interacting_card = piece_mask;

    debug_printf("clicked on card at x=%d y=%d\n", piece_x, piece_y);

    const uint64_t enemy_firewall_mask = ((pos.is_firewall_available_fir) ? 0 : (1ULL << pos.firewall_fir));
    const uint64_t all_cards_mask = pos.is_sec_mask | pos.is_fir_mask | enemy_firewall_mask;
    const uint64_t unmoveable_mask = pos.is_sec_mask | enemy_firewall_mask;

    if (pos.is_boosted_mask & piece_mask)
    {
        if (piece_x + 2 <= 7 && (unmoveable_mask & (piece_mask << 2)) == 0 && (all_cards_mask & (piece_mask << 1)) == 0)
        {
            put_new_decal(piece_x + 2, piece_y);
        }
        if (piece_x - 2 >= 0 && (unmoveable_mask & (piece_mask >> 2)) == 0 && (all_cards_mask & (piece_mask >> 1)) == 0)
        {
            put_new_decal(piece_x - 2, piece_y);
        }
        if (piece_y + 2 <= 7 && (unmoveable_mask & (piece_mask << 16)) == 0 && (all_cards_mask & (piece_mask << 8)) == 0)
        {
            put_new_decal(piece_x, piece_y + 2);
        }
        if (piece_y - 2 >= 0 && (unmoveable_mask & (piece_mask >> 16)) == 0 && (all_cards_mask & (piece_mask >> 8)) == 0)
        {
            put_new_decal(piece_x, piece_y - 2);
        }
        if (piece_x + 1 <= 7 && piece_y + 1 <= 7 && (unmoveable_mask & (piece_mask << 9)) == 0 && ((all_cards_mask & (piece_mask << 1)) == 0 || (all_cards_mask & (piece_mask << 8)) == 0))
        {
            put_new_decal(piece_x + 1, piece_y + 1);
        }
        if (piece_x - 1 >= 0 && piece_y + 1 <= 7 && (unmoveable_mask & (piece_mask << 7)) == 0 && ((all_cards_mask & (piece_mask >> 1)) == 0 || (all_cards_mask & (piece_mask << 8)) == 0))
        {
            put_new_decal(piece_x - 1, piece_y + 1);
        }
        if (piece_x + 1 <= 7 && piece_y - 1 >= 0 && (unmoveable_mask & (piece_mask >> 7)) == 0 && ((all_cards_mask & (piece_mask << 1)) == 0 || (all_cards_mask & (piece_mask >> 8)) == 0))
        {
            put_new_decal(piece_x + 1, piece_y - 1);
        }
        if (piece_x - 1 >= 0 && piece_y - 1 >= 0 && (unmoveable_mask & (piece_mask >> 9)) == 0 && ((all_cards_mask & (piece_mask >> 1)) == 0 || (all_cards_mask & (piece_mask >> 8)) == 0))
        {
            put_new_decal(piece_x - 1, piece_y - 1);
        }

        // depo I genuinely hate this code but wrapping it is even worse, we have to make sure we dont place two buttons on either of depo spots
        if (((piece_x == 2 && piece_y == 7) || (piece_x == 3 && piece_y == 6)) && (all_cards_mask & 576460752303423488ULL) == 0)
        {
            decals[decals_size]->button_image.texture_ptr = (texture_t *)&move_depo_vis;
            decals[decals_size]->x = (7 - 3) * 54;
            decals[decals_size]->y = (9 - 8) * 54;
            decals[decals_size]->width = 54;
            decals[decals_size]->height = 54;
            decals[decals_size]->on_click = decal_depo_callback;

            ++decals_size;
        }

        if (((piece_x == 5 && piece_y == 7) || (piece_x == 4 && piece_y == 6)) && (all_cards_mask & 1152921504606846976ULL) == 0)
        {
            decals[decals_size]->button_image.texture_ptr = (texture_t *)&move_depo_vis;
            decals[decals_size]->x = (7 - 4) * 54;
            decals[decals_size]->y = (9 - 8) * 54;
            decals[decals_size]->width = 54;
            decals[decals_size]->height = 54;
            decals[decals_size]->on_click = decal_depo_callback;

            ++decals_size;
        }
    }

    if (piece_x + 1 <= 7 && (unmoveable_mask & (piece_mask << 1)) == 0)
    {
        put_new_decal(piece_x + 1, piece_y);
    }
    if (piece_x - 1 >= 0 && (unmoveable_mask & (piece_mask >> 1)) == 0)
    {
        put_new_decal(piece_x - 1, piece_y);
    }
    if (piece_y + 1 <= 7 && (unmoveable_mask & (piece_mask << 8)) == 0)
    {
        put_new_decal(piece_x, piece_y + 1);
    }
    if (piece_y - 1 >= 0 && (unmoveable_mask & (piece_mask >> 8)) == 0)
    {
        put_new_decal(piece_x, piece_y - 1);
    }

    if (piece_x == 3 && piece_y == 7)
    {
        decals[decals_size]->button_image.texture_ptr = (texture_t *)&move_depo_vis;
        decals[decals_size]->x = (7 - 3) * 54;
        decals[decals_size]->y = (9 - 8) * 54;
        decals[decals_size]->width = 54;
        decals[decals_size]->height = 54;
        decals[decals_size]->on_click = decal_depo_callback;

        ++decals_size;
    }

    if (piece_x == 4 && piece_y == 7)
    {
        decals[decals_size]->button_image.texture_ptr = (texture_t *)&move_depo_vis;
        decals[decals_size]->x = (7 - 4) * 54;
        decals[decals_size]->y = (9 - 8) * 54;
        decals[decals_size]->width = 54;
        decals[decals_size]->height = 54;
        decals[decals_size]->on_click = decal_depo_callback;

        ++decals_size;
    }
}

void cancel_boost(struct button_t *self)
{
    debug_printf("called cancel_boost\n");

    state = GAME_STATE_PLAYER_IDLE;

    surface->on_click = NULL;
    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;

    add_card_controls();
}

void boost_card(struct button_t *self)
{
    debug_printf("called boost_card\n");

    pos.is_boost_available_sec = 0;

    old_state = pos;

    surface->on_click = NULL;
    for (int i = 4; i < 12; ++i)
        controls[i]->on_click = NULL;

    const uint64_t piece_mask = 1ULL << ((7 - (self->x / 54)) + 8 * (9 - (self->y / 54)));

    pos.is_boosted_mask |= piece_mask;

    last_time_appear = current_time;

    state = GAME_STATE_PLAYER_BOOST_ANIM;
}

void boost_p1(struct button_t *self)
{
    debug_printf("called boost_p1\n");

    state = GAME_STATE_PLAYER_BOOST_P1;

    last_time_appear = current_time;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;
    surface->on_click = cancel_boost;

    uint64_t mask = pos.is_sec_mask;

    int controls_size = 4;

    while (mask)
    {
        const int piece_pos = __builtin_ctzll(mask);
        const uint64_t cur_mask = 1ULL << piece_pos;

        controls[controls_size]->x = (7 - (piece_pos & 7)) * 54;
        controls[controls_size]->y = (9 - (piece_pos >> 3)) * 54;
        controls[controls_size]->width = 54;
        controls[controls_size]->height = 54;
        controls[controls_size]->on_click = boost_card;

        ++controls_size;

        mask ^= cur_mask;
    }
}

void cancel_swap(struct button_t *self)
{
    debug_printf("called cancel_swap\n");

    decals[0]->on_click = NULL;
    decals[1]->on_click = NULL;
    surface->on_click = NULL;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;

    state = GAME_STATE_PLAYER_IDLE;

    decals_size = 0;

    add_card_controls();
}

void perform_swap(struct button_t *self)
{
    debug_printf("called perform_swap\n");

    last_time_appear = current_time;

    pos.is_swap_available_sec = 0;

    old_state = pos;

    const uint64_t fir_piece_mask = 1ULL << ((7 - (decals[0]->x / 54)) + 8 * (9 - (decals[0]->y / 54)));
    const uint64_t sec_piece_mask = 1ULL << ((7 - (decals[1]->x / 54)) + 8 * (9 - (decals[1]->y / 54)));

    uint64_t fir_link_mask = pos.is_link_mask & fir_piece_mask;
    pos.is_link_mask &= ~fir_piece_mask;
    uint64_t sec_link_mask = pos.is_link_mask & sec_piece_mask;
    pos.is_link_mask &= ~sec_piece_mask;

    if (fir_link_mask)
        pos.is_link_mask |= 1ULL << __builtin_ctzll(sec_piece_mask);
    if (sec_link_mask)
        pos.is_link_mask |= 1ULL << __builtin_ctzll(fir_piece_mask);

    decals[0]->on_click = NULL;
    decals[1]->on_click = NULL;
    surface->on_click = NULL;

    decals_size = 0;

    state = GAME_STATE_PLAYER_SWAP_ANIM;

    moved_card = fir_piece_mask;
    interacting_card = sec_piece_mask;
}

void perform_fake_swap(struct button_t *self)
{
    debug_printf("called perform_fake_swap\n");

    last_time_appear = current_time;

    const uint64_t fir_piece_mask = 1ULL << ((7 - (decals[0]->x / 54)) + 8 * (9 - (decals[0]->y / 54)));
    const uint64_t sec_piece_mask = 1ULL << ((7 - (decals[1]->x / 54)) + 8 * (9 - (decals[1]->y / 54)));

    pos.is_swap_available_sec = 0;

    decals[0]->on_click = NULL;
    decals[1]->on_click = NULL;
    surface->on_click = NULL;

    decals_size = 0;

    old_state = pos;

    state = GAME_STATE_PLAYER_SWAP_ANIM;

    moved_card = fir_piece_mask;
    interacting_card = sec_piece_mask;
}

void mark_swap_p3(struct button_t *self)
{
    debug_printf("called mark_swap_p3\n");

    state = GAME_STATE_PLAYER_SWAP_P3;

    decals[decals_size]->button_image.texture_ptr = (texture_t *)&fake_swap_vis;
    decals[decals_size]->x = self->x;
    decals[decals_size]->y = self->y;
    decals[decals_size]->width = 54;
    decals[decals_size]->height = 54;
    ++decals_size;

    for (int i = 4; i < 12; ++i)
        controls[i]->on_click = NULL;

    decals[0]->on_click = perform_swap;
    decals[1]->on_click = perform_fake_swap;
}

void mark_swap_p2(struct button_t *self)
{
    debug_printf("called mark_swap_p2\n");

    state = GAME_STATE_PLAYER_SWAP_P2;

    decals[decals_size]->button_image.texture_ptr = (texture_t *)&swap_vis;
    decals[decals_size]->x = self->x;
    decals[decals_size]->y = self->y;
    decals[decals_size]->width = 54;
    decals[decals_size]->height = 54;
    ++decals_size;

    interacting_card = 1ULL << ((7 - (decals[0]->x / 54)) + 8 * (9 - (decals[0]->y / 54)));

    uint64_t mask = pos.is_sec_mask & ~(interacting_card);

    int controls_size = 4;

    for (int i = 4; i < 12; ++i)
        controls[i]->on_click = NULL;

    while (mask)
    {
        const int piece_pos = __builtin_ctzll(mask);
        const uint64_t cur_mask = 1ULL << piece_pos;

        controls[controls_size]->x = (7 - (piece_pos & 7)) * 54;
        controls[controls_size]->y = (9 - (piece_pos >> 3)) * 54;
        controls[controls_size]->width = 54;
        controls[controls_size]->height = 54;
        controls[controls_size]->on_click = mark_swap_p3;

        ++controls_size;

        mask ^= cur_mask;
    }
}

void swap_p1(struct button_t *self)
{
    debug_printf("called swap_p1\n");

    last_time_appear = current_time;

    state = GAME_STATE_PLAYER_SWAP_P1;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;

    surface->on_click = cancel_swap;

    uint64_t mask = pos.is_sec_mask;

    int controls_size = 4;

    while (mask)
    {
        const int piece_pos = __builtin_ctzll(mask);
        const uint64_t cur_mask = 1ULL << piece_pos;

        controls[controls_size]->x = (7 - (piece_pos & 7)) * 54;
        controls[controls_size]->y = (9 - (piece_pos >> 3)) * 54;
        controls[controls_size]->width = 54;
        controls[controls_size]->height = 54;
        controls[controls_size]->on_click = mark_swap_p2;

        ++controls_size;

        mask ^= cur_mask;
    }
}

void cancel_checker(struct button_t *self)
{
    debug_printf("called cancel_checker\n");

    surface->on_click = NULL;
    for (int i = 0; i < decals_size; ++i)
        decals[i]->on_click = NULL;
    decals_size = 0;

    state = GAME_STATE_PLAYER_IDLE;

    add_card_controls();
}

void reveal_enemy_card(struct button_t *self)
{
    debug_printf("called reveal_enemy_card\n");

    const uint64_t piece_mask = 1ULL << ((7 - (self->x / 54)) + 8 * (9 - (self->y / 54)));

    pos.is_checker_available_sec = 0;
    enemy_reveal_mask = piece_mask;

    surface->on_click = NULL;
    for (int i = 0; i < decals_size; ++i)
        decals[i]->on_click = NULL;
    decals_size = 0;

    state = GAME_STATE_AI_IDLE;

    ai_move();
}

void checker_p1(struct button_t *self)
{
    debug_printf("called checker_p1\n");

    last_time_appear = current_time;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;

    state = GAME_STATE_PLAYER_CHECKER_P1;

    surface->on_click = cancel_checker;

    uint64_t mask = pos.is_fir_mask;

    while (mask)
    {
        const int piece_pos = __builtin_ctzll(mask);
        const uint64_t cur_mask = 1ULL << piece_pos;
        const int piece_x = piece_pos & 7;
        const int piece_y = piece_pos >> 3;

        decals[decals_size]->x = (7 - piece_x) * 54;
        decals[decals_size]->y = (9 - piece_y) * 54;
        decals[decals_size]->width = 54;
        decals[decals_size]->height = 54;
        decals[decals_size]->on_click = reveal_enemy_card;

        ++decals_size;

        mask ^= cur_mask;
    }
}

void cancel_unboost(struct button_t *self)
{
    debug_printf("called cancel_unboost\n");

    state = GAME_STATE_PLAYER_IDLE;

    controls[4]->on_click = NULL;
    surface->on_click = NULL;

    add_card_controls();
}

void perform_unboost(struct button_t *self)
{
    debug_printf("called perform_unboost\n");

    old_state = pos;

    self->on_click = NULL;
    surface->on_click = NULL;

    pos.is_boosted_mask &= pos.is_fir_mask;

    last_time_appear = current_time;

    // is_boost_available_sec is set after the animation plays

    state = GAME_STATE_PLAYER_UNBOOST_ANIM;
}

void unboost_p1(struct button_t *self)
{
    debug_printf("called unboost_p1\n");

    state = GAME_STATE_PLAYER_UNBOOST_P1;

    last_time_appear = current_time;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;
    surface->on_click = cancel_unboost;

    const int boosted_card_pos = __builtin_ctzll(pos.is_boosted_mask & pos.is_sec_mask);

    controls[4]->x = (7 - (boosted_card_pos & 7)) * 54;
    controls[4]->y = (9 - (boosted_card_pos >> 3)) * 54;
    controls[4]->on_click = perform_unboost;
}

void cancel_firewall(struct button_t *self)
{
    debug_printf("called cancel_firewall\n");

    for (int i = 0; i < decals_size; ++i)
        decals[i]->on_click = NULL;
    decals_size = 0;

    surface->on_click = NULL;

    state = GAME_STATE_PLAYER_IDLE;

    add_card_controls();
}

void place_firewall(struct button_t *self)
{
    debug_printf("called place_firewall\n");

    const int firewall_pos = ((7 - (self->x / 54)) + 8 * (9 - (self->y / 54)));

    pos.is_firewall_available_sec = 0;
    pos.firewall_sec = firewall_pos;

    for (int i = 0; i < decals_size; ++i)
        decals[i]->on_click = NULL;
    decals_size = 0;

    surface->on_click = NULL;

    state = GAME_STATE_AI_IDLE;

    ai_move();
}

void firewall_p1(struct button_t *self)
{
    debug_printf("called firewall_p1\n");

    state = GAME_STATE_PLAYER_FIREWALL_P1;

    last_time_appear = current_time;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;
    surface->on_click = cancel_firewall;

    const uint64_t enemy_firewall_mask = ((pos.is_firewall_available_fir) ? 0 : (1ULL << pos.firewall_fir));
    const uint64_t unmoveable_mask = pos.is_fir_mask | enemy_firewall_mask; // apparently you can place firewall on top of your cards so dont include 'pos.is_sec_mask'

    for (int i = 0; i < 64; ++i)
    {
        const uint64_t cur_mask = 1ULL << i;

        if (i == 3 || i == 4 || i == 60 || i == 59)
            continue;

        if ((cur_mask & unmoveable_mask) == 0)
        {
            decals[decals_size]->button_image.texture_ptr = (texture_t *)&firewall_ally;
            decals[decals_size]->x = (7 - (i & 7)) * 54;
            decals[decals_size]->y = (9 - (i >> 3)) * 54;
            decals[decals_size]->width = 54;
            decals[decals_size]->height = 54;
            decals[decals_size]->on_click = place_firewall;

            ++decals_size;
        }
    }
}

void cancel_unfirewall(struct button_t *self)
{
    debug_printf("called cancel_unfirewall\n");

    controls[4]->on_click = NULL;
    surface->on_click = NULL;

    state = GAME_STATE_PLAYER_IDLE;

    add_card_controls();
}

void perform_unfirewall(struct button_t *self)
{
    debug_printf("called perform_unfirewall\n");

    controls[4]->on_click = NULL;
    surface->on_click = NULL;

    pos.firewall_sec = 0;
    pos.is_firewall_available_sec = 1;

    state = GAME_STATE_AI_IDLE;

    ai_move();
}

void unfirewall_p1(struct button_t *self)
{
    debug_printf("called unfirewall_p1\n");

    state = GAME_STATE_PLAYER_UNFIREWALL_P1;

    last_time_appear = current_time;

    for (int i = 0; i < 12; ++i)
        controls[i]->on_click = NULL;
    surface->on_click = cancel_unfirewall;

    controls[4]->x = ((7 - (int)(pos.firewall_sec & 7)) * 54);
    controls[4]->y = ((9 - (int)(pos.firewall_sec >> 3)) * 54);
    controls[4]->on_click = perform_unfirewall;
}

void add_card_controls()
{
    debug_printf("called add_card_controls\n");

    assert(surface->on_click == NULL);
    for (int i = 0; i < 12; ++i)
        assert(controls[i]->on_click == NULL);
    for (int i = 0; i < 80; ++i)
        assert(decals[i]->on_click == NULL);

    if (pos.is_checker_available_sec)
        controls[0]->on_click = checker_p1;
    if (pos.is_swap_available_sec)
        controls[1]->on_click = swap_p1;
    if (pos.is_firewall_available_sec)
        controls[2]->on_click = firewall_p1;
    else
        controls[2]->on_click = unfirewall_p1;
    if (pos.is_boost_available_sec)
        controls[3]->on_click = boost_p1;
    else
        controls[3]->on_click = unboost_p1;

    uint64_t card_mask = pos.is_sec_mask;

    int controls_size = 4;

    while (card_mask)
    {
        const int card_pos = __builtin_ctzll(card_mask);
        const uint64_t cur_pos = 1ULL << card_pos;

        controls[controls_size]->x = (7 - (card_pos & 7)) * 54;
        controls[controls_size]->y = (9 - (card_pos >> 3)) * 54;
        controls[controls_size]->on_click = card_move_p1;
        ++controls_size;

        card_mask ^= cur_pos;
    }
}

void *ai_move_thread(void *args)
{
    debug_printf("adaptive_ai_level = %d\n", adaptive_ai_level);
    minimax_main_result_t move = minimax_iteration_main(adaptive_ai_level, MIN, MAX, true, pos);

    if (max_time < 500 && adaptive_ai_level < 20)
    {
        adaptive_ai_level += 2;
    }
    else if (max_time > 10000 && adaptive_ai_level > ai_level)
    {
        adaptive_ai_level -= 2;
    }

    pthread_mutex_lock(&ai_move_mtx);

    old_state = pos;

    field_t new_field = move.best_field;

    uint64_t fir_link_mask = pos.is_link_mask & pos.is_fir_mask;
    uint64_t fir_virus_mask = pos.is_fir_mask ^ fir_link_mask;
    uint64_t sec_link_mask = pos.is_link_mask ^ fir_link_mask;
    uint64_t sec_virus_mask = pos.is_sec_mask ^ sec_link_mask;

    uint64_t new_fir_link_mask = new_field.is_link_mask & new_field.is_fir_mask;
    uint64_t new_fir_virus_mask = new_field.is_fir_mask ^ new_fir_link_mask;
    uint64_t new_sec_link_mask = new_field.is_link_mask ^ new_fir_link_mask;
    uint64_t new_sec_virus_mask = new_field.is_sec_mask ^ new_sec_link_mask;

    const uint64_t moved_card_pos = (pos.is_fir_mask ^ new_field.is_fir_mask) & (~new_field.is_fir_mask);
    const uint64_t move_dest = (pos.is_fir_mask ^ new_field.is_fir_mask) & (~pos.is_fir_mask);

    texture_ptr = card_to_texture(moved_card_pos, pos, true);
    sec_texture_ptr = card_to_texture(move_dest, pos, false);

    // now the actual fuckery
    if (pos.is_firewall_available_fir != new_field.is_firewall_available_fir) // ai swapped a card
    {
        if (pos.is_firewall_available_fir) // place
        {
            state = GAME_STATE_AI_FIREWALL;

            interacting_card = 1ULL << pos.firewall_fir;
        }
        else // remove
        {
            state = GAME_STATE_AI_UNFIREWALL;

            interacting_card = 1ULL << pos.firewall_fir;
        }
    }
    else if (pos.is_swap_available_fir != new_field.is_swap_available_fir) // ai swapped a card
    {
        state = GAME_STATE_AI_SWAP;

        old_state.is_swap_available_fir = 0;

        moved_card = (fir_link_mask ^ new_fir_link_mask) & (~new_fir_link_mask);
        interacting_card = (fir_link_mask ^ new_fir_link_mask) & (~fir_link_mask);
    }
    else if (new_sec_link_mask != sec_link_mask) // ai captured link card
    {
        state = GAME_STATE_AI_CAPTURE;
        // no need to update taken_first_links

        moved_card = moved_card_pos;
        old_state.is_fir_mask &= ~moved_card_pos;
        old_state.is_link_mask &= ~moved_card_pos;
        old_state.is_boosted_mask &= ~moved_card_pos;

        interacting_card = move_dest;
        old_state.is_sec_mask &= ~interacting_card;
        old_state.is_link_mask &= ~interacting_card;
        old_state.is_boosted_mask &= ~interacting_card;

        if (pos.is_checker_available_sec == 0 && moved_card_pos == enemy_reveal_mask)
        {
            enemy_reveal_mask = move_dest;
        }
    }
    else if (new_sec_virus_mask != sec_virus_mask) // ai captured virus card
    {
        state = GAME_STATE_AI_CAPTURE;
        // no need to update taken_first_links

        moved_card = moved_card_pos;
        old_state.is_fir_mask &= ~moved_card_pos;
        old_state.is_link_mask &= ~moved_card_pos;
        old_state.is_boosted_mask &= ~moved_card_pos;

        interacting_card = move_dest;
        old_state.is_sec_mask &= ~interacting_card;
        old_state.is_link_mask &= ~interacting_card;
        old_state.is_boosted_mask &= ~interacting_card;

        if (pos.is_checker_available_sec == 0 && moved_card_pos == enemy_reveal_mask)
        {
            enemy_reveal_mask = move_dest;
        }
    }
    else if (fir_link_mask != new_fir_link_mask) // move or deposit
    {
        if (__builtin_popcountll(fir_link_mask) != __builtin_popcountll(new_fir_link_mask)) // deposit
        {
            state = GAME_STATE_AI_DEPOSIT;

            if (old_state.is_link_mask & moved_card_pos)
            {
                sec_texture_ptr = &link_enemy;
            }
            else
            {
                sec_texture_ptr = &virus_enemy;
            }

            old_state.is_fir_mask &= ~moved_card_pos;
            old_state.is_link_mask &= ~moved_card_pos;
            old_state.is_boosted_mask &= ~moved_card_pos;

            moved_card = moved_card_pos;
            interacting_card = move_dest;

            taken_first_links |= (1 << pos.fir_link);

            if (pos.is_checker_available_sec == 0 && moved_card_pos == enemy_reveal_mask)
            {
                enemy_reveal_mask = 0;
            }
        }
        else // move
        {
            state = GAME_STATE_AI_MOVE;

            moved_card = moved_card_pos;
            old_state.is_fir_mask &= ~moved_card_pos;
            old_state.is_link_mask &= ~moved_card_pos;
            old_state.is_boosted_mask &= ~moved_card_pos;

            interacting_card = move_dest;

            if (pos.is_checker_available_sec == 0 && moved_card_pos == enemy_reveal_mask)
            {
                enemy_reveal_mask = move_dest;
            }
        }
    }
    else if (fir_virus_mask != new_fir_virus_mask) // move or deposit
    {
        if (__builtin_popcountll(fir_virus_mask) != __builtin_popcountll(new_fir_virus_mask)) // deposit
        {
            state = GAME_STATE_AI_DEPOSIT;

            if (old_state.is_link_mask & moved_card_pos)
            {
                sec_texture_ptr = &link_enemy;
            }
            else
            {
                sec_texture_ptr = &virus_enemy;
            }

            old_state.is_fir_mask &= ~moved_card_pos;
            old_state.is_link_mask &= ~moved_card_pos;
            old_state.is_boosted_mask &= ~moved_card_pos;

            moved_card = moved_card_pos;
            interacting_card = move_dest;

            if (sec_texture_ptr == &link_ally_boosted || sec_texture_ptr == &link_ally_boosted_revealed)
            {
                sec_texture_ptr = &link_ally;
            }
            else if (sec_texture_ptr == &virus_ally_boosted || sec_texture_ptr == &virus_ally_boosted_revealed)
            {
                sec_texture_ptr = &virus_ally;
            }

            taken_first_viruses |= (1 << pos.fir_virus);

            if (pos.is_checker_available_sec == 0 && moved_card_pos == enemy_reveal_mask)
            {
                enemy_reveal_mask = 0;
            }
        }
        else // move
        {
            state = GAME_STATE_AI_MOVE;

            moved_card = moved_card_pos;
            old_state.is_fir_mask &= ~moved_card_pos;
            old_state.is_link_mask &= ~moved_card_pos;
            old_state.is_boosted_mask &= ~moved_card_pos;
            interacting_card = move_dest;

            if (pos.is_checker_available_sec == 0 && moved_card_pos == enemy_reveal_mask)
            {
                enemy_reveal_mask = move_dest;
            }
        }
    }
    else if (pos.is_boosted_mask != new_field.is_boosted_mask)
    {
        if (pos.is_boost_available_fir) // ai boosted a card
        {
            old_state.is_boost_available_fir = 0;
            state = GAME_STATE_AI_BOOST;
        }
        else // ai un-boosted a card
        {
            new_field.is_boost_available_fir = 0;
            state = GAME_STATE_AI_UNBOOST;
        }
    }
    else
    {
        assert(false);
    }

    pos = new_field;

    last_time_appear = current_time;

    // if (pos.fir_link == 4)
    // {
    //     state = GAME_STATE_PLAYER_LOSE;
    // }
    // else if (pos.fir_virus == 4)
    // {
    //     state = GAME_STATE_PLAYER_WIN;
    // }
    // else
    // {
    //     state = GAME_STATE_PLAYER_IDLE;

    //     add_card_controls();
    // }

    pthread_mutex_unlock(&ai_move_mtx);

    return NULL;
}

void ai_move()
{
    pthread_t ai_thread;

    pthread_create(&ai_thread, NULL, ai_move_thread, NULL);
    pthread_detach(ai_thread);
}

void place_card(struct button_t *self)
{
    const uint64_t piece_mask = 1ULL << ((7 - (self->x / 54)) + 8 * (9 - (self->y / 54)));

    pos.is_sec_mask |= piece_mask;
    if (player_field_place_id > 3)
        player_field_id |= (1 << (7 - (self->x / 54)));
    else
        pos.is_link_mask |= piece_mask;

    ++player_field_place_id;

    last_time_appear = current_time;

    self->on_click = NULL;

    if (player_field_place_id == 8)
    {
        field_construct(pos, indexes[rand() % 70], player_field_id);
        // field_construct(pos, indexes[15], player_field_id);

        adaptive_ai_level = ai_level;

        state = (is_ai_turn) ? GAME_STATE_AI_IDLE : GAME_STATE_PLAYER_IDLE;

        if (state == GAME_STATE_PLAYER_IDLE)
        {
            add_card_controls();
        }
        else
        {
            ai_move();
        }
    }
}

void begin_game(struct button_t *self)
{
    player_starts_switch->visible = false;
    ai_starts_switch->visible = false;
    ai_increase_diff->visible = false;
    ai_decrease_diff->visible = false;
    start_game->visible = false;

    state = GAME_STATE_PLACE_CARDS;

    is_ai_turn = !player_begins;

    memset(&pos, 0, sizeof(field_t));

    pos.forward_adv_fir = 2;
    pos.forward_adv_sec = 2;

    pos.is_boost_available_fir = 1;
    pos.is_boost_available_sec = 1;
    pos.is_checker_available_fir = 1;
    pos.is_checker_available_sec = 1;
    pos.is_swap_available_fir = 1;
    pos.is_swap_available_sec = 1;
    pos.is_firewall_available_fir = 1;
    pos.is_firewall_available_sec = 1;

    enemy_reveal_mask = 0;
    ally_reveal_mask = 0;
    taken_first_links = 0;
    taken_first_viruses = 0;
    taken_second_links = 0;
    taken_second_viruses = 0;
    player_field_id = 0;
    player_field_place_id = 0;

    last_time_appear = current_time;

    controls[4]->x = 54 * 7;
    controls[4]->y = 54 * 9;
    controls[4]->on_click = place_card;

    controls[5]->x = 54 * 6;
    controls[5]->y = 54 * 9;
    controls[5]->on_click = place_card;

    controls[6]->x = 54 * 5;
    controls[6]->y = 54 * 9;
    controls[6]->on_click = place_card;

    controls[7]->x = 54 * 4;
    controls[7]->y = 54 * 8;
    controls[7]->on_click = place_card;

    controls[8]->x = 54 * 3;
    controls[8]->y = 54 * 8;
    controls[8]->on_click = place_card;

    controls[9]->x = 54 * 2;
    controls[9]->y = 54 * 9;
    controls[9]->on_click = place_card;

    controls[10]->x = 54 * 1;
    controls[10]->y = 54 * 9;
    controls[10]->on_click = place_card;

    controls[11]->x = 54 * 0;
    controls[11]->y = 54 * 9;
    controls[11]->on_click = place_card;
}

int main()
{
    srand(time(NULL));

    init("rnab");

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    for (int i = 0; i < 12; ++i)
    {
        controls[i] = button_alloc();
        controls[i]->width = 54;
        controls[i]->height = 54;
        controls[i]->visible = true;
    }
    for (int i = 0; i < 80; ++i)
    {
        decals[i] = button_alloc();
        decals[i]->visible = true;
    }

    surface = button_alloc();
    surface->visible = true;
    surface->x = 0;
    surface->y = 0;
    surface->width = BASE_W;
    surface->height = BASE_H;

    controls[0]->x = 54 * 1; // virus checker
    controls[0]->y = 54 * 10;

    controls[1]->x = 54 * 2; // swap
    controls[1]->y = 54 * 10;

    controls[2]->x = 54 * 5; // firewall
    controls[2]->y = 54 * 10;

    controls[3]->x = 54 * 6; // line boost
    controls[3]->y = 54 * 10;

    player_starts_switch = button_text_3_slice_init(&main_font, 64, "AI Begins", (texture_t *)&button_slice_corner, (texture_t *)&button_slice_side, (texture_t *)&button_slice_center, 0, 50, 20, 50, 50, 20, 255, 255, 255, 255, 255, 255, 255, 255);
    player_starts_switch->x = (BASE_W - player_starts_switch->padding_left - player_starts_switch->padding_right - player_starts_switch->width) / 2;
    player_starts_switch->visible = false;
    player_starts_switch->on_click = switch_player;
    ai_starts_switch = button_text_3_slice_init(&main_font, 64, "Player Begins", (texture_t *)&button_slice_corner, (texture_t *)&button_slice_side, (texture_t *)&button_slice_center, 0, 50, 20, 50, 50, 20, 255, 255, 255, 255, 255, 255, 255, 255);
    ai_starts_switch->x = (BASE_W - ai_starts_switch->padding_left - ai_starts_switch->padding_right - ai_starts_switch->width) / 2;
    ai_starts_switch->on_click = switch_player;

    ai_decrease_diff = button_text_3_slice_init(&main_font, 64, "-", (texture_t *)&button_slice_corner, (texture_t *)&button_slice_side, (texture_t *)&button_slice_center, 0, 300, 20, 30, 30, 20, 255, 255, 255, 255, 255, 255, 255, 255);
    ai_decrease_diff->on_click = decrease_diff;
    ai_increase_diff = button_text_3_slice_init(&main_font, 64, "+", (texture_t *)&button_slice_corner, (texture_t *)&button_slice_side, (texture_t *)&button_slice_center, 0, 300, 20, 30, 30, 20, 255, 255, 255, 255, 255, 255, 255, 255);
    ai_increase_diff->on_click = increase_diff;
    int spacer = (BASE_W - ai_decrease_diff->padding_left - ai_decrease_diff->padding_right - ai_decrease_diff->width - ai_increase_diff->padding_left - ai_increase_diff->padding_right - ai_increase_diff->width) / 3;

    ai_decrease_diff->x = spacer;
    ai_increase_diff->x = spacer * 2 + ai_decrease_diff->padding_left + ai_decrease_diff->padding_right + ai_decrease_diff->width;

    start_game = button_text_3_slice_init(&main_font, 64, "Begin Game", (texture_t *)&button_slice_corner, (texture_t *)&button_slice_side, (texture_t *)&button_slice_center, 0, 450, 20, 30, 30, 20, 255, 255, 255, 255, 255, 255, 255, 255);
    start_game->x = (BASE_W - start_game->padding_left - start_game->padding_right - start_game->width) / 2;
    start_game->on_click = begin_game;

    char buf[64];

    while (!glfwWindowShouldClose(window))
    {
        switch (state)
        {
        case GAME_STATE_AI_FIREWALL:
        {
            state = GAME_STATE_PLAYER_IDLE;
            add_card_controls();
            break;
        }

        case GAME_STATE_AI_UNFIREWALL:
        {
            state = GAME_STATE_PLAYER_IDLE;
            add_card_controls();
            break;
        }

        case GAME_STATE_MENU:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_button(player_starts_switch);
            render_button(ai_starts_switch);
            render_button(ai_decrease_diff);
            render_button(ai_increase_diff);
            render_button(start_game);

            sprintf(buf, "AI level: %d", ai_level);

            int text_w, text_h;
            size_text_int(&main_font, 64, buf, &text_w, &text_h);
            render_text(&main_font, 64, buf, (BASE_W - text_w) / 2, 200, 255, 255, 255, 255);

            end_render();
            break;
        }

        case GAME_STATE_PLACE_CARDS:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            float progress = (float)(current_time - last_time_appear) / (float)(500000);
            if (progress > 1.0f)
                progress = 1.0f;

            float eased = 2.0f * progress - progress * progress;
            float tex_size = 54.0f * eased;

            render_tex_scale(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), (BASE_W - tex_size) / 2, (BASE_H - tex_size) / 2, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

            if ((pos.is_sec_mask & 1) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 7, 9 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            if ((pos.is_sec_mask & 2) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 6, 9 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            if ((pos.is_sec_mask & 4) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 5, 9 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            if ((pos.is_sec_mask & 2048) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 4, 8 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            if ((pos.is_sec_mask & 4096) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 3, 8 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            if ((pos.is_sec_mask & 32) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 2, 9 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            if ((pos.is_sec_mask & 64) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 1, 9 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            if ((pos.is_sec_mask & 128) == 0)
            {
                render_tex(((player_field_place_id < 4) ? (&link_ally) : (&virus_ally)), 54 * 0, 9 * 54, blend_alpha, blend_alpha, blend_alpha, blend_alpha, 0);
            }

            end_render();

            break;
        }

        case GAME_STATE_AI_IDLE:
        {
            pthread_mutex_lock(&ai_move_mtx);
            if (state != GAME_STATE_AI_IDLE)
                break;
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);
            pthread_mutex_unlock(&ai_move_mtx);

            end_render();

            break;
        }

        case GAME_STATE_PLAYER_CAPTURE_CARD_ANIM:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            if ((current_time - last_time_appear) < 750000)
            {
                const int fir_piece_pos = __builtin_ctzll(interacting_card);
                const int sec_piece_pos = __builtin_ctzll(moved_card);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(sec_texture_ptr, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                const int sec_piece_pos = __builtin_ctzll(moved_card);
                const int tray_pos = (old_state.sec_link < pos.sec_link) ? ((int)old_state.sec_link * 54 + 4 * 54) : ((int)old_state.sec_virus * 54);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(((old_state.sec_link < pos.sec_link) ? (&link_enemy) : (&virus_enemy)), (float)(tray_pos) + (1.0f - eased) * 27.0f, (float)(11 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                if (old_state.is_boost_available_fir == 0 && pos.is_boost_available_fir != 0)
                {
                    render_tex_scale(&boost_enemy, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(1 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                }

                if ((current_time - last_time_appear) >= 1500000)
                {
                    last_time_appear = current_time;

                    if (pos.sec_link == 4)
                    {
                        state = GAME_STATE_PLAYER_WIN;
                    }
                    else if (pos.sec_virus == 4)
                    {
                        state = GAME_STATE_PLAYER_LOSE;
                    }
                    else
                    {
                        state = GAME_STATE_AI_IDLE;
                        ai_move();
                    }
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_MOVE_CARD_ANIM:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            if ((current_time - last_time_appear) < 750000)
            {
                const int piece_pos = __builtin_ctzll(interacting_card);
                const int piece_x = piece_pos & 7;
                const int piece_y = piece_pos >> 3;

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                const int piece_pos = __builtin_ctzll(moved_card);
                const int piece_x = piece_pos & 7;
                const int piece_y = piece_pos >> 3;

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 1500000)
                {
                    state = GAME_STATE_AI_IDLE;
                    ai_move();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_AI_MOVE:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            if ((current_time - last_time_appear) < 750000)
            {
                const int piece_pos = __builtin_ctzll(moved_card);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                const int piece_pos = __builtin_ctzll(interacting_card);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 1500000)
                {
                    state = GAME_STATE_PLAYER_IDLE;
                    add_card_controls();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_SWAP_ANIM:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            if ((current_time - last_time_appear) < 750000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(&not_found_ally, (float)(2 * 54) + (1.0f - eased) * 27.0f, (float)(10 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                const int fir_piece_pos = __builtin_ctzll(moved_card);
                const int sec_piece_pos = __builtin_ctzll(interacting_card);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(&not_found_ally, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(&not_found_ally, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                if (ally_reveal_mask == moved_card || ally_reveal_mask == interacting_card)
                    ally_reveal_mask = 0;

                const int fir_piece_pos = __builtin_ctzll(moved_card);
                const int sec_piece_pos = __builtin_ctzll(interacting_card);

                float progress = 1.0f - (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(&not_found_ally, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(&not_found_ally, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 2250000)
                {
                    last_time_appear = current_time;

                    state = GAME_STATE_AI_IDLE;
                    ai_move();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_AI_SWAP:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            if ((current_time - last_time_appear) < 750000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&not_found_enemy, (float)(2 * 54) + (1.0f - eased) * 27.0f, (float)(1 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                const int fir_piece_pos = __builtin_ctzll(moved_card);
                const int sec_piece_pos = __builtin_ctzll(interacting_card);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&not_found_enemy, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(&not_found_enemy, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                if (enemy_reveal_mask == moved_card || enemy_reveal_mask == interacting_card)
                    enemy_reveal_mask = 0;

                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                const int fir_piece_pos = __builtin_ctzll(moved_card);
                const int sec_piece_pos = __builtin_ctzll(interacting_card);

                float progress = 1.0f - (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&not_found_enemy, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(&not_found_enemy, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 2250000)
                {
                    last_time_appear = current_time;

                    state = GAME_STATE_PLAYER_IDLE;
                    add_card_controls();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_AI_CAPTURE:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            if ((current_time - last_time_appear) < 750000)
            {
                const int fir_piece_pos = __builtin_ctzll(moved_card);
                const int sec_piece_pos = __builtin_ctzll(interacting_card);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(sec_texture_ptr, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                const int sec_piece_pos = __builtin_ctzll(interacting_card);
                const int tray_pos = (old_state.fir_link < pos.fir_link) ? ((int)old_state.fir_link * 54 + 4 * 54) : ((int)old_state.fir_virus * 54);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (sec_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (sec_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                render_tex_scale(((old_state.fir_link < pos.fir_link) ? (&link_ally) : (&virus_ally)), (float)(tray_pos) + (1.0f - eased) * 27.0f, (float)(0) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                if (old_state.is_boost_available_sec == 0 && pos.is_boost_available_sec != 0)
                {
                    render_tex_scale(&boost_ally, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(10 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                }

                if ((current_time - last_time_appear) >= 1500000)
                {
                    last_time_appear = current_time;

                    if (pos.fir_link == 4)
                    {
                        state = GAME_STATE_PLAYER_LOSE;
                    }
                    else if (pos.fir_virus == 4)
                    {
                        state = GAME_STATE_PLAYER_WIN;
                    }
                    else
                    {
                        state = GAME_STATE_PLAYER_IDLE;

                        add_card_controls();
                    }
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_DEPOSIT_CARD_ANIM:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            if ((current_time - last_time_appear) < 750000)
            {
                const int fir_piece_pos = __builtin_ctzll(interacting_card);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                const int fir_piece_pos = __builtin_ctzll(interacting_card);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(sec_texture_ptr, 189.0f + (1.0f - eased) * 27.0f, (float)(54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                const int tray_pos = (old_state.sec_link < pos.sec_link) ? ((int)old_state.sec_link * 54 + 4 * 54) : ((int)old_state.sec_virus * 54);

                {
                    float progress = (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                    if (progress > 1.0f)
                        progress = 1.0f;

                    float eased = 2.0f * progress - progress * progress;

                    render_tex_scale(sec_texture_ptr, (float)(tray_pos) + (1.0f - eased) * 27.0f, (float)(11 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                    if (old_state.is_boost_available_sec == 0 && pos.is_boost_available_sec != 0)
                    {
                        render_tex_scale(&boost_ally, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(10 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                    }
                }

                {
                    float progress = 1.0f - (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                    if (progress < 0.0f)
                        progress = 0.0f;

                    float eased = 2.0f * progress - progress * progress;

                    render_tex_scale(sec_texture_ptr, 189.0f + (1.0f - eased) * 27.0f, (float)(54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                }

                if ((current_time - last_time_appear) >= 2250000)
                {
                    last_time_appear = current_time;

                    if (pos.sec_link == 4)
                    {
                        state = GAME_STATE_PLAYER_WIN;
                    }
                    else if (pos.sec_virus == 4)
                    {
                        state = GAME_STATE_PLAYER_LOSE;
                    }
                    else
                    {
                        state = GAME_STATE_AI_IDLE;
                        ai_move();
                    }
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_AI_DEPOSIT:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            if ((current_time - last_time_appear) < 750000)
            {
                const int fir_piece_pos = __builtin_ctzll(moved_card);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(texture_ptr, (float)((7 - (fir_piece_pos & 7)) * 54) + (1.0f - eased) * 27.0f, (float)((9 - (fir_piece_pos >> 3)) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                const int fir_piece_pos = __builtin_ctzll(moved_card);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;
                float tex_size = 54.0f * eased;

                render_tex_scale(sec_texture_ptr, 189.0F + (1.0f - eased) * 27.0f, (float)(10 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                const int tray_pos = (old_state.fir_link < pos.fir_link) ? ((int)old_state.fir_link * 54 + 4 * 54) : ((int)old_state.fir_virus * 54);

                {
                    float progress = (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                    if (progress > 1.0f)
                        progress = 1.0f;

                    float eased = 2.0f * progress - progress * progress;

                    render_tex_scale(sec_texture_ptr, (float)(tray_pos) + (1.0f - eased) * 27.0f, (float)(0) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                    if (old_state.is_boost_available_fir == 0 && pos.is_boost_available_fir != 0)
                    {
                        render_tex_scale(&boost_enemy, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(1 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                    }
                }

                {
                    float progress = 1.0f - (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                    if (progress < 0.0f)
                        progress = 0.0f;

                    float eased = 2.0f * progress - progress * progress;

                    render_tex_scale(sec_texture_ptr, 189.0F + (1.0f - eased) * 27.0f, (float)(10 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
                }

                if ((current_time - last_time_appear) >= 2250000)
                {
                    last_time_appear = current_time;

                    if (pos.fir_link == 4)
                    {
                        state = GAME_STATE_PLAYER_LOSE;
                    }
                    else if (pos.fir_virus == 4)
                    {
                        state = GAME_STATE_PLAYER_WIN;
                    }
                    else
                    {
                        state = GAME_STATE_PLAYER_IDLE;

                        add_card_controls();
                    }
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_BOOST_ANIM:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            const uint64_t piece_mask = pos.is_boosted_mask & pos.is_sec_mask;
            const int piece_pos = __builtin_ctzll(piece_mask);
            const int piece_x = piece_pos & 7;
            const int piece_y = piece_pos >> 3;

            if ((current_time - last_time_appear) < 750000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_ally, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(10 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_ally, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_ally, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 2250000)
                {
                    state = GAME_STATE_AI_IDLE;
                    ai_move();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_AI_BOOST:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            const uint64_t piece_mask = pos.is_boosted_mask & pos.is_fir_mask;
            const int piece_pos = __builtin_ctzll(piece_mask);
            const int piece_x = piece_pos & 7;
            const int piece_y = piece_pos >> 3;

            if ((current_time - last_time_appear) < 750000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_enemy, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(1 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_enemy, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_enemy, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 2250000)
                {
                    state = GAME_STATE_PLAYER_IDLE;
                    add_card_controls();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_UNBOOST_ANIM:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            const uint64_t piece_mask = old_state.is_boosted_mask & old_state.is_sec_mask;
            const int piece_pos = __builtin_ctzll(piece_mask);
            const int piece_x = piece_pos & 7;
            const int piece_y = piece_pos >> 3;

            if ((current_time - last_time_appear) < 750000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = (float)(current_time - last_time_appear) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_ally, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_ally, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_ally, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(10 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 2250000)
                {
                    pos.is_boost_available_sec = 1;
                    state = GAME_STATE_AI_IDLE;
                    ai_move();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_UNFIREWALL_P1:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            const int card_pos = pos.firewall_sec;

            render_tex(&firewall_ally, (7 - (card_pos & 7)) * 54, (9 - (card_pos >> 3)) * 54, blend_alpha, blend_alpha, blend_alpha, 255, 0);

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_FIREWALL_P1:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            for (int i = 0; i < decals_size; ++i)
            {
                render_tex(decals[i]->button_image.texture_ptr, decals[i]->x, decals[i]->y, blend_alpha, blend_alpha, blend_alpha, 255, 0);
            }

            end_render();
            break;
        }

        case GAME_STATE_AI_UNBOOST:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            const uint64_t piece_mask = old_state.is_boosted_mask & old_state.is_fir_mask;
            const int piece_pos = __builtin_ctzll(piece_mask);
            const int piece_x = piece_pos & 7;
            const int piece_y = piece_pos >> 3;

            if ((current_time - last_time_appear) < 750000)
            {
                render_game_field(old_state, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = (float)(current_time - last_time_appear) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_enemy, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else if ((current_time - last_time_appear) < 1500000)
            {
                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = 1.0f - (float)(current_time - last_time_appear - 750000) / (float)(500000);
                if (progress < 0.0f)
                    progress = 0.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_enemy, (float)((7 - piece_x) * 54) + (1.0f - eased) * 27.0f, (float)((9 - piece_y) * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);
            }
            else
            {
                render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

                float progress = (float)(current_time - last_time_appear - 1500000) / (float)(500000);
                if (progress > 1.0f)
                    progress = 1.0f;

                float eased = 2.0f * progress - progress * progress;

                render_tex_scale(&boost_enemy, (float)(6 * 54) + (1.0f - eased) * 27.0f, (float)(1 * 54) + (1.0f - eased) * 27.0f, eased, 255, 255, 255, (uint8_t)((1.0f - (1.0f - progress) * (1.0f - progress)) * 255.0f), 0);

                if ((current_time - last_time_appear) >= 2250000)
                {
                    pos.is_boost_available_fir = 1;
                    state = GAME_STATE_PLAYER_IDLE;
                    add_card_controls();
                }
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_SWAP_P1:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            uint64_t card_mask = pos.is_sec_mask;

            while (card_mask)
            {
                const int card_pos = __builtin_ctzll(card_mask);
                const uint64_t cur_pos = 1ULL << card_pos;

                render_tex(card_to_texture(cur_pos, pos, false), (7 - (card_pos & 7)) * 54, (9 - (card_pos >> 3)) * 54, blend_alpha, blend_alpha, blend_alpha, 255, 0);

                card_mask ^= cur_pos;
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_SWAP_P2:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            uint64_t card_mask = pos.is_sec_mask;

            while (card_mask)
            {
                const int card_pos = __builtin_ctzll(card_mask);
                const uint64_t cur_pos = 1ULL << card_pos;

                if (cur_pos != interacting_card)
                {
                    render_tex(card_to_texture(cur_pos, pos, false), (7 - (card_pos & 7)) * 54, (9 - (card_pos >> 3)) * 54, blend_alpha, blend_alpha, blend_alpha, 255, 0);
                }

                card_mask ^= cur_pos;
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_SWAP_P3:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));
            for (int i = 0; i < 2; ++i)
            {
                render_tex(decals[i]->button_image.texture_ptr, decals[i]->x, decals[i]->y, blend_alpha, blend_alpha, blend_alpha, 255, 0);
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_BOOST_P1:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            uint64_t card_mask = pos.is_sec_mask;

            while (card_mask)
            {
                const int card_pos = __builtin_ctzll(card_mask);
                const uint64_t cur_pos = 1ULL << card_pos;

                render_tex(card_to_texture(cur_pos, pos, false), (7 - (card_pos & 7)) * 54, (9 - (card_pos >> 3)) * 54, blend_alpha, blend_alpha, blend_alpha, 255, 0);

                card_mask ^= cur_pos;
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_MOVE_CARD:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            for (int i = 0; i < decals_size; ++i)
            {
                render_tex(decals[i]->button_image.texture_ptr, decals[i]->x, decals[i]->y, 255, 255, 255, 255, 0);
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_CHECKER_P1:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            uint64_t card_mask = pos.is_fir_mask;

            while (card_mask)
            {
                const int card_pos = __builtin_ctzll(card_mask);
                const uint64_t cur_pos = 1ULL << card_pos;

                render_tex(card_to_texture(cur_pos, pos, true), (7 - (card_pos & 7)) * 54, (9 - (card_pos >> 3)) * 54, blend_alpha, blend_alpha, blend_alpha, 255, 0);

                card_mask ^= cur_pos;
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_UNBOOST_P1:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            uint8_t blend_alpha = (uint8_t)(191.0f + 64.0f * cosf(M_PI * 2.0f * ((current_time - last_time_appear) / 1000000.0f)));

            const int card_pos = __builtin_ctzll(pos.is_boosted_mask & pos.is_sec_mask);
            const int card_x = card_pos & 7;
            const int card_y = card_pos >> 3;

            if (pos.is_link_mask & (1ULL << card_pos))
                render_tex(&link_ally_boosted, (7 - card_x) * 54, (9 - card_y) * 54, blend_alpha, blend_alpha, blend_alpha, 255, 0);
            else
                render_tex(&virus_ally_boosted, (7 - card_x) * 54, (9 - card_y) * 54, blend_alpha, blend_alpha, blend_alpha, 255, 0);

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_LOSE:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            int progress = (current_time - last_time_appear) / 10000;
            int glob_iteration = 0;

            for (int i = 0; i < 12 && glob_iteration < progress; ++i)
            {
                for (int u = 0; u < 8 && glob_iteration < progress; ++u)
                {
                    render_tex(&virus_enemy, u * 54, i * 54, 255, 255, 255, 255, 0);
                    ++glob_iteration;
                }
            }

            if (progress >= 12 * 8)
            {
                last_time_appear = current_time;

                state = GAME_STATE_PLAYER_LOST;
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_LOST:
        {
            if (current_time - last_time_appear > 6000000)
            {
                state = GAME_STATE_MENU;

                if (player_begins)
                    ai_starts_switch->visible = true;
                else
                    player_starts_switch->visible = true;
                ai_increase_diff->visible = true;
                ai_decrease_diff->visible = true;
                start_game->visible = true;
                break;
            }

            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            uint8_t alpha = ((current_time - last_time_appear) > 3000000) ? (((6000000 - (current_time - last_time_appear)) * 255) / 3000000) : 255;
            ;

            render_tex(&loose_screen_vis, 0, 0, 255, 255, 255, alpha, 0);

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_IDLE:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_WIN:
        {
            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            render_game_field(pos, HIDE_ENEMY_CARDS, enemy_reveal_mask, ally_reveal_mask, taken_first_links, taken_first_viruses, taken_second_links, taken_second_viruses);

            int progress = (current_time - last_time_appear) / 10000;
            int glob_iteration = 0;

            for (int i = 0; i < 12 && glob_iteration < progress; ++i)
            {
                for (int u = 0; u < 8 && glob_iteration < progress; ++u)
                {
                    render_tex(&link_ally, u * 54, i * 54, 255, 255, 255, 255, 0);
                    ++glob_iteration;
                }
            }

            if (progress >= 12 * 8)
            {
                last_time_appear = current_time;

                state = GAME_STATE_PLAYER_WON;
            }

            end_render();
            break;
        }

        case GAME_STATE_PLAYER_WON:
        {
            if (current_time - last_time_appear > 3000000)
            {
                state = GAME_STATE_MENU;

                if (player_begins)
                    ai_starts_switch->visible = true;
                else
                    player_starts_switch->visible = true;
                ai_increase_diff->visible = true;
                ai_decrease_diff->visible = true;
                start_game->visible = true;
                break;
            }

            start_render();
            glClear(GL_COLOR_BUFFER_BIT);

            uint8_t alpha = (((3000000 - (current_time - last_time_appear)) * 255) / 3000000);

            for (int i = 0; i < 12; ++i)
            {
                for (int u = 0; u < 8; ++u)
                {
                    render_tex(&link_ally, u * 54, i * 54, 255, 255, 255, alpha, 0);
                }
            }

            end_render();
            break;
        }

        default:
            assert(false);
        }
    }
}
