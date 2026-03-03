#include "rnab_impl.hpp"

#ifdef _WIN32
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __attribute__((visibility("default")))
#endif

#define DECLARE_SCALAR(ret, name, ...) \
    extern ret name##_scalar(__VA_ARGS__)

#define DECLARE_SIMD_VARIANTS(ret, name, ...) \
    extern ret name##_avx512f(__VA_ARGS__);   \
    extern ret name##_avx2(__VA_ARGS__);      \
    extern ret name##_avx(__VA_ARGS__);       \
    extern ret name##_sse4_2(__VA_ARGS__)

#define DISPATCH_EXPR(name) ((__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) ? name##_avx512f : (__builtin_cpu_supports("avx2") ? name##_avx2 : (__builtin_cpu_supports("avx") ? name##_avx : (__builtin_cpu_supports("sse4.2") ? name##_sse4_2 : name##_scalar))))

// Define the function pointer, initialized to best available impl
#if defined(__x86_64__)
#define DEFINE_DISPATCHED(ret, name, ...)          \
    DECLARE_SCALAR(ret, name, __VA_ARGS__);        \
    DECLARE_SIMD_VARIANTS(ret, name, __VA_ARGS__); \
    ret (*name)(__VA_ARGS__) = DISPATCH_EXPR(name)
#else
#define DEFINE_DISPATCHED(ret, name, ...)   \
    DECLARE_SCALAR(ret, name, __VA_ARGS__); \
    ret (*name)(__VA_ARGS__) = name##_scalar
#endif

#if defined(__x86_64__)
#define DEFINE_DISPATCHED_VAR(type, name) \
    extern type name##_scalar;            \
    extern type name##_avx512f;           \
    extern type name##_avx2;              \
    extern type name##_avx;               \
    extern type name##_sse4_2;            \
    type name = DISPATCH_EXPR(name)
#else
#define DEFINE_DISPATCHED_VAR(type, name) \
    extern type name##_scalar;            \
    type name = name##_scalar
#endif

#ifdef BRANCH_DEBUG
DEFINE_DISPATCHED_VAR(int, _total_branch_count);

extern cutoff_tracker_t cutoff_tracker[1000];

void clear_branch_tracker()
{
    for (int i = 0; i < 1000; ++i)
    {
        cutoff_tracker[i].total_entries = 0;
        cutoff_tracker[i].cutoff_entries = 0;
        cutoff_tracker[i].improved_score = 0;
        cutoff_tracker[i].recursion_cost = 0;
    }
}

#endif

DEFINE_DISPATCHED(minimax_main_result_t, minimax_iteration_main, const int max_depth, const int64_t max_search_time, int alpha, int beta, const bool player, field_t *position);
DEFINE_DISPATCHED(minimax_main_result_t, minimax_main, const int depth, int alpha, int beta, const bool player, field_t *position);
DEFINE_DISPATCHED(possible_moves_t, possible_moves, const field_t *position, const bool player);

struct generic_representation
{
    uint64_t player_first_card_mask;
    uint64_t player_second_card_mask;
    uint64_t link_card_mask;
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

    void format() const
    {
        __builtin_printf("(generic_representation){0x%016" PRIx64 ", 0x%016" PRIx64 ", 0x%016" PRIx64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}\n", player_first_card_mask, player_second_card_mask, link_card_mask, player_first_boosted_cell, player_second_boosted_cell, player_first_firewalled_cell, player_second_firewalled_cell, player_first_captured_links_num, player_first_captured_viruses_num, player_second_captured_links_num, player_second_captured_viruses_num, player_first_is_virus_checker_available, player_second_is_virus_checker_available, player_first_is_404_not_found_available, player_second_is_404_not_found_available);
    }
};

struct benchmark_t
{
    generic_representation position;
    int depth;
};

benchmark_t benchmark_array[] = {
    {(generic_representation){0xe718000000000000, 0x00000000000018e7, 0xa5000000000000a5, -1, -1, -1, -1, 0, 0, 0, 0, 1, 1, 1, 1}, 16},
    {(generic_representation){0x8008000010000000, 0x2000000000003007, 0x0008000010000005, 28, 61, 51, -1, 2, 0, 2, 3, 1, 1, 0, 1}, 14},
    {(generic_representation){0x8008001000000000, 0x0040000000001027, 0x8008000000000005, 36, 54, 51, -1, 2, 0, 2, 3, 1, 1, 1, 1}, 14},
    {(generic_representation){0x0004000000000420, 0x0400020001000010, 0x0004000001000400, 10, 4, 50, 61, 2, 1, 3, 3, 1, 1, 0, 0}, 18},
    {(generic_representation){0x1000000800000000, 0x0800040001000000, 0x0000040801000000, -1, 42, 35, 52, 2, 3, 3, 3, 1, 1, 1, 1}, 20},
    {(generic_representation){0x4008000010000000, 0x2000000000003007, 0x0008000010000005, 28, 61, 51, 61, 2, 0, 2, 3, 1, 1, 0, 1}, 14},
    {(generic_representation){0xa100000000100000, 0x00000000000000a8, 0xa000000000000020, 20, 5, -1, 5, 3, 1, 2, 3, 1, 1, 1, 1}, 16},
    {(generic_representation){0x8101000000000100, 0x0000004000000004, 0x8000000000000104, 8, 38, 63, 2, 2, 3, 3, 2, 1, 1, 0, 0}, 20},
    
};

generic_representation intern_to_generic_representation(field_t *pos)
{
    generic_representation res = {0};

    res.player_first_card_mask = pos->is_fir_mask;
    res.player_second_card_mask = pos->is_sec_mask;

    res.link_card_mask = pos->is_link_mask;

    res.player_first_boosted_cell = ((pos->is_fir_mask & pos->is_boosted_mask) ? (__builtin_ctzll(pos->is_fir_mask & pos->is_boosted_mask)) : -1);
    res.player_second_boosted_cell = ((pos->is_sec_mask & pos->is_boosted_mask) ? (__builtin_ctzll(pos->is_sec_mask & pos->is_boosted_mask)) : -1);

    res.player_first_firewalled_cell = ((pos->firewall_fir == 0) ? -1 : (pos->firewall_fir >> 1));
    res.player_second_firewalled_cell = ((pos->firewall_sec == 0) ? -1 : (pos->firewall_sec >> 1));

    res.player_first_captured_links_num = pos->fir_link;
    res.player_first_captured_viruses_num = pos->fir_virus;

    res.player_second_captured_links_num = pos->sec_link;
    res.player_second_captured_viruses_num = pos->sec_virus;

    res.player_first_is_virus_checker_available = ((pos->is_checker_available_fir == 1) ? 1 : 0);
    res.player_second_is_virus_checker_available = ((pos->is_checker_available_sec == 1) ? 1 : 0);

    res.player_first_is_404_not_found_available = ((pos->is_swap_available_fir == 1) ? 1 : 0);
    res.player_second_is_404_not_found_available = ((pos->is_swap_available_sec == 1) ? 1 : 0);

    return res;
}

field_t generic_representation_to_intern(generic_representation *game_state)
{
    assert(minimax_iteration_main);
    assert(minimax_main);
    assert(possible_moves);

    field_t position;
#define IS_ARG_LEGIT(arg_var) \
    assert((arg_var == -1 || (arg_var >= 0 && arg_var <= 63)) && "Illegal argument (" #arg_var ")")

    IS_ARG_LEGIT(game_state->player_first_boosted_cell);
    IS_ARG_LEGIT(game_state->player_second_boosted_cell);
    IS_ARG_LEGIT(game_state->player_first_firewalled_cell);
    IS_ARG_LEGIT(game_state->player_second_firewalled_cell);

#undef IS_ARG_LEGIT

    position.is_fir_mask = game_state->player_first_card_mask;
    position.is_sec_mask = game_state->player_second_card_mask;
    position.is_link_mask = game_state->link_card_mask;
    position.is_boosted_mask = 0;

    position.firewall_fir = 0;
    position.firewall_sec = 0;

    position.forward_adv_fir = 0;
    position.forward_adv_sec = 0;

    assert((position.is_fir_mask & position.is_sec_mask) == 0 && "Player one cards may not be on top of player two cards");

    uint64_t temp;

    temp = position.is_fir_mask;

    while (temp)
    {
        const int cur_pos = __builtin_ctzll(temp);
        const uint64_t cur_mask = 1ULL << cur_pos;

        position.forward_adv_fir += (7 - (cur_pos >> 3));

        temp ^= cur_mask;
    }

    temp = position.is_sec_mask;

    while (temp)
    {
        const int cur_pos = __builtin_ctzll(temp);
        const uint64_t cur_mask = 1ULL << cur_pos;

        position.forward_adv_sec += (cur_pos >> 3);

        temp ^= cur_mask;
    }

    if (game_state->player_first_boosted_cell >= 0 && game_state->player_first_boosted_cell <= 63)
    {
        assert((position.is_fir_mask & (1ULL << game_state->player_first_boosted_cell)) && "Boost must be on top of a card (player_first_boosted_cell)");

        position.is_boosted_mask |= (1ULL << game_state->player_first_boosted_cell);
    }

    if (game_state->player_second_boosted_cell >= 0 && game_state->player_second_boosted_cell <= 63)
    {
        assert((position.is_sec_mask & (1ULL << game_state->player_second_boosted_cell)) && "Boost must be on top of a card (player_second_boosted_cell)");

        position.is_boosted_mask |= (1ULL << game_state->player_second_boosted_cell);
    }

    if (game_state->player_first_firewalled_cell >= 0 && game_state->player_first_firewalled_cell <= 63)
    {
        position.firewall_fir = (game_state->player_first_firewalled_cell << 1) | 1;

        assert(((1ULL << game_state->player_first_firewalled_cell) & (position.is_sec_mask | 1729382256910270488ULL)) == 0 && "Firewall cant be on top of enemy cards or exit squares (player_first_firewalled_cell)");
    }

    if (game_state->player_second_firewalled_cell >= 0 && game_state->player_second_firewalled_cell <= 63)
    {
        position.firewall_sec = (game_state->player_second_firewalled_cell << 1) | 1;

        assert(((1ULL << game_state->player_second_firewalled_cell) & (position.is_fir_mask | 1729382256910270488ULL)) == 0 && "Firewall cant be on top of enemy cards or exit squares (player_second_firewalled_cell)");
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

    return position;
}

#define CHECK_WIN()                    \
    if (pos.sec_link == 4)             \
    {                                  \
        __builtin_printf("Player one wins!\n");  \
        pos.print_field();             \
        break;                         \
    }                                  \
    else if (pos.sec_virus == 4)       \
    {                                  \
        __builtin_printf("Player one loses!\n"); \
        pos.print_field();             \
        break;                         \
    }                                  \
    else if (pos.fir_link == 4)        \
    {                                  \
        __builtin_printf("Player two wins!\n");  \
        pos.print_field();             \
        break;                         \
    }                                  \
    else if (pos.fir_virus == 4)       \
    {                                  \
        __builtin_printf("Player one loses!\n"); \
        pos.print_field();             \
        break;                         \
    }

void simulate_game(generic_representation *init_game_state, bool player_to_move, int analysis_depth)
{
    field_t pos = generic_representation_to_intern(init_game_state);

    pos.print_field();

    for (;;)
    {
        struct timespec start_it, stop_it;

        __builtin_printf("pos: ");
        intern_to_generic_representation(&pos).format();

        clock_gettime(CLOCK_MONOTONIC, &start_it);
        minimax_main_result_t move = minimax_iteration_main(analysis_depth, INT64_MAX, MIN, MAX, player_to_move, &pos);
        player_to_move = !player_to_move;
        clock_gettime(CLOCK_MONOTONIC, &stop_it);

        __builtin_printf("Maximized score: %d      %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);

        pos = move.best_field;
        pos.print_field();
        __builtin_printf("\n");

        CHECK_WIN();

        __builtin_printf("pos: ");
        intern_to_generic_representation(&pos).format();

        clock_gettime(CLOCK_MONOTONIC, &start_it);
        move = minimax_iteration_main(analysis_depth, INT64_MAX, MIN, MAX, player_to_move, &pos);
        player_to_move = !player_to_move;
        clock_gettime(CLOCK_MONOTONIC, &stop_it);

        __builtin_printf("Minimized score: %d      %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);

        pos = move.best_field;
        pos.print_field();
        __builtin_printf("\n");

        CHECK_WIN();

#ifdef BRANCH_DEBUG
        clear_branch_tracker();
        for (int i = 0; i < _total_branch_count; ++i)
        {
            __builtin_printf("branch=%4d (%s), total entries: %ld, cutoffs: %ld, cutoff%%=%f, recursion_cost=%ld\n", i + 1, cutoff_tracker[i].msg, cutoff_tracker[i].total_entries, cutoff_tracker[i].cutoff_entries, (double)cutoff_tracker[i].cutoff_entries * 100.0 / (double)cutoff_tracker[i].total_entries, cutoff_tracker[i].recursion_cost);
        }
#endif
    }
}

#undef CHECK_WIN

void rnab_benchmark()
{
#ifdef BRANCH_DEBUG
    clear_branch_tracker();
#endif

    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < sizeof(benchmark_array) / sizeof(benchmark_array[0]); ++i)
    {
        field_t pos = generic_representation_to_intern(&benchmark_array[i].position);

        pos.print_field();

        minimax_iteration_main(benchmark_array[i].depth, INT64_MAX, MIN, MAX, true, &pos).best_field.print_field();

        pos = pos.reverse_field();
        pos.print_field();

        minimax_iteration_main(benchmark_array[i].depth, INT64_MAX, MIN, MAX, false, &pos).best_field.print_field();

#ifdef BRANCH_DEBUG
        for (int i = 0; i < _total_branch_count; ++i)
        {
            __builtin_printf("branch=%4d (%s), total entries: %ld, cutoffs: %ld, cutoff%%=%f, recursion_cost=%ld\n", i + 1, cutoff_tracker[i].msg, cutoff_tracker[i].total_entries, cutoff_tracker[i].cutoff_entries, (double)cutoff_tracker[i].cutoff_entries * 100.0 / (double)cutoff_tracker[i].total_entries, cutoff_tracker[i].recursion_cost);
        }

        for (int i = 0; i < _total_branch_count / 2; ++i)
        {
            if (cutoff_tracker[i].total_entries != cutoff_tracker[i + _total_branch_count / 2].total_entries)
            {
                __builtin_printf("cutoff tracker total_entries missmatch\n");
                exit(1);
            }
            if (cutoff_tracker[i].cutoff_entries != cutoff_tracker[i + _total_branch_count / 2].cutoff_entries)
            {
                __builtin_printf("cutoff tracker cutoff_entries missmatch\n");
                exit(1);
            }
            if (cutoff_tracker[i].recursion_cost != cutoff_tracker[i + _total_branch_count / 2].recursion_cost)
            {
                __builtin_printf("cutoff tracker recursion_cost missmatch\n");
                exit(1);
            }
            __builtin_printf("branch=%4d (%s), total entries: %ld, cutoffs: %ld, cutoff%%=%f, recursion_cost=%ld\n", i + 1, cutoff_tracker[i].msg, cutoff_tracker[i].total_entries, cutoff_tracker[i].cutoff_entries, (double)cutoff_tracker[i].cutoff_entries * 100.0 / (double)cutoff_tracker[i].total_entries, cutoff_tracker[i].recursion_cost);
        }
        //clear_branch_tracker();
#endif
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    __builtin_printf("Compute time: %ld\n", (stop.tv_sec * 1000000000l + stop.tv_nsec - start.tv_sec * 1000000000l - start.tv_nsec) / 1000000);
}

extern "C" EXPORT_API void rnab_compute_best_move(generic_representation *game_state, int32_t max_depth, int64_t max_search_time, int32_t player)
{
    field_t position = generic_representation_to_intern(game_state);
    minimax_main_result_t res = minimax_iteration_main(max_depth, max_search_time, MIN, MAX, player, &position);
    *game_state = intern_to_generic_representation(&res.best_field);
    return;
}
