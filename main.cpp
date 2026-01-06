#include <ankerl/unordered_dense.h>
#include <pthread.h>
#include <time.h>

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

const int init_pos_fir[8] = {63, 62, 61, 52, 51, 58, 57, 56};
const int init_pos_sec[8] = {0, 1, 2, 11, 12, 5, 6, 7};

#define MIN -1000000
#define MAX 1000000

#define MIN_CACHE_DEPTH 2

#define ITERATION_CURRENT_IS_FIRST_UNKNOWN 0
#define ITERATION_CURRENT_IS_FIRST_LINK 1
#define ITERATION_CURRENT_IS_FIRST_VIRUS 2
#define ITERATION_CURRENT_IS_SECOND_UNKNOWN 3
#define ITERATION_CURRENT_IS_SECOND_LINK 4
#define ITERATION_CURRENT_IS_SECOND_VIRUS 5

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

    int evaluate() // maximize
    {
        return ((1024 << fir_link) - (2048 << fir_virus) - (1024 << sec_link) + (2048 << sec_virus)) + (int)forward_adv_fir - (int)forward_adv_sec;
    }

    size_t operator()(const field_t &s) const
    {
        return s.is_fir_mask | s.is_sec_mask;
    }

    bool operator==(const field_t &) const = default;

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

typedef struct
{
    int score;
    int flag;
} ttentry_t;

typedef struct
{
    field_t moves[56];
    int moves_count;
} possible_moves_t;

typedef struct
{
    field_t best_field;
    int evaluation;
} minimax_main_result_t;

typedef struct
{
    field_t *field;
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

#define PERFORM_ITERATION(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                          \
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
    }

possible_moves_t possiblemoves(field_t &position, const bool player)
{
    possible_moves_t res;
    res.moves_count = 0;

    uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
    uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
    uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
    uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;

    if (player)
    {
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

        if (position.is_boost_available_fir)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                uint64_t pos = (1ULL << __builtin_ctzll(temp));

                position.is_boosted_mask ^= pos;
                position.is_boost_available_fir = 0;
                res.moves[res.moves_count++] = position;
                position.is_boost_available_fir = 1;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }

            temp = fir_link_mask;

            while (temp)
            {
                uint64_t pos = (1ULL << __builtin_ctzll(temp));

                position.is_boosted_mask ^= pos;
                position.is_boost_available_fir = 0;
                res.moves[res.moves_count++] = position;
                position.is_boost_available_fir = 1;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }
        }

        const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;

        if (position.is_firewall_available_sec)
        {
            if (position.is_boost_available_fir == 0)
            {
                const uint64_t cur_pos_bitboard = firmask & position.is_boosted_mask;
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8); // double forward
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // forward left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // double right
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // backwards left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = (cur_pos_bitboard << 8); // double backwards
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                new_pos_bitboard = (cur_pos_bitboard << 8);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                temp ^= cur_pos_bitboard;
            }

            temp = fir_link_mask & (~position.is_boosted_mask);

            while (temp)
            {
                const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));
                const uint64_t other = firmask | secmask;

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

            if (position.is_boost_available_fir == 0)
            {
                field_t temp = position;

                temp.is_boost_available_fir = 1;
                temp.is_boosted_mask &= temp.is_sec_mask;

                res.moves[res.moves_count++] = temp;
            }
        }
        else
        {
            // todo
        }
    }
    else
    {
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

        if (position.is_boost_available_sec)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                position.is_boosted_mask ^= pos;
                position.is_boost_available_sec = 0;
                res.moves[res.moves_count++] = position;
                position.is_boost_available_sec = 1;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }

            temp = sec_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                position.is_boosted_mask ^= pos;
                position.is_boost_available_sec = 0;
                res.moves[res.moves_count++] = position;
                position.is_boost_available_sec = 1;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }
        }

        const uint64_t firmask = (fir_link_mask | fir_virus_mask), secmask = (sec_link_mask | sec_virus_mask);
        if (position.is_firewall_available_fir)
        {
            if (position.is_boost_available_sec == 0)
            {
                const uint64_t cur_pos_bitboard = secmask & position.is_boosted_mask;
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard << 8); // double forward
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // forward left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // double right
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // backwards left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = (cur_pos_bitboard >> 8); // double backwards
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
                }

                new_pos_bitboard = (cur_pos_bitboard >> 8);
                if (new_pos_bitboard)
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

            if (position.is_boost_available_sec == 0)
            {
                field_t temp = position;

                temp.is_boost_available_sec = 1;
                temp.is_boosted_mask &= temp.is_fir_mask;

                res.moves[res.moves_count++] = temp;
            }
        }
        else
        {
            // todo
        }
    }

    return res;
}

// inline void minimaxfullfir(int &reschild, int &depth, field_t &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field_t, ttentry_t, field_t>> &cache)
// {
//     if (depth == 0)
//         reschild = position.evaluate_fir();
//     else
//         reschild = minimax(depth, alpha, beta, true, position, cache);
// }

// inline void minimaxscoutfir(int &reschild, int &depth, field_t &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field_t, ttentry_t, field_t>> &cache)
// {
//     if (depth == 0)
//         reschild = position.evaluate_fir();
//     else
//     {
//         if (beta < MAX)
//         {
//             reschild = minimax(depth, beta - 1, beta, true, position, cache);
//             if (reschild > alpha && reschild < beta)
//                 reschild = minimax(depth, alpha, reschild, true, position, cache);
//         }
//         else
//             reschild = minimax(depth, alpha, beta, true, position, cache);
//     }
// }

// inline void minimaxfullsec(int &reschild, int &depth, field_t &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field_t, ttentry_t, field_t>> &cache)
// {
//     if (depth == 0)
//         reschild = position.evaluate_sec();
//     else
//         reschild = minimax(depth, alpha, beta, false, position, cache);
// }

// inline void minimaxscoutsec(int &reschild, int &depth, field_t &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field_t, ttentry_t, field_t>> &cache)
// {
//     if (depth == 0)
//         reschild = position.evaluate_sec();
//     else
//     {
//         if (alpha > MIN && depth > 0)
//         {
//             reschild = minimax(depth, alpha, alpha + 1, false, position, cache);
//             if (reschild > alpha && reschild < beta)
//                 reschild = minimax(depth, reschild, beta, false, position, cache);
//         }
//         else
//             reschild = minimax(depth, alpha, beta, false, position, cache);
//     }
// }

#define PERFORM_ITERATION(shift_func, shift_count, forward_adv, is_boosted, current_card)                                                                          \
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
                int reschild = minimax(depth, alpha, beta, false, temp, cache);                                                                                    \
                alpha = (reschild > alpha) ? reschild : alpha;                                                                                                     \
                if (beta <= alpha)                                                                                                                                 \
                {                                                                                                                                                  \
                    if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                        cache[depth][position] = {alpha, 0};                                                                                                       \
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
                int reschild = minimax(depth, alpha, beta, false, temp, cache);                                                                                    \
                alpha = (reschild > alpha) ? reschild : alpha;                                                                                                     \
                if (beta <= alpha)                                                                                                                                 \
                {                                                                                                                                                  \
                    if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                        cache[depth][position] = {alpha, 0};                                                                                                       \
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
            int reschild = minimax(depth, alpha, beta, false, temp, cache);                                                                                        \
            alpha = (reschild > alpha) ? reschild : alpha;                                                                                                         \
            if (beta <= alpha)                                                                                                                                     \
            {                                                                                                                                                      \
                if (depth > MIN_CACHE_DEPTH)                                                                                                                       \
                    cache[depth][position] = {alpha, 0};                                                                                                           \
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
                temp.is_boost_available_fir |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                     \
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
                int reschild = minimax(depth, alpha, beta, true, temp, cache);                                                                                     \
                beta = (reschild < beta) ? reschild : beta;                                                                                                        \
                if (beta <= alpha)                                                                                                                                 \
                {                                                                                                                                                  \
                    if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                        cache[depth][position] = {beta, 0};                                                                                                        \
                    return beta;                                                                                                                                   \
                }                                                                                                                                                  \
            }                                                                                                                                                      \
            else if (position.sec_virus < 3)                                                                                                                       \
            {                                                                                                                                                      \
                field_t temp = position;                                                                                                                           \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.is_boost_available_fir |= (((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1);                     \
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
                int reschild = minimax(depth, alpha, beta, true, temp, cache);                                                                                     \
                beta = (reschild < beta) ? reschild : beta;                                                                                                        \
                if (beta <= alpha)                                                                                                                                 \
                {                                                                                                                                                  \
                    if (depth > MIN_CACHE_DEPTH)                                                                                                                   \
                        cache[depth][position] = {beta, 0};                                                                                                        \
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
            int reschild = minimax(depth, alpha, beta, true, temp, cache);                                                                                         \
            beta = (reschild < beta) ? reschild : beta;                                                                                                            \
            if (beta <= alpha)                                                                                                                                     \
            {                                                                                                                                                      \
                if (depth > MIN_CACHE_DEPTH)                                                                                                                       \
                    cache[depth][position] = {beta, 0};                                                                                                            \
                return beta;                                                                                                                                       \
            }                                                                                                                                                      \
        }                                                                                                                                                          \
    }

int minimax(int depth, int alpha, int beta, const bool player, field_t &position, ankerl::unordered_dense::map<field_t, ttentry_t, field_t> *cache)
{
    if (player)
    {
        if (depth == 0)
            return position.evaluate();
        --depth;

        uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
        uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
        uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
        uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;

        if (position.fir_link == 3)
        {
            if (fir_link_mask & 24)
                return (32768 * (depth + 1));
            if (sec_link_mask)
            {
                const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;
                if (((sec_link_mask >> 8) & firmask) ||                             // up
                    ((sec_link_mask << 8) & firmask) ||                             // down
                    (((sec_link_mask & 18374403900871474942ULL) >> 1) & firmask) || // left
                    (((sec_link_mask & 9187201950435737471ULL) << 1) & firmask))    // right
                    return (32768 * (depth + 1));
                if (position.is_boost_available_fir == 0)
                {
                    uint64_t boosted_mask = position.is_boosted_mask & firmask;
                    uint64_t other_mask = sec_virus_mask | firmask;

                    if ((((sec_link_mask >> 16) & boosted_mask) && ((other_mask >> 8) & boosted_mask) == 0) ||                                                                                                     // up and not blocked
                        ((sec_link_mask << 16) & boosted_mask && ((other_mask << 8) & boosted_mask) == 0) ||                                                                                                       // down and not blocked
                        ((((sec_link_mask & 18229723555195321596ULL) >> 2) & boosted_mask) && (((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0) ||                                              // left and not blocked
                        ((((sec_link_mask & 4557430888798830399ULL) << 2) & boosted_mask) && (((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0) ||                                                // right and not blocked
                        ((((sec_link_mask & 18374403900871474942ULL) >> 9) & boosted_mask) && ((((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((other_mask >> 8) & boosted_mask) == 0)) || // up left
                        ((((sec_link_mask & 18374403900871474942ULL) << 7) & boosted_mask) && ((((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((other_mask << 8) & boosted_mask) == 0)) || // down left
                        ((((sec_link_mask & 9187201950435737471ULL) << 9) & boosted_mask) && ((((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((other_mask << 8) & boosted_mask) == 0)) ||   // down right
                        ((((sec_link_mask & 9187201950435737471ULL) >> 7) & boosted_mask) && ((((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((other_mask >> 8) & boosted_mask) == 0)))     // up right
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

                int reschild = minimax(depth, alpha, beta, false, temp, cache);
                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {alpha, 0};
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

                int reschild = minimax(depth, alpha, beta, false, temp, cache);
                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }
            }
        }

        if (position.is_boost_available_fir)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                position.is_boosted_mask ^= pos;
                position.is_boost_available_fir = 0;
                int reschild = minimax(depth, alpha, beta, false, position, cache);
                position.is_boost_available_fir = 1;
                position.is_boosted_mask ^= pos;

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }

                temp ^= pos;
            }

            temp = fir_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                position.is_boosted_mask ^= pos;
                position.is_boost_available_fir = 0;
                int reschild = minimax(depth, alpha, beta, false, position, cache);
                position.is_boost_available_fir = 1;
                position.is_boosted_mask ^= pos;

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }

                temp ^= pos;
            }
        }

        const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;

        if (position.is_firewall_available_sec)
        {
            if (position.is_boost_available_fir == 0)
            {
                const uint64_t cur_pos_bitboard = firmask & position.is_boosted_mask;
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8); // double forward
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 7, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // forward left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 9, 1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // double right
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 9, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // backwards left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 7, -1, true, ITERATION_CURRENT_IS_FIRST_UNKNOWN)
                    }
                }

                new_pos_bitboard = (cur_pos_bitboard << 8); // double backwards
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard >> 8);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 8, 1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                new_pos_bitboard = (cur_pos_bitboard << 8);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 8, -1, false, ITERATION_CURRENT_IS_FIRST_VIRUS)
                }

                temp ^= cur_pos_bitboard;
            }

            temp = fir_link_mask & (~position.is_boosted_mask);

            while (temp)
            {
                const uint64_t cur_pos_bitboard = (1ULL << __builtin_ctzll(temp));
                const uint64_t other = firmask | secmask;

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

            if (position.is_boost_available_fir == 0)
            {
                field_t temp = position;

                temp.is_boost_available_fir = 1;
                temp.is_boosted_mask &= temp.is_sec_mask;

                int reschild = minimax(depth, alpha, beta, false, temp, cache);
                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }
            }
        }
        else
        {
            // todo
        }
        if (depth > MIN_CACHE_DEPTH)
            cache[depth][position] = {alpha, (alpha > alphabeg) ? 3 : 1};
        return alpha;
    }
    else
    {
        if (depth == 0)
            return position.evaluate();
        --depth;

        uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
        uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
        uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
        uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;

        if (position.sec_link == 3)
        {
            if (sec_link_mask & 1729382256910270464ULL)
                return (-32768 * (depth + 1));
            if (fir_link_mask)
            {
                const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;
                if (((fir_link_mask >> 8) & secmask) ||                             // up
                    ((fir_link_mask << 8) & secmask) ||                             // down
                    (((fir_link_mask & 18374403900871474942ULL) >> 1) & secmask) || // left
                    (((fir_link_mask & 9187201950435737471ULL) << 1) & secmask))    // right
                    return (-32768 * (depth + 1));
                if (position.is_boost_available_sec == 0)
                {
                    uint64_t boosted_mask = position.is_boosted_mask & secmask;
                    uint64_t other_mask = fir_virus_mask | secmask;

                    if ((((fir_link_mask >> 16) & boosted_mask) && ((other_mask >> 8) & boosted_mask) == 0) ||                                                                                                     // up and not blocked
                        ((fir_link_mask << 16) & boosted_mask && ((other_mask << 8) & boosted_mask) == 0) ||                                                                                                       // down and not blocked
                        ((((fir_link_mask & 18229723555195321596ULL) >> 2) & boosted_mask) && (((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0) ||                                              // left and not blocked
                        ((((fir_link_mask & 4557430888798830399ULL) << 2) & boosted_mask) && (((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0) ||                                                // right and not blocked
                        ((((fir_link_mask & 18374403900871474942ULL) >> 9) & boosted_mask) && ((((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((other_mask >> 8) & boosted_mask) == 0)) || // up left
                        ((((fir_link_mask & 18374403900871474942ULL) << 7) & boosted_mask) && ((((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0 || ((other_mask << 8) & boosted_mask) == 0)) || // down left
                        ((((fir_link_mask & 9187201950435737471ULL) << 9) & boosted_mask) && ((((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((other_mask << 8) & boosted_mask) == 0)) ||   // down right
                        ((((fir_link_mask & 9187201950435737471ULL) >> 7) & boosted_mask) && ((((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0 || ((other_mask >> 8) & boosted_mask) == 0)))     // up right
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

                int reschild = minimax(depth, alpha, beta, true, temp, cache);
                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {beta, 0};
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

                int reschild = minimax(depth, alpha, beta, true, temp, cache);
                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }
            }
        }

        if (position.is_boost_available_sec)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                position.is_boosted_mask ^= pos;
                position.is_boost_available_sec = 0;
                int reschild = minimax(depth, alpha, beta, true, position, cache);
                position.is_boost_available_sec = 1;
                position.is_boosted_mask ^= pos;

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }

                temp ^= pos;
            }

            temp = sec_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                position.is_boosted_mask ^= pos;
                position.is_boost_available_sec = 0;
                int reschild = minimax(depth, alpha, beta, true, position, cache);
                position.is_boost_available_sec = 1;
                position.is_boosted_mask ^= pos;

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }

                temp ^= pos;
            }
        }

        const uint64_t firmask = (fir_link_mask | fir_virus_mask), secmask = (sec_link_mask | sec_virus_mask);
        if (position.is_firewall_available_fir)
        {
            if (position.is_boost_available_sec == 0)
            {
                const uint64_t cur_pos_bitboard = secmask & position.is_boosted_mask;
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard << 8); // double forward
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 7, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // forward left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard << 8)) == 0)
                {
                    new_pos_bitboard <<= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(<<, 9, 1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1); // double right
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 9, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1); // backwards left
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0 || (other & (cur_pos_bitboard >> 8)) == 0)
                {
                    new_pos_bitboard >>= 8;
                    if (new_pos_bitboard)
                    {
                        PERFORM_ITERATION(>>, 7, -1, true, ITERATION_CURRENT_IS_SECOND_UNKNOWN)
                    }
                }

                new_pos_bitboard = (cur_pos_bitboard >> 8); // double backwards
                if (new_pos_bitboard && (other & new_pos_bitboard) == 0)
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
                const uint64_t other = firmask | secmask;

                uint64_t new_pos_bitboard = (cur_pos_bitboard << 8);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 8, 1, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 18374403900871474942ULL) >> 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(>>, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
                }

                new_pos_bitboard = ((cur_pos_bitboard & 9187201950435737471ULL) << 1);
                if (new_pos_bitboard)
                {
                    PERFORM_ITERATION(<<, 1, 0, false, ITERATION_CURRENT_IS_SECOND_VIRUS)
                }

                new_pos_bitboard = (cur_pos_bitboard >> 8);
                if (new_pos_bitboard)
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

            if (position.is_boost_available_sec == 0)
            {
                field_t temp = position;

                temp.is_boost_available_sec = 1;
                temp.is_boosted_mask &= temp.is_fir_mask;

                int reschild = minimax(depth, alpha, beta, true, temp, cache);
                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > MIN_CACHE_DEPTH)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }
            }
        }
        else
        {
            // todo
        }
        if (depth > MIN_CACHE_DEPTH)
            cache[depth][position] = {beta, (beta < betabeg) ? 3 : 1};
        return beta;
    }
}

int cutoffdepth;

void *maximize_worker(void *arg)
{
    worker_t *worker_data = (worker_t *)arg;

    ankerl::unordered_dense::map<field_t, ttentry_t, field_t> newcache[worker_data->depth];
    for (int i = 0; i < worker_data->depth; ++i)
        newcache[i].reserve(1024);
    worker_data->score = minimax(worker_data->depth - 1, worker_data->alpha, worker_data->alpha + 1, false, *worker_data->field, newcache);
    if (worker_data->score > worker_data->alpha)
        worker_data->score = minimax(worker_data->depth - 1, worker_data->score, worker_data->beta, false, *worker_data->field, newcache);
    return NULL;
}

void *minimize_worker(void *arg)
{
    worker_t *worker_data = (worker_t *)arg;

    ankerl::unordered_dense::map<field_t, ttentry_t, field_t> newcache[worker_data->depth];
    for (int i = 0; i < worker_data->depth; ++i)
        newcache[i].reserve(1024);
    worker_data->score = minimax(worker_data->depth - 1, worker_data->beta - 1, worker_data->beta, true, *worker_data->field, newcache);
    if (worker_data->score < worker_data->beta)
        worker_data->score = minimax(worker_data->depth - 1, worker_data->alpha, worker_data->score, true, *worker_data->field, newcache);
    return NULL;
}

int minimax_scout(const int depth, int alpha, int beta, const bool player, field_t &position)
{
    if (depth < cutoffdepth)
    {
        ankerl::unordered_dense::map<field_t, ttentry_t, field_t> newcache[depth];
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

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field = &all_moves.moves[i];

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

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field = &all_moves.moves[i];

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

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field = &all_moves.moves[i];

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

        for (long i = 1; i < all_moves.moves_count; ++i)
        {
            args[i].alpha = alpha;
            args[i].beta = beta;
            args[i].depth = depth;
            args[i].field = &all_moves.moves[i];

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
    if (player)
    {
        possible_moves_t all_moves = possiblemoves(position, true);
        for (int i = 0; i < all_moves.moves_count; ++i) // check if there is a winning position
            if (all_moves.moves[i].fir_link > 3)
                return (minimax_main_result_t){.best_field = all_moves.moves[i], .evaluation = (32768 * depth)};

        field_t best_field = all_moves.moves[0];
        ankerl::unordered_dense::map<field_t, ttentry_t, field_t> newcache[depth];
        for (int i = 0; i < depth; ++i)
            newcache[i].reserve(1024);

        for (int i = 0; i < all_moves.moves_count; ++i)
        {
            int childres = minimax(depth - 1, alpha, beta, false, all_moves.moves[i], newcache);
            best_field = (childres > alpha) ? all_moves.moves[i] : best_field;
            alpha = (childres > alpha) ? childres : alpha;
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
        ankerl::unordered_dense::map<field_t, ttentry_t, field_t> newcache[depth];
        for (int i = 0; i < depth; ++i)
            newcache[i].reserve(1024);

        for (int i = 0; i < all_moves.moves_count; ++i)
        {
            int childres = minimax(depth - 1, alpha, beta, true, all_moves.moves[i], newcache);
            best_field = (childres < beta) ? all_moves.moves[i] : best_field;
            beta = (childres < beta) ? childres : beta;
        }

        return (minimax_main_result_t){.best_field = best_field, .evaluation = beta};
    }
}

int main()
{
    struct timespec start, stop;
    // srand(time(NULL));
    field_t pos;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int checksum = 0;
    for (int i = 0; i < 70; ++i)
    {
        struct timespec start_it, stop_it;
        int fir, sec;
        field_construct(pos, indexes[69], indexes[i]);
        clock_gettime(CLOCK_MONOTONIC, &start_it);
        minimax_main_result_t move = minimax_main(18, MIN, MAX, true, pos);
        clock_gettime(CLOCK_MONOTONIC, &stop_it);
        printf("%d     %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);
        fir = move.evaluation;
        checksum ^= (move.evaluation << (indexes[i] & 1));
        field_construct(pos, indexes[i], indexes[69]);
        clock_gettime(CLOCK_MONOTONIC, &start_it);
        move = minimax_main(18, MIN, MAX, false, pos);
        clock_gettime(CLOCK_MONOTONIC, &stop_it);
        printf("%d     %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);
        sec = move.evaluation;
        if (fir != -1 * sec)
        {
            printf("Eval error\n");
            exit(1);
        }
        checksum ^= (move.evaluation << (indexes[i] & 1));
        break;
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    printf("%ld\nhash: %d\n", (stop.tv_sec * 1000000000l + stop.tv_nsec - start.tv_sec * 1000000000l - start.tv_nsec) / 1000000, checksum);
    return 0;
    field_construct(pos, indexes[15], indexes[15]);
    pos.print_field();
    printf("\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;)
    {
        struct timespec start_it, stop_it;

        clock_gettime(CLOCK_MONOTONIC, &start_it);
        minimax_main_result_t move = minimax_main(18, MIN, MAX, true, pos);
        clock_gettime(CLOCK_MONOTONIC, &stop_it);

        printf("Minimized score: %d      %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);
        pos = move.best_field;
        pos.print_field();
        printf("\n");
        if (pos.sec_link == 4)
        {
            printf("Player one wins!\n");
            pos.print_field();
            break;
        }
        else if (pos.sec_virus == 4)
        {
            printf("Player one loses!\n");
            pos.print_field();
            break;
        }
        clock_gettime(CLOCK_MONOTONIC, &start_it);
        move = minimax_main(18, MIN, MAX, false, pos);
        clock_gettime(CLOCK_MONOTONIC, &stop_it);

        printf("Maximized score: %d      %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);
        pos = move.best_field;
        pos.print_field();
        printf("\n");
        if (pos.fir_link == 4)
        {
            printf("Player two wins!\n");
            pos.print_field();
            break;
        }
        else if (pos.fir_virus == 4)
        {
            printf("Player one loses!\n");
            pos.print_field();
            break;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    printf("%ld\n", (stop.tv_sec * 1000000000l + stop.tv_nsec - start.tv_sec * 1000000000l - start.tv_nsec) / 1000000);
}
