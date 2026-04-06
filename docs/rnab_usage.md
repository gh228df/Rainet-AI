# RNAB API Usage

`rnab_export.hpp` exports the following key functions and data structures.

## Board Representation

The engine uses a flat `generic_representation` structure to describe the game state. This is both the input to move computation and its return type.

```cpp
struct generic_representation {
    // Player 1 (first) pieces
    uint64_t player_first_card_mask;

    // Player 2 (second) pieces
    uint64_t player_second_card_mask;

    // Link Mask
    uint64_t link_card_mask;

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
    int32_t player_first_is_404_not_found_available;
    int32_t player_second_is_404_not_found_available;
};
```
(64 bytes in total, make sure all the types match across the languages and there is exactly 64 bytes at the memory address)

### Position Encoding

![Example board 1 - coordinate mapping](https://github.com/user-attachments/assets/cf79d3b8-bf93-4c0e-9441-3089365e6b4a)

- Board is indexed **0–63** (8×8 grid - from rigth to left, bottom to top)

**Example** - Second player has links at positions 2, 11, 12 (no 4th link):

![Example board 2 - second player links placement](https://github.com/user-attachments/assets/0e54753f-2f61-4bdc-8bbf-532bc3e105e6)

    generic_representation state = {0};  // zero-initialize
    // ...
    state.player_second_card_mask = (1ULL << 2) | (1ULL << 11) | (1ULL << 12);
    state.link_card_mask = (1ULL << 2) | (1ULL << 11) | (1ULL << 12); // dont forget to set the link mask
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

    int32_t rnab_compute_best_move(
        generic_representation *game_state
        int32_t                 max_depth,
        uint32_t                max_search_time,
        int32_t                 player
    );

### Parameters

- `game_state`  
  Pointer to a filled `generic_representation` struct. <br>
  After computing the best move, the output will be written back to the 'game_state'

- `max_depth`  
  Maximum minimax depth.  
  • Must be ≥ 4  
  • Must be **even** (4, 6, 8, …)

- `max_search_time` (milliseconds)  
  Time budget for search. Actual time may slightly exceed this value. Pass UINT32_MAX to disable limit.

- `player`  
  • `0` = first player to move  
  • `1` = second player to move

### Error codes

The search function returns the following error codes:

| Error Code | Description |
|------------|-------------|
| `0`        | Success, no error occurred. |
| `1`        | Invalid search parameters. `max_depth` must be at least 4 and even (divisible by 2). `max_search_time` must be at least 100 milliseconds. |
| `2`        | Invalid `player_first_boosted_cell`. Value must be `-1` or in the range `[0, 63]`. |
| `3`        | Invalid `player_second_boosted_cell`. Value must be `-1` or in the range `[0, 63]`. |
| `4`        | Invalid `player_first_firewalled_cell`. Value must be `-1` or in the range `[0, 63]`. |
| `5`        | Invalid `player_second_firewalled_cell`. Value must be `-1` or in the range `[0, 63]`. |
| `6`        | Overlapping player cards. A cell cannot contain both player 1 and player 2 cards (`player_first_card_mask & player_second_card_mask != 0`). |
| `7`        | Invalid boost placement for player 1. The boost must be placed on top of a player 1 card. |
| `8`        | Invalid boost placement for player 2. The boost must be placed on top of a player 2 card. |
| `9`        | Invalid firewall placement for player 1. Firewall cannot be placed on a player 2 card or on an exit square. |
| `10`       | Invalid firewall placement for player 2. Firewall cannot be placed on a player 1 card or on an exit square. |
| `11`       | Invalid captured pieces count. Captured links or viruses for either player must be in the range `[0, 3]`. |
