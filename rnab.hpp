#include <boost/unordered/unordered_flat_map.hpp>
#include <pthread.h>
#include <time.h>

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

#ifndef GAME_CONSTANTS
#define GAME_CONSTANTS

#define MAX_MOVES 80

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

int cur_search_depth = 0;

struct alignas(8) field_t
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

        is_boost_available_fir = 1;
        is_boost_available_sec = 1;
        is_checker_available_fir = 1;
        is_checker_available_sec = 1;
        is_swap_available_fir = 1;
        is_swap_available_sec = 1;
        is_firewall_available_fir = 1;
        is_firewall_available_sec = 1;

        static const int init_pos_fir[8] = {63, 62, 61, 52, 51, 58, 57, 56};
        static const int init_pos_sec[8] = {0, 1, 2, 11, 12, 5, 6, 7};

        for (int i = 0; i < 8; ++i)
        {
            is_sec_mask |= (1ULL << init_pos_sec[i]);
            is_fir_mask |= (1ULL << init_pos_fir[i]);
            is_link_mask |= ((uint64_t)(((~pos_fir) >> i) & 1) << init_pos_fir[i]) | ((uint64_t)(((~pos_sec) >> i) & 1) << init_pos_sec[i]);
        }
    }

    int evaluate() const
    {
        return ((1024 << fir_link) - (2048 << fir_virus) - (1024 << sec_link) + (2048 << sec_virus)) + (int)forward_adv_fir - (int)forward_adv_sec + 2048 * (int)is_swap_available_fir - 2048 * (int)is_swap_available_sec;
    }

    size_t operator()(const field_t &s) const
    {
        return s.is_fir_mask | s.is_sec_mask;
    }

    // Those are actually faster (and smaller) than the standart implementations!

    inline __attribute__((always_inline)) bool operator==(const field_t &other) const
    {
        return __builtin_memcmp(this, &other, sizeof(field_t)) == 0;
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

struct minimax_main_result_t
{
    field_t best_field;
    int evaluation;
};

struct worker_t
{
    field_t *field_t;
    pthread_mutex_t *mtx;
    int score;
    int depth;
    int alpha;
    int beta;
    bool printing;
};

struct ttentry_t
{
    int score;
    int flag;
#ifdef CACHE_DEBUG
    int cache_id;
#endif
};

struct possible_moves_t
{
    field_t moves[MAX_MOVES];
    int moves_count;
};

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

possible_moves_t possible_moves(const field_t &position, const bool player)
{
    possible_moves_t res{.moves_count = 0};

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
                    temp_field.is_boost_available_sec |= (((temp_field.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                     \
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
                    int reschild = (depth > 0) ? minimax(depth, alpha, beta, false, temp_field, cache) : temp_field.evaluate();                                        \
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
                    temp_field.is_boost_available_sec |= (((temp_field.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                     \
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
                        reschild = (depth > 0) ? minimax(depth, alpha, beta, false, temp_field, cache) : temp_field.evaluate();                                        \
                        BRANCH_EXIT_MAX();                                                                                                                             \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        BRANCH_ENTER_MAX("capture virus not boosted");                                                                                                 \
                        if (depth > 0)                                                                                                                                 \
                        {                                                                                                                                              \
                            reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);                                                                     \
                            if (reschild > alpha && beta > alpha + 1)                                                                                                  \
                                reschild = minimax(depth, alpha, beta, false, temp_field, cache);                                                                      \
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
                    reschild = (depth > 0) ? minimax(depth, alpha, beta, false, temp_field, cache) : temp_field.evaluate();                                            \
                    BRANCH_EXIT_MAX();                                                                                                                                 \
                }                                                                                                                                                      \
                else                                                                                                                                                   \
                {                                                                                                                                                      \
                    BRANCH_ENTER_MAX("move not boosted");                                                                                                              \
                    if (depth > 0)                                                                                                                                     \
                    {                                                                                                                                                  \
                        reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);                                                                         \
                        if (reschild > alpha && beta > alpha + 1)                                                                                                      \
                            reschild = minimax(depth, alpha, beta, false, temp_field, cache);                                                                          \
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
                    temp_field.is_boost_available_fir |= (((temp_field.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                     \
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
                    int reschild = (depth > 0) ? minimax(depth, alpha, beta, true, temp_field, cache) : temp_field.evaluate();                                         \
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
                    temp_field.is_boost_available_fir |= (((temp_field.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                                     \
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
                        reschild = (depth > 0) ? minimax(depth, alpha, beta, true, temp_field, cache) : temp_field.evaluate();                                         \
                        BRANCH_EXIT_MIN();                                                                                                                             \
                    }                                                                                                                                                  \
                    else                                                                                                                                               \
                    {                                                                                                                                                  \
                        BRANCH_ENTER_MIN("capture virus not boosted");                                                                                                 \
                        if (depth > 0)                                                                                                                                 \
                        {                                                                                                                                              \
                            reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);                                                                        \
                            if (reschild < beta && alpha < beta - 1)                                                                                                   \
                                reschild = minimax(depth, alpha, beta, true, temp_field, cache);                                                                       \
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
                    reschild = (depth > 0) ? minimax(depth, alpha, beta, true, temp_field, cache) : temp_field.evaluate();                                             \
                    BRANCH_EXIT_MIN();                                                                                                                                 \
                }                                                                                                                                                      \
                else                                                                                                                                                   \
                {                                                                                                                                                      \
                    BRANCH_ENTER_MIN("move not boosted");                                                                                                              \
                    if (depth > 0)                                                                                                                                     \
                    {                                                                                                                                                  \
                        reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);                                                                            \
                        if (reschild < beta && alpha < beta - 1)                                                                                                       \
                            reschild = minimax(depth, alpha, beta, true, temp_field, cache);                                                                           \
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

static cutoff_tracker_t cutoff_tracker[1000] = {0};

#define BEGIN_BRANCH_TRACKING() \
    static constexpr int _branch_counter_base = __COUNTER__

#define BRANCH_ENTER_MAX(MSG)                                                             \
    static constexpr int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
    const int64_t cur_rec_count_##__LINE__ = rec_counter;                                 \
    cutoff_tracker[_branch_idx_##__LINE__].total_entries++;                               \
    cutoff_tracker[_branch_idx_##__LINE__].msg = MSG;                                     \
    cutoff_tracker[_branch_idx_##__LINE__].cutoff_entries++;                              \
    cutoff_tracker[_branch_idx_##__LINE__].temp_score = alpha

#define BRANCH_ENTER_MIN(MSG)                                                             \
    static constexpr int _branch_idx_##__LINE__ = __COUNTER__ - _branch_counter_base - 1; \
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

#ifdef CACHE_DEBUG

typedef struct
{
    int64_t total_entries;
    int64_t lookup_entries;
} cache_tracker_t;

static cache_tracker_t cache_entry_tracker[1000] = {0};

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

int64_t rec_counter = 0;

int minimax(int depth, int alpha, int beta, const bool player, const field_t &position, boost::unordered_flat_map<field_t, ttentry_t, field_t> *cache)
{
#ifdef BRANCH_DEBUG
    ++rec_counter;
#endif

    --depth;

    const uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
    const uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
    const uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
    const uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;

    if (player)
    {
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
                field_t temp_field = position;

                temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp_field.is_boost_available_fir |= ((temp_field.is_boosted_mask >> __builtin_ctzll(8ULL)) & 1);
                temp_field.is_boosted_mask &= ~8ULL;
                temp_field.is_fir_mask &= ~8ULL;
                temp_field.is_link_mask &= ~8ULL;
                ++temp_field.fir_link;

                BRANCH_ENTER_MAX("deposit close");
                int reschild = (depth > 0) ? minimax(depth, alpha, beta, false, temp_field, cache) : temp_field.evaluate();
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
                temp_field.is_boost_available_fir |= ((temp_field.is_boosted_mask >> __builtin_ctzll(16ULL)) & 1);
                temp_field.is_boosted_mask &= ~16ULL;
                temp_field.is_fir_mask &= ~16ULL;
                temp_field.is_link_mask &= ~16ULL;
                ++temp_field.fir_link;

                BRANCH_ENTER_MAX("deposit close");
                int reschild = (depth > 0) ? minimax(depth, alpha, beta, false, temp_field, cache) : temp_field.evaluate();
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
            field_t temp_field = position;

            uint64_t boosted_card = position.is_fir_mask & position.is_boosted_mask;

            temp_field.forward_adv_fir -= 7 - (__builtin_ctzll(boosted_card) >> 3);
            temp_field.is_boost_available_fir = 1;
            temp_field.is_boosted_mask &= ~boosted_card;
            temp_field.is_fir_mask &= ~boosted_card;
            temp_field.is_link_mask &= ~boosted_card;
            ++temp_field.fir_link;

            BRANCH_ENTER_MAX("deposit far");
            int reschild = (depth > 0) ? minimax(depth, alpha, beta, false, temp_field, cache) : temp_field.evaluate();
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

                BRANCH_ENTER_MAX("boost virus");
                int reschild = (depth > 0) ? minimax(depth, alpha, beta, false, temp_field, cache) : temp_field.evaluate();
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

                BRANCH_ENTER_MAX("firewall link");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);
                    if (reschild > alpha && beta > alpha + 1)
                        reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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

            temp = (fir_virus_mask & 16717361816799281127ULL) & position.is_boosted_mask;

            if (temp)
            {
                const int bit_pos = __builtin_ctzll(temp);
                const uint64_t pos = (1ULL << bit_pos); // front -> back

                field_t temp_field = position;

                temp_field.is_firewall_available_fir = 0;
                temp_field.firewall_fir = bit_pos;

                BRANCH_ENTER_MAX("firewall boosted virus");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);
                    if (reschild > alpha && beta > alpha + 1)
                        reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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
        else if (position.is_firewall_available_fir == 0)
        {
            field_t temp_field = position;

            temp_field.is_firewall_available_fir = 1;

            BRANCH_ENTER_MAX("un-firewall");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);
                if (reschild > alpha && beta > alpha + 1)
                    reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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

                    BRANCH_ENTER_MAX("swap");
                    int reschild;
                    if (depth > 0)
                    {
                        reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);
                        if (reschild > alpha && beta > alpha + 1)
                            reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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

        if (position.is_boost_available_fir)
        {
            uint64_t temp = fir_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_fir = 0;

                BRANCH_ENTER_MAX("boost link");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);
                    if (reschild > alpha && beta > alpha + 1)
                        reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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

            temp_field.is_boost_available_fir = 1;
            temp_field.is_boosted_mask &= temp_field.is_sec_mask;

            BRANCH_ENTER_MAX("un-boost");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax(depth, alpha, alpha + 1, false, temp_field, cache);
                if (reschild > alpha && beta > alpha + 1)
                    reschild = minimax(depth, alpha, beta, false, temp_field, cache);
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
            TRACK_ENTRY_MAX_END();
        return alpha;
    }
    else
    {
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
                field_t temp_field = position;

                temp_field.forward_adv_sec -= (__builtin_ctzll(576460752303423488ULL) >> 3);
                temp_field.is_boost_available_sec |= ((temp_field.is_boosted_mask >> __builtin_ctzll(576460752303423488ULL)) & 1);
                temp_field.is_boosted_mask &= ~576460752303423488ULL;
                temp_field.is_sec_mask &= ~576460752303423488ULL;
                temp_field.is_link_mask &= ~576460752303423488ULL;
                ++temp_field.sec_link;

                BRANCH_ENTER_MIN("deposit close");
                int reschild = (depth > 0) ? minimax(depth, alpha, beta, true, temp_field, cache) : temp_field.evaluate();
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
                temp_field.is_boost_available_sec |= ((temp_field.is_boosted_mask >> __builtin_ctzll(1152921504606846976ULL)) & 1);
                temp_field.is_boosted_mask &= ~1152921504606846976ULL;
                temp_field.is_sec_mask &= ~1152921504606846976ULL;
                temp_field.is_link_mask &= ~1152921504606846976ULL;
                ++temp_field.sec_link;

                BRANCH_ENTER_MIN("deposit close");
                int reschild = (depth > 0) ? minimax(depth, alpha, beta, true, temp_field, cache) : temp_field.evaluate();
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
            field_t temp_field = position;

            uint64_t boosted_card = position.is_sec_mask & position.is_boosted_mask;

            temp_field.forward_adv_sec -= (__builtin_ctzll(boosted_card) >> 3);
            temp_field.is_boost_available_sec = 1;
            temp_field.is_boosted_mask &= ~boosted_card;
            temp_field.is_sec_mask &= ~boosted_card;
            temp_field.is_link_mask &= ~boosted_card;
            ++temp_field.sec_link;

            BRANCH_ENTER_MIN("deposit far");
            int reschild = (depth > 0) ? minimax(depth, alpha, beta, true, temp_field, cache) : temp_field.evaluate();
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

                BRANCH_ENTER_MIN("boost virus");
                int reschild = (depth > 0) ? minimax(depth, alpha, beta, true, temp_field, cache) : temp_field.evaluate();
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

                BRANCH_ENTER_MIN("firewall link");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);
                    if (reschild < beta && alpha < beta - 1)
                        reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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

            temp = (sec_virus_mask & 16717361816799281127ULL) & position.is_boosted_mask;

            if (temp)
            {
                const int bit_pos = __builtin_clzll(temp);
                const uint64_t pos = (1ULL << (63 - bit_pos)); // front -> back

                field_t temp_field = position;

                temp_field.is_firewall_available_sec = 0;
                temp_field.firewall_sec = (63 - bit_pos);

                BRANCH_ENTER_MIN("firewall virus");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);
                    if (reschild < beta && alpha < beta - 1)
                        reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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
        else if (position.is_firewall_available_sec == 0)
        {
            field_t temp_field = position;

            temp_field.is_firewall_available_sec = 1;

            BRANCH_ENTER_MIN("un-firewall");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);
                if (reschild < beta && alpha < beta - 1)
                    reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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

                    BRANCH_ENTER_MIN("swap");
                    int reschild;
                    if (depth > 0)
                    {
                        reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);
                        if (reschild < beta && alpha < beta - 1)
                            reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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

        if (position.is_boost_available_sec)
        {
            uint64_t temp = sec_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                field_t temp_field = position;

                temp_field.is_boosted_mask |= pos;
                temp_field.is_boost_available_sec = 0;

                BRANCH_ENTER_MIN("boost link");
                int reschild;
                if (depth > 0)
                {
                    reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);
                    if (reschild < beta && alpha < beta - 1)
                        reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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

            temp_field.is_boost_available_sec = 1;
            temp_field.is_boosted_mask &= temp_field.is_fir_mask;

            BRANCH_ENTER_MIN("un-boost");
            int reschild;
            if (depth > 0)
            {
                reschild = minimax(depth, beta - 1, beta, true, temp_field, cache);
                if (reschild < beta && alpha < beta - 1)
                    reschild = minimax(depth, alpha, beta, true, temp_field, cache);
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

void *maximize_worker(void *arg)
{
    worker_t *worker_data = (worker_t *)arg;

    boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[worker_data->depth];
    for (int i = 0; i < worker_data->depth; ++i)
        newcache[i].reserve(1024);

    struct timespec start, stop;

    clock_gettime(CLOCK_MONOTONIC, &start);
    worker_data->score = minimax(worker_data->depth - 1, worker_data->alpha, worker_data->alpha + 1, false, *worker_data->field_t, newcache);
    clock_gettime(CLOCK_MONOTONIC, &stop);

    if (worker_data->score > worker_data->alpha)
    {
        pthread_mutex_lock(worker_data->mtx);
        if (worker_data->printing)
            debug_printf("Maximize first minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->alpha, worker_data->score);
        pthread_mutex_unlock(worker_data->mtx);
        clock_gettime(CLOCK_MONOTONIC, &start);
        worker_data->score = minimax(worker_data->depth - 1, worker_data->score, worker_data->beta, false, *worker_data->field_t, newcache);
        clock_gettime(CLOCK_MONOTONIC, &stop);
        pthread_mutex_lock(worker_data->mtx);
        if (worker_data->printing)
            debug_printf("Maximize second minimax call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->alpha, worker_data->score);
        pthread_mutex_unlock(worker_data->mtx);
    }
    else
    {
        pthread_mutex_lock(worker_data->mtx);
        if (worker_data->printing)
            debug_printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->alpha, worker_data->score);
        pthread_mutex_unlock(worker_data->mtx);
    }
    return NULL;
}

void *minimize_worker(void *arg)
{
    worker_t *worker_data = (worker_t *)arg;

    boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[worker_data->depth];
    for (int i = 0; i < worker_data->depth; ++i)
        newcache[i].reserve(1024);

    struct timespec start, stop;

    clock_gettime(CLOCK_MONOTONIC, &start);
    worker_data->score = minimax(worker_data->depth - 1, worker_data->beta - 1, worker_data->beta, true, *worker_data->field_t, newcache);
    clock_gettime(CLOCK_MONOTONIC, &stop);

    if (worker_data->score < worker_data->beta)
    {
        pthread_mutex_lock(worker_data->mtx);
        if (worker_data->printing)
            debug_printf("Minimize first minimax improv call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->beta, worker_data->score);
        pthread_mutex_unlock(worker_data->mtx);
        clock_gettime(CLOCK_MONOTONIC, &start);
        worker_data->score = minimax(worker_data->depth - 1, worker_data->alpha, worker_data->score, true, *worker_data->field_t, newcache);
        clock_gettime(CLOCK_MONOTONIC, &stop);
        pthread_mutex_lock(worker_data->mtx);
        if (worker_data->printing)
            debug_printf("Minimize second minimax call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->beta, worker_data->score);
        pthread_mutex_unlock(worker_data->mtx);
    }
    else
    {
        pthread_mutex_lock(worker_data->mtx);
        if (worker_data->printing)
            debug_printf("Minimize first minimax no-improv call time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), worker_data->beta, worker_data->score);
        pthread_mutex_unlock(worker_data->mtx);
    }
    return NULL;
}

int minimax_scout(const int cutoffdepth, const int depth, int alpha, int beta, const bool player, field_t &position)
{
    if (depth < cutoffdepth)
    {
        boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[depth];
        for (int i = 0; i < depth; ++i)
            newcache[i].reserve(1024);
        return minimax(depth, alpha, beta, player, position, newcache);
    }

    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

    if (player)
    {
        possible_moves_t all_moves = possible_moves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].fir_link > 3)
                return (32768 * depth);

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];
        alpha = minimax_scout(cutoffdepth, depth - 1, alpha, beta, false, all_moves.moves[0]);

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];
            args[i].printing = false;
            args[i].mtx = &mtx;

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
        possible_moves_t all_moves = possible_moves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].sec_link > 3)
                return (-32768 * depth);

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];
        beta = minimax_scout(cutoffdepth, depth - 1, alpha, beta, true, all_moves.moves[0]);

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];
            args[i].printing = false;
            args[i].mtx = &mtx;

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

    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

    if (player)
    {
        possible_moves_t all_moves = possible_moves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * depth)};

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];

        field_t bestfield = all_moves.moves[0];
        alpha = minimax_scout(depth - 5, depth - 1, alpha, beta, false, all_moves.moves[0]);

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];
            args[i].printing = true;
            args[i].mtx = &mtx;

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
        possible_moves_t all_moves = possible_moves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-32768 * depth)};

        pthread_t threads[all_moves.moves_count];
        worker_t args[all_moves.moves_count];

        field_t bestfield = all_moves.moves[0];
        beta = minimax_scout(depth - 5, depth - 1, alpha, beta, true, all_moves.moves[0]);

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field_t = &all_moves.moves[i];
            args[i].printing = true;
            args[i].mtx = &mtx;

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

minimax_main_result_t minimax_single_main(const int depth, int alpha, int beta, const bool player, field_t &position)
{
    cur_search_depth = depth;
    struct timespec start, stop;

    if (player)
    {
        possible_moves_t all_moves = possible_moves(position, true);
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
            clock_gettime(CLOCK_MONOTONIC, &start);

            int childres;
            if (is_first_move)
            {
                childres = minimax(depth - 1, alpha, beta, false, all_moves.moves[i], newcache);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                debug_printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
            }
            else
            {
                childres = minimax(depth - 1, alpha, alpha + 1, false, all_moves.moves[i], newcache);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                if (childres > alpha)
                {
                    debug_printf("Maximize first minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    childres = minimax(depth - 1, alpha, beta, false, all_moves.moves[i], newcache);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    debug_printf("Maximize second minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                }
                else
                {
                    debug_printf("Maximize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), alpha, childres);
                }
            }

            best_field = (childres > alpha) ? all_moves.moves[i] : best_field;
            alpha = (childres > alpha) ? childres : alpha;
            is_first_move = false;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = alpha};
    }
    else
    {
        possible_moves_t all_moves = possible_moves(position, false);
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
            clock_gettime(CLOCK_MONOTONIC, &start);
            int childres;

            if (is_first_move)
            {
                childres = minimax(depth - 1, alpha, beta, true, all_moves.moves[i], newcache);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                debug_printf("Minimize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
            }
            else
            {
                childres = minimax(depth - 1, beta - 1, beta, true, all_moves.moves[i], newcache);
                clock_gettime(CLOCK_MONOTONIC, &stop);
                if (childres < beta)
                {
                    debug_printf("Minimize first minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                    clock_gettime(CLOCK_MONOTONIC, &start);
                    childres = minimax(depth - 1, alpha, beta, true, all_moves.moves[i], newcache);
                    clock_gettime(CLOCK_MONOTONIC, &stop);
                    debug_printf("Minimize second minimax call improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
                }
                else
                {
                    debug_printf("Minimize first minimax call no-improv time: %ld ms, %d -> %d\n", (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000), beta, childres);
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
        possible_moves_t all_moves = possible_moves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * depth)};

        field_t best_field = all_moves.moves[0];
        int prev_alpha = alpha;

        std::pair<int, int> move_scores[MAX_MOVES];
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = {i, MIN};

        for (int current_depth = 2; current_depth <= depth; current_depth += 2)
        {
            int64_t cur_rec_count = rec_counter;
            clock_gettime(CLOCK_MONOTONIC, &start);

            boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[current_depth];
            for (int i = 0; i < current_depth; ++i)
                newcache[i].reserve(1024);

            if (current_depth > 2)
            {
                std::stable_sort(move_scores, move_scores + all_moves.moves_count,
                                 [](const auto &a, const auto &b)
                                 {
                                     if (a.second != b.second)
                                         return a.second > b.second;
                                     return a.first < b.first;
                                 });
            }

            int iteration_alpha = prev_alpha - 56;
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

            if (iteration_alpha == prev_alpha - 56)
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
                debug_printf("%d, ", move_scores[i].first);
            debug_printf("\n");

            clock_gettime(CLOCK_MONOTONIC, &stop);
            debug_printf("Depth %d completed in %ld ms, evaluation: %d, checked_pos: %ld, pos/ms: %f\n",
                         current_depth,
                         (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000),
                         iteration_alpha,
                         rec_counter - cur_rec_count,
                         (double)(rec_counter - cur_rec_count) / ((double)(stop.tv_sec * 1000000000 + stop.tv_nsec - start.tv_sec * 1000000000 - start.tv_nsec) / 1000000.0));

            prev_alpha = iteration_alpha;
            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_alpha};
        }

        return best_result;
    }
    else
    {
        possible_moves_t all_moves = possible_moves(position, false);
        for (int i = 0; i < all_moves.moves_count; ++i)
            if (all_moves.moves[i].sec_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (-32768 * depth)};

        field_t best_field = all_moves.moves[0];
        int prev_beta = beta;

        std::pair<int, int> move_scores[MAX_MOVES];
        for (int i = 0; i < all_moves.moves_count; ++i)
            move_scores[i] = {i, MAX};

        for (int current_depth = 2; current_depth <= depth; current_depth += 2)
        {
            int64_t cur_rec_count = rec_counter;
            clock_gettime(CLOCK_MONOTONIC, &start);

            boost::unordered_flat_map<field_t, ttentry_t, field_t> newcache[current_depth];
            for (int i = 0; i < current_depth; ++i)
                newcache[i].reserve(1024);

            if (current_depth > 2)
            {
                std::stable_sort(move_scores, move_scores + all_moves.moves_count,
                                 [](const auto &a, const auto &b)
                                 {
                                     if (a.second != b.second)
                                         return a.second < b.second;
                                     return a.first < b.first;
                                 });
            }

            int iteration_beta = prev_beta + 56;
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

            if (iteration_beta == prev_beta + 56)
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
                debug_printf("%d, ", move_scores[i].first);
            debug_printf("\n");

            clock_gettime(CLOCK_MONOTONIC, &stop);
            debug_printf("Depth %d completed in %ld ms, evaluation: %d, checked_pos: %ld, pos/ms: %f\n",
                         current_depth,
                         (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000),
                         iteration_beta,
                         rec_counter - cur_rec_count,
                         (double)(rec_counter - cur_rec_count) / ((double)(stop.tv_sec * 1000000000 + stop.tv_nsec - start.tv_sec * 1000000000 - start.tv_nsec) / 1000000.0));

            prev_beta = iteration_beta;
            best_result = (minimax_main_result_t){.best_field = best_field, .evaluation = iteration_beta};
        }

        return best_result;
    }
}
