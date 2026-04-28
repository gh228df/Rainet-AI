# Contents

- [Bitboard Design](#bitboard-design)
- [Board Evaluation](#board-evaluation)
- [Move Ordering](#move-ordering)
- [Transposition Table](#transposition-table)
- [Negamax Transformation](#negamax-transformation)
- [Inline Score Computations](#inline-score-computations)
- [Terminal Nodes](#terminal-nodes)

## Bitboard Design

Everything lives inside a compact `field_t` struct:

```
typedef union
{
    uint64_t raw;                    // hack to manipulate raw memory
    struct
    {
        // location for the firewalls, mask is computed as (uint64_t)(firewall_var & 1) << (firewall_var >> 1)
        uint8_t firewall_fir;
        uint8_t firewall_sec;

        uint8_t fir_link;            // captured links by the first player
        uint8_t sec_link;            // captured links by the second player
        uint8_t fir_virus;           // captured viruses by the first player
        uint8_t sec_virus;           // captured viruses by the second player

        bool is_swap_available_fir;  // flag to track swap availability for the first player
        bool is_swap_available_sec;  // flag to track swap availability for the second player
    } fields;
} extra_args_t;

typedef struct
{
    uint64_t is_fir_mask;      // first player cards mask
    uint64_t is_sec_mask;      // second player cards mask
    uint64_t is_link_mask;     // link cards mask
    uint64_t is_boosted_mask;  // boosted cards mask

    extra_args_t args;
} field_t;
```

## Board Evaluation

This is the heart of the AI, it decides what "good" looks like. Current version:

128 * 2<sup>captured_links_player_one</sup> - 128 * 2<sup>captured_links_player_two</sup> + <br>
64 * 2<sup>captured_viruses_player_two</sup> - 64 * 2<sup>captured_viruses_player_one</sup> + <br>
player_one_total_advancement - player_two_total_advancement + <br>
256 * is_swap_available_player_one - 256 * is_swap_available_player_two + <br>
8 * is_firewall_available_fir - 8 * is_firewall_available_sec<br>

With the regular score bound:

```
Max Evaluation:
    (128 << 3) - (128 << 0)                         = 896
    (64 << 3) - (64 << 0)                           = 508
    (7 * 8) - (0)                                   = 56
    256 - 0 + 8 - 0                                 = 264
    Sum                                             = 1664

#define SCORE_REGULAR_MAX 1664
#define SCORE_TERMINAL_BASE (SCORE_REGULAR_MAX + 1)
```

We squeeze everything in 12 bits: [-2047 ~ 2047], leaving almost 400 slots for the terminal states per player.

Terminal states are produced if either of the players wins the game by getting 4 links, which sets evaluation to 
```(SCORE_TERMINAL_BASE + depth) -> maximizer``` <br>
```(-SCORE_TERMINAL_BASE - depth) -> minimizer```

### Key Points
- Capturing **Link** cards is rewarded exponentially.
- Capturing **Virus** cards is punished exponentially (though defending links is prioritized).
- The **swap** power-up gets a huge bonus because it's often decisive in the late game, the AI avoids wasting it on small edges.
- AI should only apply firewall when neccessary, just like swap. AI gains score when firewall is not used, avoiding stale firewall placements.
- Advancement (how far your pieces have moved forward) encourages aggressive play.

This evaluation function can be improved upon, and suggestions are welcome.

## Move Ordering

There is no definitive best move ordering as the game is extremely complex. The best static move order is:

- **Check if any link cards can be deposited** (in the stack area).  
  This almost certainly is the best move and would cause an alpha/beta cutoff.

  <img width="432" height="648" alt="Deposit link cards" src="https://github.com/user-attachments/assets/aab273da-c055-4c14-ac36-6bb069887484" />

---

- **If current player has a boosted card** -> check if any opponent **link cards** can be captured.

  <img width="432" height="648" alt="Capture opponent links" src="https://github.com/user-attachments/assets/cda5104b-aba3-40fc-9a7f-8706d1642f10" />

---

- **If current player has a boosted card** -> check if any opponent **virus cards** can be captured.

  <img width="432" height="648" alt="Capture opponent viruses" src="https://github.com/user-attachments/assets/f80d9684-cf55-4601-94f0-bc0c4265e29b" />

---

- **If the boost powerup is available** -> try boosting each virus card (starting from the most advanced one).  
  Otherwise try moving the boosted card, prioritizing forward moves.

  <img width="432" height="648" alt="Boost or move virus" src="https://github.com/user-attachments/assets/4db119ce-804f-4acc-80b3-3ac07543df3a" />

---

- **For each not boosted virus card** -> check if any links or viruses can be captured.

  <img width="432" height="648" alt="Capture with non-boosted virus" src="https://github.com/user-attachments/assets/839a9370-d9c2-4d90-b1c6-fe94b45d3245" />

---

- **For each not boosted link card** -> check if any links or viruses can be captured.

  <img width="432" height="648" alt="Capture with non-boosted link" src="https://github.com/user-attachments/assets/1d844f22-28a7-453c-8272-81496baeadf6" />

---

- **For each not boosted link card** -> try moving it, prioritizing forward moves.

  <img width="432" height="648" alt="Move non-boosted link" src="https://github.com/user-attachments/assets/b6d2c98d-7ed8-4658-89be-21b8c0efa987" />

---

- **For each not boosted virus card** -> try moving it, prioritizing forward moves.

  <img width="432" height="648" alt="Move non-boosted virus" src="https://github.com/user-attachments/assets/d54c9958-eaef-4231-8c9a-c9ad656b14e1" />

---

- **If firewall powerup is available** -> try applying it on top of each link card, otherwise deactivate it.

  <img width="432" height="648" alt="Apply firewall on links" src="https://github.com/user-attachments/assets/52a39cf6-f041-4fc2-9332-4e91a1a6e7f1" />

---

- **If firewall powerup is available** -> try applying it on a boosted virus card + cells in front (and diagonals), plus in front of the enemy boosted card.

  <img width="432" height="648" alt="Firewall on boosted virus" src="https://github.com/user-attachments/assets/21780baf-72eb-429e-b6b5-50764078b8bd" />

---

- **If swap powerup is available** -> try swapping each virus card with a link card.

  <img width="432" height="648" alt="Swap virus with link" src="https://github.com/user-attachments/assets/5382aeb9-fda8-4635-990c-d81a98d853fc" />

---

- **If boost powerup is available** -> try boosting each link card (starting from the most advanced one), otherwise deactivate the boost.

  <img width="432" height="648" alt="Boost links or deactivate" src="https://github.com/user-attachments/assets/afadfd58-ec1f-48cc-aa86-6533d082867d" />

## Transposition Table

To improve pruning efficiency, we use transposition tables to cache previously explored positions. Since many board states reappear multiple times in the game tree, storing their evaluation and entry type allows us to skip redundant work.

We generate a 64-bit hash for each board using rapidhash. The probability of a hash collision is extremely low, approximately 1 in 18.4 quintillion.

Each transposition table entry is stored in a compact bitboard with the following layout:

```c
uint64_t verification_hash : 41;   // Upper bits of the board hash (for collision detection)
uint32_t entry_type        : 2;    // UPPERBOUND, EXACT, or LOWERBOUND
uint32_t entry_depth       : 5;    // Search depth of this entry (up to 31)
uint32_t entry_best_section: 4;    // Best section that previously caused a cutoff or improved the score
int32_t  entry_eval        : 12;   // Board evaluation, compressed into 12 bits
```

If a retrieved entry does not cause an immediate cutoff, we immediately jump to the best section stored in the entry. <br>
This section is highly likely to produce a cutoff on the current search. <br>
Saving the best section also helps mitigate the static move ordering problem. <br>

## Negamax Transformation

Most Negamax implementations collapse the minimizer and maximizer into a single function by negating the evaluation and search window. However, in this game, maintaining symmetrical move ordering and correctly handling deposit squares requires flipping the entire board state on every recursive call.
Flipping all four bitboards on every ply proved too expensive for performance. As a result, we maintain two separate branches for the maximizing and minimizing player instead of using the standard single-function Negamax approach.

## Inline Score Computations

Instead of computing the board evaluation only at terminal nodes, we update the score incrementally as each move is made. This eliminates the overhead of calling the evaluation function at every leaf node.
While this approach improves performance, it significantly reduces the flexibility and ease of tuning the evaluation function. An alternative would have been to compute the static evaluation once upon reaching a terminal node and then adjust it there, but the inline update method was chosen for its speed.

## Terminal Nodes

When the search reaches depth 1, the next recursive call would immediately return the board evaluation at depth 0. To avoid this unnecessary function call, we handle depth-1 nodes specially.
We extract the depth-1 search into a dedicated function that only explores moves likely to improve the evaluation, primarily link card captures. This allows us to skip iterating over every possible card and focus solely on high-impact moves, further reducing overhead at the leaves.
