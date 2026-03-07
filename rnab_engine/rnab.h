#include "rnab_impl.c"

#ifdef _WIN32
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __attribute__((visibility("default")))
#endif

typedef struct
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
    int32_t player_first_is_404_not_found_available;
    int32_t player_second_is_404_not_found_available;
} generic_representation;

static void print_generic_representation(generic_representation *r)
{
    __builtin_printf("(generic_representation){0x%016" PRIx64 ", 0x%016" PRIx64 ", 0x%016" PRIx64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}\n", r->player_first_card_mask, r->player_second_card_mask, r->link_card_mask, r->player_first_boosted_cell, r->player_second_boosted_cell, r->player_first_firewalled_cell, r->player_second_firewalled_cell, r->player_first_captured_links_num, r->player_first_captured_viruses_num, r->player_second_captured_links_num, r->player_second_captured_viruses_num, r->player_first_is_404_not_found_available, r->player_second_is_404_not_found_available);
}

typedef struct 
{
    generic_representation position;
    int depth;
} benchmark_t;

static const benchmark_t benchmark_array[] = {
    // {(generic_representation){0x0000000840040000, 0x000000000010001a, 0x0000000840100018,30,20,-1,-1,0,3,3,3,1,0}, 18},
    // {(generic_representation){0x0000000000080090, 0x0002000002000008, 0x0002000002080080,19,49,26,10,2,3,2,3,1,0}, 18},
    // {(generic_representation){0x00000000000100a0, 0x0000000000080600, 0x0000000000010680,16,19,15,-1,2,3,2,3,1,0}, 16},
    // {(generic_representation){0x0410048000000000, 0x08000000000000e5, 0x04000400000000a5,58,59,42,-1,0,2,2,2,1,1}, 14},
    {(generic_representation){0xe718000000000000, 0x00000000000018e7, 0xa5000000000000a5, -1, -1, -1, -1, 0, 0, 0, 0, 1, 1}, 16},
    {(generic_representation){0x8008000010000000, 0x2000000000003007, 0x0008000010000005, 28, 61, 51, -1, 2, 0, 2, 3, 0, 1}, 14},
    {(generic_representation){0x8008001000000000, 0x0040000000001027, 0x8008000000000005, 36, 54, 51, -1, 2, 0, 2, 3, 1, 1}, 14},
    {(generic_representation){0x0004000000000420, 0x0400020001000010, 0x0004000001000400, 10, 4, 50, 61, 2, 1, 3, 3, 0, 0}, 18},
    {(generic_representation){0x1000000800000000, 0x0800040001000000, 0x0000040801000000, -1, 42, 35, 52, 2, 3, 3, 3, 1, 1}, 20},
    {(generic_representation){0x4008000010000000, 0x2000000000003007, 0x0008000010000005, 28, 61, 51, 61, 2, 0, 2, 3, 0, 1}, 14},
    {(generic_representation){0xa100000000100000, 0x00000000000000a8, 0xa000000000000020, 20, 5, -1, 5, 3, 1, 2, 3, 1, 1}, 16},
    {(generic_representation){0x8101000000000100, 0x0000004000000004, 0x8000000000000104, 8, 38, 63, 2, 2, 3, 3, 2, 0, 0}, 20},

};

static generic_representation intern_to_generic_representation(field_t *pos)
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

    res.player_first_is_404_not_found_available = ((pos->is_swap_available_fir == 1) ? 1 : 0);
    res.player_second_is_404_not_found_available = ((pos->is_swap_available_sec == 1) ? 1 : 0);

    return res;
}

static field_t generic_representation_to_intern(const generic_representation *game_state)
{
    assert(minimax_iteration_main);
    assert(minimax_main);

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

#define CHECK_WIN()                              \
    if (pos.sec_link == 4)                       \
    {                                            \
        __builtin_printf("Player one wins!\n");  \
        print_field(&pos);                       \
        break;                                   \
    }                                            \
    else if (pos.sec_virus == 4)                 \
    {                                            \
        __builtin_printf("Player one loses!\n"); \
        print_field(&pos);                       \
        break;                                   \
    }                                            \
    else if (pos.fir_link == 4)                  \
    {                                            \
        __builtin_printf("Player two wins!\n");  \
        print_field(&pos);                       \
        break;                                   \
    }                                            \
    else if (pos.fir_virus == 4)                 \
    {                                            \
        __builtin_printf("Player one loses!\n"); \
        print_field(&pos);                       \
        break;                                   \
    }

static void simulate_game(generic_representation *init_game_state, bool player_to_move, int analysis_depth)
{
    field_t pos = generic_representation_to_intern(init_game_state);
    generic_representation temp_rep;

    print_field(&pos);

    for (;;)
    {
        struct timespec start_it, stop_it;

        __builtin_printf("pos: ");
        temp_rep = intern_to_generic_representation(&pos);
        print_generic_representation(&temp_rep);

        clock_gettime(CLOCK_MONOTONIC, &start_it);
        minimax_main_result_t move = minimax_iteration_main(analysis_depth, INT64_MAX, MIN, MAX, player_to_move, &pos);
        player_to_move = !player_to_move;
        clock_gettime(CLOCK_MONOTONIC, &stop_it);

        __builtin_printf("Maximized score: %d      %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);

        pos = move.best_field;
        print_field(&pos);
        __builtin_printf("\n");

        CHECK_WIN();

        __builtin_printf("pos: ");
        temp_rep = intern_to_generic_representation(&pos);
        print_generic_representation(&temp_rep);

        clock_gettime(CLOCK_MONOTONIC, &start_it);
        move = minimax_iteration_main(analysis_depth, INT64_MAX, MIN, MAX, player_to_move, &pos);
        player_to_move = !player_to_move;
        clock_gettime(CLOCK_MONOTONIC, &stop_it);

        __builtin_printf("Minimized score: %d      %ld\n", move.evaluation, (stop_it.tv_sec * 1000000000l + stop_it.tv_nsec - start_it.tv_sec * 1000000000l - start_it.tv_nsec) / 1000000);

        pos = move.best_field;
        print_field(&pos);
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

static void rnab_benchmark()
{
#ifdef BRANCH_DEBUG
    clear_branch_tracker();
#endif

    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < sizeof(benchmark_array) / sizeof(benchmark_array[0]); ++i)
    {
        field_t pos = generic_representation_to_intern(&benchmark_array[i].position);
        field_t temp_field;

        print_field(&pos);

        CLEAR_TT();

        temp_field = minimax_iteration_main(benchmark_array[i].depth, INT64_MAX, MIN, MAX, true, &pos).best_field;
        print_field(&temp_field);

        pos = reverse_field(&pos);
        print_field(&pos);

        CLEAR_TT();

        temp_field = minimax_iteration_main(benchmark_array[i].depth, INT64_MAX, MIN, MAX, false, &pos).best_field;
        print_field(&temp_field);

#ifdef BRANCH_DEBUG
        for (int i = 0; i < _total_branch_count; ++i)
        {
            __builtin_printf("branch=%4d (%s), total entries: %ld, cutoffs: %ld, cutoff%%=%f, recursion_cost=%ld\n", i + 1, cutoff_tracker[i].msg, cutoff_tracker[i].total_entries, cutoff_tracker[i].cutoff_entries, (double)cutoff_tracker[i].cutoff_entries * 100.0 / (double)cutoff_tracker[i].total_entries, cutoff_tracker[i].recursion_cost);
        }
        // clear_branch_tracker();
#endif
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    __builtin_printf("Compute time: %ld\n", (stop.tv_sec * 1000000000l + stop.tv_nsec - start.tv_sec * 1000000000l - start.tv_nsec) / 1000000);
}

extern EXPORT_API void rnab_compute_best_move(generic_representation *game_state, int32_t max_depth, int64_t max_search_time, int32_t player)
{
    field_t position = generic_representation_to_intern(game_state);
    minimax_main_result_t res = minimax_iteration_main(max_depth, max_search_time, MIN, MAX, player, &position);
    *game_state = intern_to_generic_representation(&res.best_field);
    return;
}
