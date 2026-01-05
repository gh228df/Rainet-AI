#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <ankerl/unordered_dense.h>
#include <thread>
#include <mutex>
#include <assert.h>

using namespace std;
using namespace chrono;

const int indexes[70] = {15, 23, 27, 29, 30, 39, 43, 45, 46, 51, 53, 54, 57, 58, 60, 71, 75, 77, 78, 83, 85, 86, 89, 90, 92, 99, 101, 102, 105, 106, 108, 113, 114, 116, 120, 135, 139, 141, 142, 147, 149, 150, 153, 154, 156, 163, 165, 166, 169, 170, 172, 177, 178, 180, 184, 195, 197, 198, 201, 202, 204, 209, 210, 212, 216, 225, 226, 228, 232, 240};

const int init_pos_fir[8] = {63, 62, 61, 52, 51, 58, 57, 56};
const int init_pos_sec[8] = {0, 1, 2, 11, 12, 5, 6, 7};

#define MIN -1000000
#define MAX 1000000

#define STATE_IS_BOOST_AVAILABLE_FIR (1u)
#define STATE_IS_BOOST_AVAILABLE_FIR_SHIFT (0)
#define STATE_IS_BOOST_AVAILABLE_SEC (2u)
#define STATE_IS_BOOST_AVAILABLE_SEC_SHIFT (1)
#define STATE_IS_SWAP_AVAILABLE_FIR (4u)
#define STATE_IS_SWAP_AVAILABLE_FIR_SHIFT (2)
#define STATE_IS_SWAP_AVAILABLE_SEC (8u)
#define STATE_IS_SWAP_AVAILABLE_SEC_SHIFT (3)
#define STATE_IS_CHECKER_AVAILABLE_FIR (16u)
#define STATE_IS_CHECKER_AVAILABLE_FIR_SHIFT (4)
#define STATE_IS_CHECKER_AVAILABLE_SEC (32u)
#define STATE_IS_CHECKER_AVAILABLE_SEC_SHIFT (5)
#define STATE_IS_FIREWALL_AVAILABLE_FIR (64u)
#define STATE_IS_FIREWALL_AVAILABLE_FIR_SHIFT (6)
#define STATE_IS_FIREWALL_AVAILABLE_SEC (128u)
#define STATE_IS_FIREWALL_AVAILABLE_SEC_SHIFT (7)

#define ITERATION_CURRENT_IS_FIRST_UNKNOWN 0
#define ITERATION_CURRENT_IS_FIRST_LINK 1
#define ITERATION_CURRENT_IS_FIRST_VIRUS 2
#define ITERATION_CURRENT_IS_SECOND_UNKNOWN 3
#define ITERATION_CURRENT_IS_SECOND_LINK 4
#define ITERATION_CURRENT_IS_SECOND_VIRUS 5

struct ttentry
{
    int score;
    int flag;
};

struct field
{
    uint64_t is_fir_mask;
    uint64_t is_sec_mask;
    uint64_t is_link_mask;
    uint64_t is_boosted_mask;

    uint8_t fir_link;
    uint8_t sec_link;
    uint8_t fir_virus;
    uint8_t sec_virus;

    uint8_t forward_adv_fir;
    uint8_t forward_adv_sec;

    uint8_t firewall_fir;
    uint8_t firewall_sec;

    uint8_t state_mask;

    int evaluate_fir() // maximize
    {
        return (((int)fir_link << 10) - ((int)fir_virus << 11) - ((int)sec_link << 11) + ((int)sec_virus << 10)) + (int)forward_adv_fir;
    }
    int evaluate_sec() // minimize
    {
        return (((int)sec_virus << 11) - ((int)sec_link << 10) + ((int)fir_link << 11) - ((int)fir_virus << 10)) - (int)forward_adv_sec;
    }
    size_t operator()(const field &s) const
    {
        return s.is_fir_mask | s.is_sec_mask;
    }
    bool operator!=(const field &other) const
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
        if (state_mask != other.state_mask)
        {
            printf("Mismatch in state_mask\n");
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
    bool operator==(const field &other) const
    {
        return (is_sec_mask == other.is_sec_mask && is_link_mask == other.is_link_mask &&
                is_fir_mask == other.is_fir_mask && is_boosted_mask == other.is_boosted_mask &&
                state_mask == other.state_mask &&
                fir_virus == other.fir_virus &&
                fir_link == other.fir_link &&
                forward_adv_fir == other.forward_adv_fir &&
                forward_adv_sec == other.forward_adv_sec &&
                sec_virus == other.sec_virus &&
                sec_link == other.sec_link);
    }

    uint64_t reverse_mask(uint64_t mask)
    {
        uint64_t res = 0;

        while (mask)
        {
            int set_bit = __builtin_ctzll(mask);

            res |= (1ULL << (63 - set_bit));

            mask ^= (1ULL << set_bit);
        }

        return res;
    }

    field reverse_field()
    {
        field new_field;
        new_field.fir_link = sec_link;
        new_field.fir_virus = sec_virus;
        new_field.sec_link = fir_link;
        new_field.sec_virus = fir_virus;
        new_field.firewall_fir = firewall_sec;
        new_field.firewall_sec = firewall_fir;
        new_field.state_mask = 0;

        new_field.state_mask |= ((state_mask & STATE_IS_BOOST_AVAILABLE_FIR) >> STATE_IS_BOOST_AVAILABLE_FIR_SHIFT) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT;
        new_field.state_mask |= ((state_mask & STATE_IS_BOOST_AVAILABLE_SEC) >> STATE_IS_BOOST_AVAILABLE_SEC_SHIFT) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT;

        new_field.state_mask |= ((state_mask & STATE_IS_SWAP_AVAILABLE_FIR) >> STATE_IS_SWAP_AVAILABLE_FIR_SHIFT) << STATE_IS_SWAP_AVAILABLE_SEC_SHIFT;
        new_field.state_mask |= ((state_mask & STATE_IS_SWAP_AVAILABLE_SEC) >> STATE_IS_SWAP_AVAILABLE_SEC_SHIFT) << STATE_IS_SWAP_AVAILABLE_FIR_SHIFT;

        new_field.state_mask |= ((state_mask & STATE_IS_CHECKER_AVAILABLE_FIR) >> STATE_IS_CHECKER_AVAILABLE_FIR_SHIFT) << STATE_IS_CHECKER_AVAILABLE_SEC_SHIFT;
        new_field.state_mask |= ((state_mask & STATE_IS_CHECKER_AVAILABLE_SEC) >> STATE_IS_CHECKER_AVAILABLE_SEC_SHIFT) << STATE_IS_CHECKER_AVAILABLE_FIR_SHIFT;

        new_field.state_mask |= ((state_mask & STATE_IS_FIREWALL_AVAILABLE_FIR) >> STATE_IS_FIREWALL_AVAILABLE_FIR_SHIFT) << STATE_IS_FIREWALL_AVAILABLE_SEC_SHIFT;
        new_field.state_mask |= ((state_mask & STATE_IS_FIREWALL_AVAILABLE_SEC) >> STATE_IS_FIREWALL_AVAILABLE_SEC_SHIFT) << STATE_IS_FIREWALL_AVAILABLE_FIR_SHIFT;

        new_field.is_fir_mask = reverse_mask(is_sec_mask);
        new_field.is_sec_mask = reverse_mask(is_fir_mask);
        new_field.is_boosted_mask = reverse_mask(is_fir_mask & is_boosted_mask) | reverse_mask(is_sec_mask & is_boosted_mask);
        new_field.is_link_mask = reverse_mask(is_fir_mask & is_link_mask) | reverse_mask(is_sec_mask & is_link_mask);

        int sum = 0;
        uint64_t mask = new_field.is_fir_mask;

        while (mask)
        {
            int pos = __builtin_ctzll(mask);

            sum += 7 - (pos >> 3);

            mask ^= (1ULL << pos);
        }

        new_field.forward_adv_fir = sum;

        sum = 0;
        mask = new_field.is_sec_mask;

        while (mask)
        {
            int pos = __builtin_ctzll(mask);

            sum += (pos >> 3);

            mask ^= (1ULL << pos);
        }

        new_field.forward_adv_sec = sum;

        return new_field;
    }

    bool check_integrity()
    {
        if (((state_mask & STATE_IS_BOOST_AVAILABLE_FIR) == 0 && (is_fir_mask & is_boosted_mask) == 0) || ((state_mask & STATE_IS_BOOST_AVAILABLE_FIR) && (is_fir_mask & is_boosted_mask)))
        {
            printf("failed 5\n");
            return false;
        }

        if (((state_mask & STATE_IS_BOOST_AVAILABLE_SEC) == 0 && (is_sec_mask & is_boosted_mask) == 0) || ((state_mask & STATE_IS_BOOST_AVAILABLE_SEC) && (is_sec_mask & is_boosted_mask)))
        {
            printf("failed 6\n");
            return false;
        }

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

    void print_field()
    {
        // printf("fir_boost: %d, sec_boost: %d\n", (state_mask & STATE_IS_BOOST_AVAILABLE_FIR) >> STATE_IS_BOOST_AVAILABLE_FIR_SHIFT, (state_mask & STATE_IS_BOOST_AVAILABLE_SEC) >> STATE_IS_BOOST_AVAILABLE_SEC_SHIFT);
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

void field_construct(field &f, uint8_t pos_fir, uint8_t pos_sec)
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

    f.state_mask = 255;

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
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT);                     \
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
                nplusone.push_back(temp);                                                                                                                          \
            }                                                                                                                                                      \
            else if (position.fir_virus < 3)                                                                                                                       \
            {                                                                                                                                                      \
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT);                     \
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
                nplusone.push_back(temp);                                                                                                                          \
            }                                                                                                                                                      \
        }                                                                                                                                                          \
        else if ((firmask & new_pos_bitboard) == 0)                                                                                                                \
        {                                                                                                                                                          \
            field temp = position;                                                                                                                                 \
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
            nplusone.push_back(temp);                                                                                                                              \
        }                                                                                                                                                          \
    }                                                                                                                                                              \
    else                                                                                                                                                           \
    {                                                                                                                                                              \
        if (firmask & new_pos_bitboard)                                                                                                                            \
        {                                                                                                                                                          \
            if (fir_link_mask & new_pos_bitboard)                                                                                                                  \
            {                                                                                                                                                      \
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT);                     \
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
                nplusone.push_back(temp);                                                                                                                          \
            }                                                                                                                                                      \
            else if (position.sec_virus < 3)                                                                                                                       \
            {                                                                                                                                                      \
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT);                     \
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
                nplusone.push_back(temp);                                                                                                                          \
            }                                                                                                                                                      \
        }                                                                                                                                                          \
        else if ((secmask & new_pos_bitboard) == 0)                                                                                                                \
        {                                                                                                                                                          \
            field temp = position;                                                                                                                                 \
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
            nplusone.push_back(temp);                                                                                                                              \
        }                                                                                                                                                          \
    }

vector<field> possiblemoves(field &position, const bool player)
{
    vector<field> nplusone;
    nplusone.reserve(40);

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
                field temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(8ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT;
                temp.is_boosted_mask &= ~8ULL;
                temp.is_fir_mask &= ~8ULL;
                temp.is_link_mask &= ~8ULL;
                ++temp.fir_link;

                nplusone.push_back(temp);
            }
            if (fir_link_mask & 16ULL)
            {
                field temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(16ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT;
                temp.is_boosted_mask &= ~16ULL;
                temp.is_fir_mask &= ~16ULL;
                temp.is_link_mask &= ~16ULL;
                ++temp.fir_link;

                nplusone.push_back(temp);
            }
        }

        if (position.state_mask & STATE_IS_BOOST_AVAILABLE_FIR)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                uint64_t pos = (1ULL << __builtin_ctzll(temp));

                position.is_boosted_mask ^= pos;
                position.state_mask &= ~STATE_IS_BOOST_AVAILABLE_FIR;
                nplusone.push_back(position);
                position.state_mask |= STATE_IS_BOOST_AVAILABLE_FIR;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }

            temp = fir_link_mask;

            while (temp)
            {
                uint64_t pos = (1ULL << __builtin_ctzll(temp));

                position.is_boosted_mask ^= pos;
                position.state_mask &= ~STATE_IS_BOOST_AVAILABLE_FIR;
                nplusone.push_back(position);
                position.state_mask |= STATE_IS_BOOST_AVAILABLE_FIR;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }
        }

        const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;

        if (position.state_mask & STATE_IS_FIREWALL_AVAILABLE_SEC)
        {
            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_FIR) == 0)
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

            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_FIR) == 0)
            {
                field temp = position;

                temp.state_mask |= STATE_IS_BOOST_AVAILABLE_FIR;
                temp.is_boosted_mask &= temp.is_sec_mask;

                nplusone.push_back(temp);
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
                field temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(576460752303423488ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(576460752303423488ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT;
                temp.is_boosted_mask &= ~576460752303423488ULL;
                temp.is_sec_mask &= ~576460752303423488ULL;
                temp.is_link_mask &= ~576460752303423488ULL;
                ++temp.sec_link;

                nplusone.push_back(temp);
            }
            if (sec_link_mask & 1152921504606846976ULL)
            {
                field temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(1152921504606846976ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(1152921504606846976ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT;
                temp.is_boosted_mask &= ~1152921504606846976ULL;
                temp.is_sec_mask &= ~1152921504606846976ULL;
                temp.is_link_mask &= ~1152921504606846976ULL;
                ++temp.sec_link;

                nplusone.push_back(temp);
            }
        }

        if (position.state_mask & STATE_IS_BOOST_AVAILABLE_SEC)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                position.is_boosted_mask ^= pos;
                position.state_mask &= ~STATE_IS_BOOST_AVAILABLE_SEC;
                nplusone.push_back(position);
                position.state_mask |= STATE_IS_BOOST_AVAILABLE_SEC;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }

            temp = sec_link_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                position.is_boosted_mask ^= pos;
                position.state_mask &= ~STATE_IS_BOOST_AVAILABLE_SEC;
                nplusone.push_back(position);
                position.state_mask |= STATE_IS_BOOST_AVAILABLE_SEC;
                position.is_boosted_mask ^= pos;

                temp ^= pos;
            }
        }

        const uint64_t firmask = (fir_link_mask | fir_virus_mask), secmask = (sec_link_mask | sec_virus_mask);
        if (position.state_mask & STATE_IS_FIREWALL_AVAILABLE_FIR)
        {
            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_SEC) == 0)
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

            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_SEC) == 0)
            {
                field temp = position;

                temp.state_mask |= STATE_IS_BOOST_AVAILABLE_SEC;
                temp.is_boosted_mask &= temp.is_fir_mask;

                nplusone.push_back(temp);
            }
        }
        else
        {
            // todo
        }
    }

    return nplusone;
}

const int mincachedepth = 2, mincachedepthfull = 2, maxthreads = 50, mindepthformultithreadedsearch = 9;

int minimax(int depth, int alpha, int beta, const bool player, field &position, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache);

// inline void minimaxfullfir(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache)
// {
//     if (depth == 0)
//         reschild = position.evaluate_fir();
//     else
//         reschild = minimax(depth, alpha, beta, true, position, cache);
// }

// inline void minimaxscoutfir(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache)
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

// inline void minimaxfullsec(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache)
// {
//     if (depth == 0)
//         reschild = position.evaluate_sec();
//     else
//         reschild = minimax(depth, alpha, beta, false, position, cache);
// }

// inline void minimaxscoutsec(int &reschild, int &depth, field &position, int &alpha, int &beta, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache)
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
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT);                     \
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
                    if (depth > mincachedepth)                                                                                                                     \
                        cache[depth][position] = {alpha, 0};                                                                                                       \
                    return alpha;                                                                                                                                  \
                }                                                                                                                                                  \
            }                                                                                                                                                      \
            else if (position.fir_virus < 3)                                                                                                                       \
            {                                                                                                                                                      \
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT);                     \
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
                    if (depth > mincachedepth)                                                                                                                     \
                        cache[depth][position] = {alpha, 0};                                                                                                       \
                    return alpha;                                                                                                                                  \
                }                                                                                                                                                  \
            }                                                                                                                                                      \
        }                                                                                                                                                          \
        else if ((firmask & new_pos_bitboard) == 0)                                                                                                                \
        {                                                                                                                                                          \
            field temp = position;                                                                                                                                 \
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
                if (depth > mincachedepth)                                                                                                                         \
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
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT);                     \
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
                    if (depth > mincachedepth)                                                                                                                     \
                        cache[depth][position] = {beta, 0};                                                                                                        \
                    return beta;                                                                                                                                   \
                }                                                                                                                                                  \
            }                                                                                                                                                      \
            else if (position.sec_virus < 3)                                                                                                                       \
            {                                                                                                                                                      \
                field temp = position;                                                                                                                             \
                                                                                                                                                                   \
                int new_pos_coord = __builtin_ctzll(new_pos_bitboard);                                                                                             \
                temp.state_mask |= ((((temp.is_boosted_mask & new_pos_bitboard) >> new_pos_coord) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT);                     \
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
                    if (depth > mincachedepth)                                                                                                                     \
                        cache[depth][position] = {beta, 0};                                                                                                        \
                    return beta;                                                                                                                                   \
                }                                                                                                                                                  \
            }                                                                                                                                                      \
        }                                                                                                                                                          \
        else if ((secmask & new_pos_bitboard) == 0)                                                                                                                \
        {                                                                                                                                                          \
            field temp = position;                                                                                                                                 \
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
                if (depth > mincachedepth)                                                                                                                         \
                    cache[depth][position] = {beta, 0};                                                                                                            \
                return beta;                                                                                                                                       \
            }                                                                                                                                                      \
        }                                                                                                                                                          \
    }

int minimax(int depth, int alpha, int beta, const bool player, field &position, vector<ankerl::unordered_dense::map<field, ttentry, field>> &cache)
{
    uint64_t fir_link_mask = position.is_link_mask & position.is_fir_mask;
    uint64_t fir_virus_mask = position.is_fir_mask ^ fir_link_mask;
    uint64_t sec_link_mask = position.is_link_mask ^ fir_link_mask;
    uint64_t sec_virus_mask = position.is_sec_mask ^ sec_link_mask;

    if (player)
    {
        if (depth == 0)
            return position.evaluate_sec();
        --depth;

        if (position.fir_link == 3)
        {
            if (fir_link_mask & 24)
                return (16384 * depth);
            if (sec_link_mask)
            {
                const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;
                if (((sec_link_mask >> 8) & firmask) ||                             // up
                    ((sec_link_mask << 8) & firmask) ||                             // down
                    (((sec_link_mask & 18374403900871474942ULL) >> 1) & firmask) || // left
                    (((sec_link_mask & 9187201950435737471ULL) << 1) & firmask))    // right
                    return (16384 * depth);
                if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_FIR) == 0)
                {
                    uint64_t boosted_mask = position.is_boosted_mask & firmask;
                    uint64_t other_mask = sec_virus_mask | firmask;

                    if ((((sec_link_mask >> 16) & boosted_mask) && ((other_mask >> 8) & boosted_mask) == 0) ||                                                        // up and not blocked
                        ((sec_link_mask << 16) & boosted_mask && ((other_mask << 8) & boosted_mask) == 0) ||                                                          // down and not blocked
                        ((((sec_link_mask & 18229723555195321596ULL) >> 2) & boosted_mask) && (((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0) || // left and not blocked
                        ((((sec_link_mask & 4557430888798830399ULL) << 2) & boosted_mask) && (((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0))     // right and not blocked
                        return (16384 * depth);
                }
            }
        }
        int alphabeg;
        if (depth > mincachedepth)
        {
            auto it = cache[depth].find(position);
            if (it != cache[depth].end())
            {
                ttentry entry = it->second;
                if (__builtin_expect(entry.flag & 1, 0))
                {
                    if (entry.score <= alpha) // if current alpha >= cached alpha then the alpha during evaluation wont change, thus we can return the current alpha
                        return alpha;
                    if (entry.flag > 1) // if the cached alpha is exact && it is bigger than the current alpha (because of the condition above) then we can return it
                        return entry.score;
                    beta = min(beta, entry.score);
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
                field temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(8ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(8ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT;
                temp.is_boosted_mask &= ~8ULL;
                temp.is_fir_mask &= ~8ULL;
                temp.is_link_mask &= ~8ULL;
                ++temp.fir_link;

                int reschild = minimax(depth, alpha, beta, false, temp, cache);
                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }
            }
            if (fir_link_mask & 16ULL)
            {
                field temp = position;

                temp.forward_adv_fir -= 7 - (__builtin_ctzll(16ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(16ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_FIR_SHIFT;
                temp.is_boosted_mask &= ~16ULL;
                temp.is_fir_mask &= ~16ULL;
                temp.is_link_mask &= ~16ULL;
                ++temp.fir_link;

                int reschild = minimax(depth, alpha, beta, false, temp, cache);
                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }
            }
        }

        if (position.state_mask & STATE_IS_BOOST_AVAILABLE_FIR)
        {
            uint64_t temp = fir_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << __builtin_ctzll(temp)); // back -> front

                position.is_boosted_mask ^= pos;
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_FIR;
                int reschild = minimax(depth, alpha, beta, false, position, cache);
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_FIR;
                position.is_boosted_mask ^= pos;

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
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
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_FIR;
                int reschild = minimax(depth, alpha, beta, false, position, cache);
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_FIR;
                position.is_boosted_mask ^= pos;

                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }

                temp ^= pos;
            }
        }

        const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;

        if (position.state_mask & STATE_IS_FIREWALL_AVAILABLE_SEC)
        {
            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_FIR) == 0)
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

            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_FIR) == 0)
            {
                field temp = position;

                temp.state_mask |= STATE_IS_BOOST_AVAILABLE_FIR;
                temp.is_boosted_mask &= temp.is_sec_mask;

                int reschild = minimax(depth, alpha, beta, false, temp, cache);
                alpha = (reschild > alpha) ? reschild : alpha;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {alpha, 0};
                    return alpha;
                }
            }
        }
        else
        {
            // todo
        }
        if (depth > mincachedepthfull)
            cache[depth][position] = {alpha, (alpha > alphabeg) ? 3 : 1};
        return alpha;
    }
    else
    {
        if (depth == 0)
            return position.evaluate_fir();
        --depth;
        if (position.sec_link == 3)
        {
            if (sec_link_mask & 1729382256910270464ULL)
                return (-16384 * depth);
            if (fir_link_mask)
            {
                const uint64_t firmask = position.is_fir_mask, secmask = position.is_sec_mask;
                if (((fir_link_mask >> 8) & secmask) ||                             // up
                    ((fir_link_mask << 8) & secmask) ||                             // down
                    (((fir_link_mask & 18374403900871474942ULL) >> 1) & secmask) || // left
                    (((fir_link_mask & 9187201950435737471ULL) << 1) & secmask))    // right
                    return (-16384 * depth);
                if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_SEC) == 0)
                {
                    uint64_t boosted_mask = position.is_boosted_mask & secmask;
                    uint64_t other_mask = fir_virus_mask | secmask;

                    if ((((fir_link_mask >> 16) & boosted_mask) && ((other_mask >> 8) & boosted_mask) == 0) ||                                                        // up and not blocked
                        ((fir_link_mask << 16) & boosted_mask && ((other_mask << 8) & boosted_mask) == 0) ||                                                          // down and not blocked
                        ((((fir_link_mask & 18229723555195321596ULL) >> 2) & boosted_mask) && (((other_mask & 18374403900871474942ULL) >> 1) & boosted_mask) == 0) || // left and not blocked
                        ((((fir_link_mask & 4557430888798830399ULL) << 2) & boosted_mask) && (((other_mask & 9187201950435737471ULL) << 1) & boosted_mask) == 0))     // right and not blocked
                        return (-16384 * depth);
                }
            }
        }
        int betabeg;
        if (depth > mincachedepth)
        {
            auto it = cache[depth].find(position);
            if (it != cache[depth].end())
            {
                ttentry entry = it->second;
                if (__builtin_expect(entry.flag & 1, 0))
                {
                    if (entry.score >= beta) // if current beta <= cached beta then the beta during evaluation wont change, thus we can return the current beta
                        return beta;
                    if (entry.flag > 1) // if the cached beta is exact && it is smaller than the current beta (because of the condition above) then we can return it
                        return entry.score;
                    alpha = max(alpha, entry.score);
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
                field temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(576460752303423488ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(576460752303423488ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT;
                temp.is_boosted_mask &= ~576460752303423488ULL;
                temp.is_sec_mask &= ~576460752303423488ULL;
                temp.is_link_mask &= ~576460752303423488ULL;
                ++temp.sec_link;

                int reschild = minimax(depth, alpha, beta, true, temp, cache);
                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }
            }
            if (sec_link_mask & 1152921504606846976ULL)
            {
                field temp = position;

                temp.forward_adv_sec -= (__builtin_ctzll(1152921504606846976ULL) >> 3);
                temp.state_mask |= ((temp.is_boosted_mask >> __builtin_ctzll(1152921504606846976ULL)) & 1) << STATE_IS_BOOST_AVAILABLE_SEC_SHIFT;
                temp.is_boosted_mask &= ~1152921504606846976ULL;
                temp.is_sec_mask &= ~1152921504606846976ULL;
                temp.is_link_mask &= ~1152921504606846976ULL;
                ++temp.sec_link;

                int reschild = minimax(depth, alpha, beta, true, temp, cache);
                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }
            }
        }

        if (position.state_mask & STATE_IS_BOOST_AVAILABLE_SEC)
        {
            uint64_t temp = sec_virus_mask;

            while (temp)
            {
                const uint64_t pos = (1ULL << (63 - __builtin_clzll(temp))); // front -> back

                position.is_boosted_mask ^= pos;
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_SEC;
                int reschild = minimax(depth, alpha, beta, true, position, cache);
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_SEC;
                position.is_boosted_mask ^= pos;

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
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
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_SEC;
                int reschild = minimax(depth, alpha, beta, true, position, cache);
                position.state_mask ^= STATE_IS_BOOST_AVAILABLE_SEC;
                position.is_boosted_mask ^= pos;

                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }

                temp ^= pos;
            }
        }

        const uint64_t firmask = (fir_link_mask | fir_virus_mask), secmask = (sec_link_mask | sec_virus_mask);
        if (position.state_mask & STATE_IS_FIREWALL_AVAILABLE_FIR)
        {
            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_SEC) == 0)
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

            if ((position.state_mask & STATE_IS_BOOST_AVAILABLE_SEC) == 0)
            {
                field temp = position;

                temp.state_mask |= STATE_IS_BOOST_AVAILABLE_SEC;
                temp.is_boosted_mask &= temp.is_fir_mask;

                int reschild = minimax(depth, alpha, beta, true, temp, cache);
                beta = (reschild < beta) ? reschild : beta;
                if (beta <= alpha)
                {
                    if (depth > mincachedepth)
                        cache[depth][position] = {beta, 0};
                    return beta;
                }
            }
        }
        else
        {
            // todo
        }
        if (depth > mincachedepthfull)
            cache[depth][position] = {beta, (beta < betabeg) ? 3 : 1};
        return beta;
    }
}

const int multifinish = 2;

mutex mtx;

void displayProgressBar(const double total, const double finished, const string &text)
{
    cout << "\33[2K\r" << flush;
    cout << text << " " << int(finished * 100.0 / total) << " %\r" << flush;
}

int cutoffdepth;

int minimaxscout(const int depth, int alpha, int beta, const bool player, field &position)
{
    if (depth < cutoffdepth)
    {
        vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
        return minimax(depth, alpha, beta, player, position, newcache);
    }
    if (player)
    {
        vector<field> allmoves = possiblemoves(position, true);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].fir_link > 3)
                return (16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                scores[i] = minimax(depth - 3, alpha, beta, false, allmoves[i], newcache);
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2a/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        int max = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] > max)
            {
                max = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        alpha = minimaxscout(depth - 1, alpha, beta, false, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        int tscore;
        // cout << endl;
        // cout << "D" << depth << " alpha: " << alpha << endl;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &tscore]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, alpha, alpha + 1, false, allmoves[i], newcache);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << depth << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] > alpha){
                //    cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, scores[i], beta, false, allmoves[i], newcache);
                }
                //else
                //    cout << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2b/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (scores[i] > alpha)
            {
                alpha = scores[i];
            }
        }
        return alpha;
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].sec_link > 3)
                return (-16384 * depth);
        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                scores[i] = minimax(depth - 3, alpha, beta, true, allmoves[i], newcache);
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2a/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        int min = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] < min)
            {
                min = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        beta = minimaxscout(depth - 1, alpha, beta, true, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        int tscore;
        // cout << endl;
        // cout << "D" << depth << " beta: " << beta << endl;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished, &tscore]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, beta - 1, beta, true, allmoves[i], newcache);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << depth << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] < beta){
                //    cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, alpha, scores[i], true, allmoves[i], newcache);
                }
                //else
                //    cout << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 2b/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (beta > scores[i])
            {
                beta = scores[i];
            }
        }
        return beta;
    }
}

pair<field, int> minimaxmain(const int depth, int alpha, int beta, const bool player, field &position)
{
    cutoffdepth = depth - 5;
    if (player)
    {
        vector<field> allmoves = possiblemoves(position, true);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].fir_link > 3)
                return make_pair(allmoves[i], (16384 * depth));

        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 3, alpha, beta, false, allmoves[i], newcache);
                auto stop = high_resolution_clock::now();
                //cout << "Predict time: " << duration_cast<milliseconds>(stop - start).count() << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 1/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        int max = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] > max)
            {
                max = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        // cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        alpha = minimaxscout(depth - 1, alpha, beta, false, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        // cout << endl;
        // cout << "alpha: " << alpha << endl;
        start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, alpha, alpha + 1, false, allmoves[i], newcache);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << i << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] > alpha){
                    //cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, scores[i], beta, false, allmoves[i], newcache);
                }
                //else
                //    cout << endl;
                mtx.lock();
                // for(int j = 0; j < depth; ++j)
                //     cout << "j:" << j << " " << newcache[j].size() << endl;
                // if(toterminate){
                //     mtx.unlock();
                //     scores[i] = minimaxscout(depth - 1, alpha, beta, false, allmoves[i]);
                // }
                // else if(allmoves.size() - curfreethreads =return make_pair(bestfield, alpha);= multifinish && duration_cast<milliseconds>(stop - start).count() > (800 * (1 << (depth - 12))) && depth > 11){
                //     toterminate = true;
                // }
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 3/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (scores[i] > alpha)
            {
                alpha = scores[i];
                bestfield = allmoves[i];
            }
        }
        return make_pair(bestfield, alpha);
    }
    else
    {
        vector<field> allmoves = possiblemoves(position, false);
        for (int i = 0; i < allmoves.size(); ++i)
            if (allmoves[i].sec_link > 3)
                return make_pair(allmoves[i], (-16384 * depth));

        vector<thread> threads(allmoves.size());
        vector<int> scores(allmoves.size());
        int finished = 0;
        auto start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, alpha, beta, &allmoves, &finished]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 3, alpha, beta, true, allmoves[i], newcache);
                auto stop = high_resolution_clock::now();
                //cout << "Predict time: " << duration_cast<milliseconds>(stop - start).count() << endl;
                mtx.lock();
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 1/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        auto end = high_resolution_clock::now();
        int min = scores[0], index = 0;
        for (int i = 1; i < allmoves.size(); ++i)
        {
            if (scores[i] < min)
            {
                min = scores[i];
                index = i;
            }
        }
        swap(allmoves[0], allmoves[index]);
        // cout << "Prediction time: " << duration_cast<milliseconds>(end - start).count() << endl;
        field bestfield = allmoves[0];
        beta = minimaxscout(depth - 1, alpha, beta, true, allmoves[0]);
        allmoves.erase(allmoves.begin());
        threads.erase(threads.begin());
        scores.erase(scores.begin());
        finished = 0;
        // cout << endl;
        // cout << "beta: " << beta << endl;
        start = high_resolution_clock::now();
        for (int i = 0; i < allmoves.size(); ++i)
        {
            threads[i] = thread([&scores, i, depth, &alpha, &beta, &allmoves, &finished]()
                                {
                vector<ankerl::unordered_dense::map<field, ttentry, field>> newcache(depth, ankerl::unordered_dense::map<field, ttentry, field>(1024));
                auto start = high_resolution_clock::now();
                scores[i] = minimax(depth - 1, beta - 1, beta, true, allmoves[i], newcache);
                auto stop = high_resolution_clock::now();
                //cout << "Scout " << i << " time: " << duration_cast<milliseconds>(stop - start).count();
                if(scores[i] < beta){
                    //cout << "    >" << endl;
                    scores[i] = minimax(depth - 1, alpha, scores[i], true, allmoves[i], newcache);
                }
                //else
                //    cout << endl;
                mtx.lock();
                // if(toterminate){
                //     mtx.unlock();
                //     scores[i] = minimaxscout(depth - 1, alpha, beta, true, allmoves[i]);
                // }
                // else if(allmoves.size() - curfreethreads == multifinish && duration_cast<milliseconds>(stop - start).count() > (800 * (1 << (depth - 12))) && depth > 11){
                //     toterminate = true;
                // }
                ++finished;
                mtx.unlock();
                displayProgressBar(allmoves.size(), finished, "Calculating stage 3/3 "); });
        }
        for (int i = 0; i < allmoves.size(); ++i)
            threads[i].join();
        end = high_resolution_clock::now();
        // cout << "Minimax time: " << duration_cast<milliseconds>(end - start).count() << endl;
        for (int i = 0; i < allmoves.size(); ++i)
        {
            if (beta > scores[i])
            {
                beta = scores[i];
                bestfield = allmoves[i];
            }
        }
        return make_pair(bestfield, beta);
    }
}

int main()
{
    // srand(time(NULL));
    field pos;
    // auto start = high_resolution_clock::now();
    // int checksum = 0;
    // for (int i = 0; i < 70; ++i)
    // {
    //     int fir, sec;
    //     field_construct(pos, indexes[69], indexes[i]);
    //     auto startit = high_resolution_clock::now();
    //     pair<field, int> move = minimaxmain(16, MIN, MAX, true, pos);
    //     auto endit = high_resolution_clock::now();
    //     cout << "\33[2K\r" << flush;
    //     cout << move.second << "     " << duration_cast<milliseconds>(endit - startit).count() << endl;
    //     fir = move.second;
    //     checksum ^= (move.second << (indexes[i] & 1));
    //     field_construct(pos, indexes[i], indexes[69]);
    //     startit = high_resolution_clock::now();
    //     move = minimaxmain(16, MIN, MAX, false, pos);
    //     endit = high_resolution_clock::now();
    //     cout << "\33[2K\r" << flush;
    //     cout << move.second << "     " << duration_cast<milliseconds>(endit - startit).count() << endl;
    //     sec = move.second;
    //     if (fir != -1 * sec)
    //     {
    //         cout << "Eval error\n";
    //         exit(1);
    //     }
    //     checksum ^= (move.second << (indexes[i] & 1));
    // }
    // auto end = high_resolution_clock::now();
    // cout << duration_cast<milliseconds>(end - start).count() << endl;
    // cout << "hash: " << checksum << endl;

    // return 0;
    field_construct(pos, indexes[15], indexes[15]);
    pos.print_field();
    cout << endl;
    auto startm = high_resolution_clock::now();
    for (;;)
    {
        auto start = high_resolution_clock::now();
        pair<field, int> move = minimaxmain(16, MIN, MAX, false, pos);
        auto end = high_resolution_clock::now();
        cout << "\33[2K\r" << flush;
        cout << "Minimized score: " << move.second << "      " << duration_cast<milliseconds>(end - start).count() << endl;
        pos = move.first;
        pos.print_field();
        cout << endl;
        if (pos.sec_link == 4)
        {
            cout << "Player one wins! " << endl;
            pos.print_field();
            break;
        }
        else if (pos.sec_virus == 4)
        {
            cout << "Player one loses! " << endl;
            pos.print_field();
            break;
        }
        start = high_resolution_clock::now();
        move = minimaxmain(16, MIN, MAX, true, pos);
        end = high_resolution_clock::now();
        cout << "\33[2K\r" << flush;
        cout << "Maximized score: " << move.second << "      " << duration_cast<milliseconds>(end - start).count() << endl;
        pos = move.first;
        pos.print_field();
        cout << endl;
        if (pos.fir_link == 4)
        {
            cout << "Player two wins! " << endl;
            pos.print_field();
            break;
        }
        else if (pos.fir_virus == 4)
        {
            cout << "Player two loses! " << endl;
            pos.print_field();
            break;
        }
    }
    auto endm = high_resolution_clock::now();
    cout << duration_cast<milliseconds>(endm - startm).count() << endl;
    // ifstream getdata("evaluations.txt");
    // int firwin = 0, secwin = 0;
    // for(int i = 0; i < 70; ++i){
    //     for(int u = 0; u < 70; ++u){
    //         cout << u << endl;
    //         field pos;
    //         generate_field(pos, true, indexes[i]);
    //         generate_field(pos, false, indexes[u]);
    //         vector<field> moves;
    //         for(;;){
    //             pair<field, int> move = minimaxmain(6, -1000000, 1000000, false, pos);
    //             int check;
    //             getdata >> check;
    //             if(check != move.second){
    //                 cout << "Check error1!" << endl;
    //                 cout << "Expected: " << check << endl;
    //                 cout << "Got: " << move.second << endl;
    //                 return 1;
    //             }
    //             pos = move.first;
    //             bool isfound = false;
    //             for(int it = 0; it < moves.size(); ++it)
    //                 if(moves[it] == pos)
    //                 {
    //                     isfound = true;
    //                     break;
    //                 }
    //             if(isfound)
    //                 break;
    //             moves.push_back(pos);
    //             // pos.print_field();
    //             // cout << endl;
    //             if(pos.sec_link == 4){
    //                 firwin++;
    //                 break;
    //             }
    //             else if(pos.sec_virus == 4){
    //                 secwin++;
    //                 break;
    //             }
    //             move = minimaxmain(6, -1000000, 1000000, true, pos);
    //             getdata >> check;
    //             if(check != move.second){
    //                 cout << "Check error2!" << endl;
    //                 cout << "Expected: " << check << endl;
    //                 cout << "Got: " << move.second << endl;
    //                 return 1;
    //             }
    //             pos = move.first;
    //             for(int it = 0; it < moves.size(); ++it)
    //                 if(moves[it] == pos)
    //                 {
    //                     isfound = true;
    //                     break;
    //                 }
    //             if(isfound)
    //                 break;
    //             moves.push_back(pos);
    //             if(pos.fir_link == 4){
    //                 secwin++;
    //                 break;
    //             }
    //             else if(pos.fir_virus == 4){
    //                 firwin++;
    //                 break;
    //             }
    //         }
    //     }
    //     cout << "Firwinrate = " << double(firwin) / double(70 + i * 70) << endl;
    //     cout << "Secwinrate = " << double(secwin) / double(70 + i * 70) << endl;
    // }
    // cout << "Firwinrate = " << double(firwin) / 4900.0 << endl;
    // cout << "Secwinrate = " << double(secwin) / 4900.0 << endl;
}
