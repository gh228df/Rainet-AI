# RNAB API Usage

`rnab_export.hpp` exports the following key functions and data structures.

## Initialization

Call this immediately after loading the library (`.dll` / `.so`):

```cpp
void rnab_engine_init();
```

## Board Representation

The engine uses a flat `generic_representation` structure to describe the game state. This is both the input to move computation and its return type.

```cpp
struct generic_representation {
    // Player 1 (first) pieces
    int32_t player_first_link_1_pos;
    int32_t player_first_link_2_pos;
    int32_t player_first_link_3_pos;
    int32_t player_first_link_4_pos;
    int32_t player_first_virus_1_pos;
    int32_t player_first_virus_2_pos;
    int32_t player_first_virus_3_pos;
    int32_t player_first_virus_4_pos;

    // Player 2 (second) pieces
    int32_t player_second_link_1_pos;
    int32_t player_second_link_2_pos;
    int32_t player_second_link_3_pos;
    int32_t player_second_link_4_pos;
    int32_t player_second_virus_1_pos;
    int32_t player_second_virus_2_pos;
    int32_t player_second_virus_3_pos;
    int32_t player_second_virus_4_pos;

    // Power-ups & special cells (-1 = not active)
    int32_t player_first_boosted_cell;
    int32_t player_second_boosted_cell;
    int32_t player_first_firewalled_cell;
    int32_t player_second_firewalled_cell;

    // Captured pieces count (displayed at top/bottom)
    int32_t player_first_captured_links_num;
    int32_t player_first_captured_viruses_num;
    int32_t player_second_captured_links_num;
    int32_t player_second_captured_viruses_num;

    // Power-up availability flags (0/1)
    int32_t player_first_is_virus_checker_available;
    int32_t player_second_is_virus_checker_available;
    int32_t player_first_is_404_not_found_available;
    int32_t player_second_is_404_not_found_available;
};
```

### Position Encoding

![Example board 1 - coordinate mapping](https://github.com/user-attachments/assets/cf79d3b8-bf93-4c0e-9441-3089365e6b4a)

- Board is indexed **0–63** (8×8 grid - from rigth to left, bottom to top)
- Unused pieces -> set position to **`-1`**
- Order of links/viruses **does not matter**

**Example** - Second player has links at positions 2, 11, 12 (no 4th link):

![Example board 2 - second player links placement](https://github.com/user-attachments/assets/0e54753f-2f61-4bdc-8bbf-532bc3e105e6)

    generic_representation state = {0};  // zero-initialize
    // ...
    state.player_second_link_1_pos = 2;
    state.player_second_link_2_pos = 11;
    state.player_second_link_3_pos = 12;
    state.player_second_link_4_pos = -1;
    // ...

### Power-ups & Boosts

| Field                              | Meaning                                      | Value when inactive |
|------------------------------------|----------------------------------------------|----------------------|
| `player_first_boosted_cell`        | Boosted cell for player 1                    | `-1`                 |
| `player_second_boosted_cell`       | Boosted cell for player 2                    | `-1`                 |
| `player_first_firewalled_cell`     | Firewalled cell for player 1                 | `-1`                 |
| `player_second_firewalled_cell`    | Firewalled cell for player 2                 | `-1`                 |

### Captured Counts & Power-up Flags

- Captured links/viruses - amount of captured cards for both players
- Virus Checker / 404 Not Found - `0` / `1` (boolean flags)

## Computing the Best Move

    generic_representation rnab_compute_best_move(
        int32_t                 max_depth,
        int64_t                 max_search_time,
        int32_t                 player,
        generic_representation *game_state
    );

### Parameters

- `max_depth`  
  Maximum minimax depth.  
  • Must be ≥ 2  
  • Must be **even** (4, 6, 8, …)

- `max_search_time` (milliseconds)  
  Time budget for search. Actual time may slightly exceed this value.

- `player`  
  • `0` = first player to move  
  • `1` = second player to move

- `game_state`  
  Pointer to a filled `generic_representation` struct

> **Important:** The function includes several runtime assertions that validate input follows game rules. Invalid data will trigger asserts.



