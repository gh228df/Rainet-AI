#pragma once

#ifdef _WIN32
#define EXPORT_API __declspec(dllimport)
#else
#define EXPORT_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

extern "C"
{
    struct generic_representation
    {
        uint64_t player_first_card_mask;
        uint64_t player_second_card_mask;
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

    void rnab_engine_init();

    void rnab_compute_best_move(generic_representation *game_state, int32_t max_depth, int64_t max_search_time, int32_t player);
}
