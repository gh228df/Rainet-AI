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

    void rnab_engine_init();
    
    generic_representation rnab_compute_best_move(int32_t max_depth, int64_t max_search_time, int32_t player, generic_representation *game_state);
}
