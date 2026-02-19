#include "rnab_impl.hpp"

minimax_main_result_t (*minimax_iteration_main)(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position) = NULL;
minimax_main_result_t (*minimax_main)(const int depth, int alpha, int beta, const bool player, field_t *position) = NULL;
possible_moves_t (*possible_moves)(const field_t *position, const bool player) = NULL;

extern minimax_main_result_t minimax_iteration_main_scalar(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position);
extern minimax_main_result_t minimax_main_scalar(const int depth, int alpha, int beta, const bool player, field_t *position);
extern possible_moves_t possible_moves_scalar(const field_t *position, const bool player);

#if defined(__x86_64__)

extern minimax_main_result_t minimax_iteration_main_avx512f(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position);
extern minimax_main_result_t minimax_iteration_main_avx2(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position);
extern minimax_main_result_t minimax_iteration_main_avx(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position);
extern minimax_main_result_t minimax_iteration_main_sse4_2(const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position);

extern minimax_main_result_t minimax_main_avx512f(const int depth, int alpha, int beta, const bool player, field_t *position);
extern minimax_main_result_t minimax_main_avx2(const int depth, int alpha, int beta, const bool player, field_t *position);
extern minimax_main_result_t minimax_main_avx(const int depth, int alpha, int beta, const bool player, field_t *position);
extern minimax_main_result_t minimax_main_sse4_2(const int depth, int alpha, int beta, const bool player, field_t *position);

extern possible_moves_t possible_moves_avx512f(const field_t *position, const bool player);
extern possible_moves_t possible_moves_avx2(const field_t *position, const bool player);
extern possible_moves_t possible_moves_avx(const field_t *position, const bool player);
extern possible_moves_t possible_moves_sse4_2(const field_t *position, const bool player);

#else

#endif

#ifdef _WIN32
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __attribute__((visibility("default")))
#endif

struct generic_representation
{
    int32_t player_first_link_1_pos;
    int32_t player_first_link_2_pos;
    int32_t player_first_link_3_pos;
    int32_t player_first_link_4_pos;
    int32_t player_first_virus_1_pos;
    int32_t player_first_virus_2_pos;
    int32_t player_first_virus_3_pos;
    int32_t player_first_virus_4_pos;
    int32_t player_second_link_1_pos;
    int32_t player_second_link_2_pos;
    int32_t player_second_link_3_pos;
    int32_t player_second_link_4_pos;
    int32_t player_second_virus_1_pos;
    int32_t player_second_virus_2_pos;
    int32_t player_second_virus_3_pos;
    int32_t player_second_virus_4_pos;
    int32_t player_first_boosted_cell;
    int32_t player_second_boosted_cell;
    int32_t player_first_firewalled_cell;
    int32_t player_second_firewalled_cell;
    int32_t player_first_captured_links_num;
    int32_t player_first_captured_viruses_num;
    int32_t player_second_captured_links_num;
    int32_t player_second_captured_viruses_num;
    int32_t player_first_is_virus_checker_available;
    int32_t player_second_is_virus_checker_available;
    int32_t player_first_is_404_not_found_available;
    int32_t player_second_is_404_not_found_available;
};

generic_representation intern_to_generic_representation(field_t pos)
{
    generic_representation res = {0};
    uint64_t temp;

    temp = pos.is_fir_mask & pos.is_link_mask;

    res.player_first_link_1_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_first_link_2_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_first_link_3_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_first_link_4_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);

    temp = pos.is_fir_mask & (~pos.is_link_mask);

    res.player_first_virus_1_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_first_virus_2_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_first_virus_3_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_first_virus_4_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);

    temp = pos.is_sec_mask & pos.is_link_mask;

    res.player_second_link_1_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_second_link_2_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_second_link_3_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_second_link_4_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);

    temp = pos.is_sec_mask & (~pos.is_link_mask);

    res.player_second_virus_1_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_second_virus_2_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_second_virus_3_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);
    res.player_second_virus_4_pos = (temp) ? (__builtin_ctzll(temp)) : -1;
    temp &= (temp - 1);

    res.player_first_boosted_cell = (pos.is_fir_mask & pos.is_boosted_mask) ? (__builtin_ctzll(pos.is_fir_mask & pos.is_boosted_mask)) : -1;
    res.player_second_boosted_cell = (pos.is_sec_mask & pos.is_boosted_mask) ? (__builtin_ctzll(pos.is_sec_mask & pos.is_boosted_mask)) : -1;

    res.player_first_firewalled_cell = pos.firewall_fir;
    res.player_second_firewalled_cell = pos.firewall_sec;

    res.player_first_captured_links_num = pos.fir_link;
    res.player_first_captured_viruses_num = pos.fir_virus;

    res.player_second_captured_links_num = pos.sec_link;
    res.player_second_captured_viruses_num = pos.sec_virus;

    res.player_first_is_virus_checker_available = (pos.is_checker_available_fir == 1) ? 1 : 0;
    res.player_second_is_virus_checker_available = (pos.is_checker_available_sec == 1) ? 1 : 0;

    res.player_first_is_404_not_found_available = (pos.is_swap_available_fir == 1) ? 1 : 0;
    res.player_second_is_404_not_found_available = (pos.is_swap_available_sec == 1) ? 1 : 0;

    return res;
}

extern "C" EXPORT_API void rnab_engine_init()
{
#if defined(__x86_64__)
    minimax_iteration_main = ((__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) ? minimax_iteration_main_avx512f : ((__builtin_cpu_supports("avx2")) ? minimax_iteration_main_avx2 : ((__builtin_cpu_supports("avx")) ? minimax_iteration_main_avx : ((__builtin_cpu_supports("sse4.2")) ? minimax_iteration_main_sse4_2 : minimax_iteration_main_scalar))));
    minimax_main = ((__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) ? minimax_main_avx512f : ((__builtin_cpu_supports("avx2")) ? minimax_main_avx2 : ((__builtin_cpu_supports("avx")) ? minimax_main_avx : ((__builtin_cpu_supports("sse4.2")) ? minimax_main_sse4_2 : minimax_main_scalar))));
    possible_moves = ((__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) ? possible_moves_avx512f : ((__builtin_cpu_supports("avx2")) ? possible_moves_avx2 : ((__builtin_cpu_supports("avx")) ? possible_moves_avx : ((__builtin_cpu_supports("sse4.2")) ? possible_moves_sse4_2 : possible_moves_scalar))));
#else
    minimax_iteration_main = minimax_iteration_main_scalar;
    minimax_main = minimax_main_scalar;
    possible_moves = possible_moves_scalar;
#endif
}

extern "C" EXPORT_API generic_representation rnab_compute_best_move(int32_t max_depth, int64_t max_search_time, int32_t player, generic_representation *game_state)
{
    assert(minimax_iteration_main);
    assert(minimax_main);
    assert(possible_moves);

    field_t position;
#define IS_ARG_LEGIT(arg_var) \
    assert((arg_var == -1 || (arg_var >= 0 && arg_var <= 63)) && "Illegal argument (" #arg_var ")")

#define PLACE_AND_CHECK_VIRUS(virus_var, player_mask_var)                                                      \
    if (virus_var >= 0 && virus_var <= 63)                                                                     \
    {                                                                                                          \
        const uint64_t cur_mask = 1ULL << virus_var;                                                           \
                                                                                                               \
        assert((player_mask_var & cur_mask) == 0 && "Cards may not be on top of each other (" #virus_var ")"); \
                                                                                                               \
        player_mask_var |= cur_mask;                                                                           \
    }

#define PLACE_AND_CHECK_LINK(link_var, player_mask_var, link_mask)                                                                           \
    if (link_var >= 0 && link_var <= 63)                                                                                                     \
    {                                                                                                                                        \
        const uint64_t cur_mask = 1ULL << link_var;                                                                                          \
                                                                                                                                             \
        assert((player_mask_var & cur_mask) == 0 && (link_mask & cur_mask) == 0 && "Cards may not be on top of each other (" #link_var ")"); \
                                                                                                                                             \
        player_mask_var |= cur_mask;                                                                                                         \
        link_mask |= cur_mask;                                                                                                               \
    }

    assert(player == 0 || player == 1);

    IS_ARG_LEGIT(game_state->player_first_link_1_pos);
    IS_ARG_LEGIT(game_state->player_first_link_2_pos);
    IS_ARG_LEGIT(game_state->player_first_link_3_pos);
    IS_ARG_LEGIT(game_state->player_first_link_4_pos);

    IS_ARG_LEGIT(game_state->player_first_virus_1_pos);
    IS_ARG_LEGIT(game_state->player_first_virus_2_pos);
    IS_ARG_LEGIT(game_state->player_first_virus_3_pos);
    IS_ARG_LEGIT(game_state->player_first_virus_4_pos);

    IS_ARG_LEGIT(game_state->player_second_link_1_pos);
    IS_ARG_LEGIT(game_state->player_second_link_2_pos);
    IS_ARG_LEGIT(game_state->player_second_link_3_pos);
    IS_ARG_LEGIT(game_state->player_second_link_4_pos);

    IS_ARG_LEGIT(game_state->player_second_virus_1_pos);
    IS_ARG_LEGIT(game_state->player_second_virus_2_pos);
    IS_ARG_LEGIT(game_state->player_second_virus_3_pos);
    IS_ARG_LEGIT(game_state->player_second_virus_4_pos);

    IS_ARG_LEGIT(game_state->player_first_boosted_cell);
    IS_ARG_LEGIT(game_state->player_second_boosted_cell);
    IS_ARG_LEGIT(game_state->player_first_firewalled_cell);
    IS_ARG_LEGIT(game_state->player_second_firewalled_cell);

    position.is_sec_mask = 0;
    position.is_link_mask = 0;
    position.is_fir_mask = 0;
    position.is_boosted_mask = 0;

    PLACE_AND_CHECK_LINK(game_state->player_first_link_1_pos, position.is_fir_mask, position.is_link_mask);
    PLACE_AND_CHECK_LINK(game_state->player_first_link_2_pos, position.is_fir_mask, position.is_link_mask);
    PLACE_AND_CHECK_LINK(game_state->player_first_link_3_pos, position.is_fir_mask, position.is_link_mask);
    PLACE_AND_CHECK_LINK(game_state->player_first_link_4_pos, position.is_fir_mask, position.is_link_mask);

    PLACE_AND_CHECK_VIRUS(game_state->player_first_virus_1_pos, position.is_fir_mask);
    PLACE_AND_CHECK_VIRUS(game_state->player_first_virus_2_pos, position.is_fir_mask);
    PLACE_AND_CHECK_VIRUS(game_state->player_first_virus_3_pos, position.is_fir_mask);
    PLACE_AND_CHECK_VIRUS(game_state->player_first_virus_4_pos, position.is_fir_mask);

    PLACE_AND_CHECK_LINK(game_state->player_second_link_1_pos, position.is_sec_mask, position.is_link_mask);
    PLACE_AND_CHECK_LINK(game_state->player_second_link_2_pos, position.is_sec_mask, position.is_link_mask);
    PLACE_AND_CHECK_LINK(game_state->player_second_link_3_pos, position.is_sec_mask, position.is_link_mask);
    PLACE_AND_CHECK_LINK(game_state->player_second_link_4_pos, position.is_sec_mask, position.is_link_mask);

    PLACE_AND_CHECK_VIRUS(game_state->player_second_virus_1_pos, position.is_sec_mask);
    PLACE_AND_CHECK_VIRUS(game_state->player_second_virus_2_pos, position.is_sec_mask);
    PLACE_AND_CHECK_VIRUS(game_state->player_second_virus_3_pos, position.is_sec_mask);
    PLACE_AND_CHECK_VIRUS(game_state->player_second_virus_4_pos, position.is_sec_mask);

    assert((position.is_fir_mask & position.is_sec_mask) == 0 && "Player one cards may not be on top of player two cards");

    assert((position.is_fir_mask & (1ULL << game_state->player_first_boosted_cell)) && "Boost must be on top of a card (player_first_boosted_cell)");
    assert((position.is_sec_mask & (1ULL << game_state->player_second_boosted_cell)) && "Boost must be on top of a card (player_second_boosted_cell)");

    if (game_state->player_first_boosted_cell >= 0 && game_state->player_first_boosted_cell <= 63)
    {
        position.is_boosted_mask |= (1ULL << game_state->player_first_boosted_cell);
        position.is_boost_available_fir = 0;
    }
    else
    {
        position.is_boost_available_fir = 1;
    }

    if (game_state->player_second_boosted_cell >= 0 && game_state->player_second_boosted_cell <= 63)
    {
        position.is_boosted_mask |= (1ULL << game_state->player_second_boosted_cell);
        position.is_boost_available_sec = 0;
    }
    else
    {
        position.is_boost_available_sec = 1;
    }

    if (game_state->player_first_firewalled_cell >= 0 && game_state->player_first_firewalled_cell <= 63)
    {
        position.firewall_fir = game_state->player_first_firewalled_cell;

        assert(((1ULL << game_state->player_first_firewalled_cell) & (position.is_sec_mask | 1729382256910270488ULL)) == 0 && "Firewall cant be on top of enemy cards or exit squares (player_first_firewalled_cell)");

        position.is_firewall_available_fir = 0;
    }
    else
    {
        position.is_firewall_available_fir = 1;
    }

    if (game_state->player_second_firewalled_cell >= 0 && game_state->player_second_firewalled_cell <= 63)
    {
        position.firewall_sec = game_state->player_second_firewalled_cell;

        assert(((1ULL << game_state->player_second_firewalled_cell) & (position.is_fir_mask | 1729382256910270488ULL)) == 0 && "Firewall cant be on top of enemy cards or exit squares (player_second_firewalled_cell)");

        position.is_firewall_available_sec = 0;
    }
    else
    {
        position.is_firewall_available_sec = 1;
    }

    position.is_checker_available_fir = (game_state->player_first_is_virus_checker_available) ? 1 : 0;
    position.is_checker_available_sec = (game_state->player_second_is_virus_checker_available) ? 1 : 0;

    position.is_swap_available_fir = (game_state->player_first_is_404_not_found_available) ? 1 : 0;
    position.is_swap_available_sec = (game_state->player_second_is_404_not_found_available) ? 1 : 0;

    assert(game_state->player_first_captured_links_num >= 0 && game_state->player_first_captured_links_num < 4 && "Terminal conditions are not allowed");
    assert(game_state->player_first_captured_viruses_num >= 0 && game_state->player_first_captured_viruses_num < 4 && "Terminal conditions are not allowed");

    assert(game_state->player_second_captured_links_num >= 0 && game_state->player_second_captured_links_num < 4 && "Terminal conditions are not allowed");
    assert(game_state->player_second_captured_viruses_num >= 0 && game_state->player_second_captured_viruses_num < 4 && "Terminal conditions are not allowed");

    int total_links_fir = __builtin_popcountll(position.is_fir_mask & position.is_link_mask);
    int total_links_sec = __builtin_popcountll(position.is_sec_mask & position.is_link_mask);

    int total_viruses_fir = __builtin_popcountll(position.is_fir_mask) - total_links_fir;
    int total_viruses_sec = __builtin_popcountll(position.is_sec_mask) - total_links_sec;

    assert(total_links_fir + total_links_sec + game_state->player_first_captured_links_num + game_state->player_second_captured_links_num == 8 && "There should be 8 links in total");
    assert(total_viruses_fir + total_viruses_sec + game_state->player_first_captured_viruses_num + game_state->player_second_captured_viruses_num == 8 && "There should be 8 viruses in total");

    position.fir_link = game_state->player_first_captured_links_num;
    position.fir_virus = game_state->player_first_captured_viruses_num;

    position.sec_link = game_state->player_second_captured_links_num;
    position.sec_virus = game_state->player_second_captured_viruses_num;

#undef IS_ARG_LEGIT
#undef PLACE_AND_CHECK_VIRUS
#undef PLACE_AND_CHECK_LINK
    return intern_to_generic_representation(minimax_iteration_main(max_depth, max_search_time, MAX, MIN, player, &position).best_field);
}
