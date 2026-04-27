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
    _printf("(generic_representation){%lluULL,%lluULL,%lluULL,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}\n", r->player_first_card_mask, r->player_second_card_mask, r->link_card_mask, r->player_first_boosted_cell, r->player_second_boosted_cell, r->player_first_firewalled_cell, r->player_second_firewalled_cell, r->player_first_captured_links_num, r->player_first_captured_viruses_num, r->player_second_captured_links_num, r->player_second_captured_viruses_num, r->player_first_is_404_not_found_available, r->player_second_is_404_not_found_available);
}

static void intern_to_generic_representation(const field_t *__restrict__ pos, generic_representation *__restrict__ f_out)
{
    f_out->player_first_card_mask = pos->is_fir_mask;
    f_out->player_second_card_mask = pos->is_sec_mask;

    f_out->link_card_mask = pos->is_link_mask;

    f_out->player_first_boosted_cell = ((pos->is_fir_mask & pos->is_boosted_mask) ? (__builtin_ctzll(pos->is_fir_mask & pos->is_boosted_mask)) : -1);
    f_out->player_second_boosted_cell = ((pos->is_sec_mask & pos->is_boosted_mask) ? (__builtin_ctzll(pos->is_sec_mask & pos->is_boosted_mask)) : -1);

    f_out->player_first_firewalled_cell = ((pos->args.fields.firewall_fir == 0) ? -1 : (pos->args.fields.firewall_fir >> 1));
    f_out->player_second_firewalled_cell = ((pos->args.fields.firewall_sec == 0) ? -1 : (pos->args.fields.firewall_sec >> 1));

    f_out->player_first_captured_links_num = pos->args.fields.fir_link;
    f_out->player_first_captured_viruses_num = pos->args.fields.fir_virus;

    f_out->player_second_captured_links_num = pos->args.fields.sec_link;
    f_out->player_second_captured_viruses_num = pos->args.fields.sec_virus;

    f_out->player_first_is_404_not_found_available = ((pos->args.fields.is_swap_available_fir == 1) ? 1 : 0);
    f_out->player_second_is_404_not_found_available = ((pos->args.fields.is_swap_available_sec == 1) ? 1 : 0);
}

static int32_t generic_representation_to_intern(const generic_representation *__restrict__ game_state, field_t *__restrict__ f_out)
{
    if (!(game_state->player_first_boosted_cell == -1 || (game_state->player_first_boosted_cell >= 0 && game_state->player_first_boosted_cell <= 63)))
        return 2;
    if (!(game_state->player_second_boosted_cell == -1 || (game_state->player_second_boosted_cell >= 0 && game_state->player_second_boosted_cell <= 63)))
        return 3;
    if (!(game_state->player_first_firewalled_cell == -1 || (game_state->player_first_firewalled_cell >= 0 && game_state->player_first_firewalled_cell <= 63)))
        return 4;
    if (!(game_state->player_second_firewalled_cell == -1 || (game_state->player_second_firewalled_cell >= 0 && game_state->player_second_firewalled_cell <= 63)))
        return 5;

    f_out->is_fir_mask = game_state->player_first_card_mask;
    f_out->is_sec_mask = game_state->player_second_card_mask;
    f_out->is_link_mask = game_state->link_card_mask;
    f_out->is_boosted_mask = 0;
    f_out->args.raw = 0;

    // Player one cards may not be on top of player two cards
    if (f_out->is_fir_mask & f_out->is_sec_mask)
        return 6;

    if (game_state->player_first_boosted_cell >= 0 && game_state->player_first_boosted_cell <= 63)
    {
        // Boost must be on top of a card (player_first_boosted_cell)
        if ((f_out->is_fir_mask & (1ULL << game_state->player_first_boosted_cell)) == 0)
            return 7;

        f_out->is_boosted_mask |= (1ULL << game_state->player_first_boosted_cell);
    }

    if (game_state->player_second_boosted_cell >= 0 && game_state->player_second_boosted_cell <= 63)
    {
        // Boost must be on top of a card (player_second_boosted_cell)
        if ((f_out->is_sec_mask & (1ULL << game_state->player_second_boosted_cell)) == 0)
            return 8;

        f_out->is_boosted_mask |= (1ULL << game_state->player_second_boosted_cell);
    }

    if (game_state->player_first_firewalled_cell >= 0 && game_state->player_first_firewalled_cell <= 63)
    {
        // Firewall cant be on top of enemy cards or exit squares (player_first_firewalled_cell)
        if (((1ULL << game_state->player_first_firewalled_cell) & (f_out->is_sec_mask | 1729382256910270488ULL)) != 0)
            return 9;

        f_out->args.fields.firewall_fir = (game_state->player_first_firewalled_cell << 1) | 1;
    }

    if (game_state->player_second_firewalled_cell >= 0 && game_state->player_second_firewalled_cell <= 63)
    {
        // Firewall cant be on top of enemy cards or exit squares (player_second_firewalled_cell)
        if (((1ULL << game_state->player_second_firewalled_cell) & (f_out->is_fir_mask | 1729382256910270488ULL)) != 0)
            return 10;

        f_out->args.fields.firewall_sec = (game_state->player_second_firewalled_cell << 1) | 1;
    }

    f_out->args.fields.is_swap_available_fir = (game_state->player_first_is_404_not_found_available) ? 1 : 0;
    f_out->args.fields.is_swap_available_sec = (game_state->player_second_is_404_not_found_available) ? 1 : 0;

    // We support custom game configurations though the winning conditions are hard coded

    f_out->args.fields.fir_link = game_state->player_first_captured_links_num;
    f_out->args.fields.fir_virus = game_state->player_first_captured_viruses_num;

    f_out->args.fields.sec_link = game_state->player_second_captured_links_num;
    f_out->args.fields.sec_virus = game_state->player_second_captured_viruses_num;

    return 0;
}

#ifdef LIBRNAB_BUILD_LAZY_HANDLERS

extern EXPORT_API int32_t rnab_compute_possible_moves(generic_representation *__restrict__ game_state, generic_representation *__restrict__ out_buffer, int32_t player)
{
    STATIC_BSS field_t position;

    int32_t convert_res = generic_representation_to_intern(game_state, &position);
    if (convert_res != 0)
        return -convert_res;

    (player ? possible_moves_max : possible_moves_min)(position.is_fir_mask, position.is_sec_mask, position.is_link_mask, position.is_boosted_mask, position.args);

    for (int i = 0; i < possible_moves_buf_moves_count; ++i)
    {
        intern_to_generic_representation(&possible_moves_buf.moves[i], &out_buffer[i]);
    }

    return possible_moves_buf_moves_count;
}

extern EXPORT_API void rnab_flip_board(generic_representation *__restrict__ position)
{
    uint64_t fir_mask = position->player_first_card_mask;
    int32_t temp;

    position->player_first_card_mask = reverse_mask(position->player_second_card_mask);
    position->player_second_card_mask = reverse_mask(fir_mask);
    position->link_card_mask = reverse_mask(position->link_card_mask);

    temp = position->player_first_boosted_cell;

    position->player_first_boosted_cell = ((position->player_second_boosted_cell != -1) ? (63 - position->player_second_boosted_cell) : -1);
    position->player_second_boosted_cell = ((temp != -1) ? (63 - temp) : -1);

    temp = position->player_first_firewalled_cell;

    position->player_first_firewalled_cell = ((position->player_second_firewalled_cell != -1) ? (63 - position->player_second_firewalled_cell) : -1);
    position->player_second_firewalled_cell = ((temp != -1) ? (63 - temp) : -1);

    temp = position->player_first_captured_links_num;
    position->player_first_captured_links_num = position->player_second_captured_links_num;
    position->player_second_captured_links_num = temp;

    temp = position->player_first_captured_viruses_num;
    position->player_first_captured_viruses_num = position->player_second_captured_viruses_num;
    position->player_second_captured_viruses_num = temp;

    temp = position->player_first_is_404_not_found_available;
    position->player_first_is_404_not_found_available = position->player_second_is_404_not_found_available;
    position->player_second_is_404_not_found_available = temp;
}

extern EXPORT_API int32_t rnab_compute_starting_position(generic_representation *__restrict__ out_buffer, int32_t player_one_code, int32_t player_two_code)
{
    STATIC_BSS field_t position;

    if (player_one_code > 69 || player_one_code < 0)
        return -1;
    if (player_two_code > 69 || player_two_code < 0)
        return -2;

    init_field(&position, player_one_code, player_two_code);
    intern_to_generic_representation(&position, out_buffer);

    return 0;
}

extern EXPORT_API void rnab_clear_tt()
{
    CLEAR_TT();
}

#endif

extern EXPORT_API int32_t rnab_compute_best_move(generic_representation *__restrict__ game_state, int32_t max_depth, uint32_t max_search_time, int32_t player)
{
    STATIC_BSS field_t position;
    STATIC_BSS minimax_main_result_t res;
    // Depth must be at least 4 (max 30) and even (divisible by 2) for iterative deepening
    // max_search_time must be at least 100 milliseconds
    if (max_depth < 4 || max_depth > 30 ||  max_depth % 2 != 0 || max_search_time < 100)
        return 1;

    int32_t convert_res = generic_representation_to_intern(game_state, &position);
    if (convert_res != 0)
        return convert_res;

    // Terminal conditions are not allowed
    if (game_state->player_first_captured_links_num < 0 || game_state->player_first_captured_links_num > 3 ||
        game_state->player_first_captured_viruses_num < 0 || game_state->player_first_captured_viruses_num > 3 ||
        game_state->player_second_captured_links_num < 0 || game_state->player_second_captured_links_num > 3 ||
        game_state->player_second_captured_viruses_num < 0 || game_state->player_second_captured_viruses_num > 3)
        return 11;

    minimax_iteration_main(max_depth, max_search_time, player, &position, &res);
    intern_to_generic_representation(&res.best_field, game_state);

    return 0;
}

#undef EXPORT_API
