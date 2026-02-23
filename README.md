**Rainet Access Battlers AI**
=============================

This project is my attempt to build a strong AI player for **Rai-Net Access Battlers** using the minimax algorithm to pick the best possible move in any given position.

## Project Dependencies

This project is written in **C++** and relies on the following external libraries:

- **GLFW 3** (`libglfw3`)  
  Used for creating the window, handling input, and rendering the simple graphical interface / board visualization.

- **Boost.Unordered**  
  Specifically: `#include <boost/unordered/unordered_flat_map.hpp>`  
  Provides a high-performance, flat hash map implementation used for transposition tables / caching in the search algorithm.

- **wyhash**
  Due to 'Unlicensed' License directly embedded in the project  

  Compilation instructions can be found at: [Compilation Guide](docs/compile_instructions.md)

## Integration

This project can be compiled into a standalone library which can be used anywhere else. See [Integration Guide](docs/rnab_usage.md)

## Current Limitations

This AI is already pretty capable, but the game is surprisingly deep and computationally expensive. Here's what it's currently not great at:

- **Firewall** and **404-Not-Found (swap)** power-ups  
  Because of the explosion in branching factor, the AI only looks ahead a limited number of moves (`COSTLY_POWERUPS_LOOKAHEAD`) when both players have these. Also, firewall is only ever applied to cover **Link cards** right now.

- **Revealed / covered card knowledge**  
  The engine doesn't yet factor in which cards have been revealed or are known to be covered.

- **Full board position setup**  
  A real game position can have up to ~70 possible card combinations per side. Enumerating everything is brutally slow, so analysis times can get long.

- **Exponential complexity**  
  Like most perfect-information games, deeper search = dramatically more computation.

**Targeted Analyzing Depth**
---------------------------

You can set the search depth anywhere from **6** to **16** plies.  
Deeper = stronger play, but also much more memory usage (thanks to caching/transposition tables).

**Sweet spot for most machines:** **10–14**  
Anything beyond 14 usually requires serious RAM and patience.

**Position Representation**
-------------------------

Everything lives inside a compact `field_t` struct:

```
struct field_t
{
    uint64_t is_fir_mask; // first player cards mask
    uint64_t is_sec_mask; // second player cards mask
    uint64_t is_link_mask; // link cards mask
    uint64_t is_boosted_mask; // boosted cards mask

    uint8_t fir_link : 4; // captured links by the first player
    uint8_t sec_link : 4; // captured viruses by the first player
    uint8_t fir_virus : 4; // captured links by the second player
    uint8_t sec_virus : 4; // captured viruses by the second player

    uint8_t is_boost_available_fir : 1;
    uint8_t is_boost_available_sec : 1;
    uint8_t is_checker_available_fir : 1;
    uint8_t is_checker_available_sec : 1;
    uint8_t is_swap_available_fir : 1;
    uint8_t is_swap_available_sec : 1;
    uint8_t is_firewall_available_fir : 1;
    uint8_t is_firewall_available_sec : 1;

    uint8_t forward_adv_fir; // card advancement for the first player
    uint8_t forward_adv_sec; // card advancement for the second player

    // location for the firewalls (mask is computed as 1ULL << firewall_var)
    uint8_t firewall_fir; 
    uint8_t firewall_sec;
}
```

The struct is currently ~40 bytes. With some clever packing (e.g. representing boosted cards with uint8_t's like firewalls), it could shrink to ~32–33 bytes, but it would require more manual bit-twiddling.
We lean heavily on __builtin_ctzll / __builtin_clzll to extract piece locations quickly.

**Position Evaluation**
---------------------

This is the heart of the AI, it decides what "good" looks like. Current version:

1024 * 2<sup>captured_links_player_one</sup> - 2048 * 2<sup>captured_viruses_player_one</sup> - 1024 * 2<sup>captured_links_player_two</sup> + 2048 * 2<sup>captured_viruses_player_two</sup> + player_one_total_advancement - player_two_total_advancement + 2048 * is_swap_available_player_one - 2048 * is_swap_available_player_two

**Player One** (fir) maximizes the score  
**Player Two** (sec) minimizes it

### Key Points
- Capturing **Link** cards is rewarded exponentially.
- Capturing **Virus** cards is punished exponentially.
- The **swap** power-up gets a huge bonus because it's often decisive in the late game, the AI avoids wasting it on small edges.
- Advancement (how far your pieces have moved forward) encourages aggressive play.

This evaluation function can be improved upon, and suggestions are welcome.

**Search Implementations**
---------------------
In main.cpp you'll find two minimax variants:

- **minimax_single_main** — classic single-threaded version (what the AI actually uses right now)
- **minimax_main** — multi-threaded attempt to explore moves faster

Surprisingly, the parallel version often uses way more memory and isn't consistently faster, so single-threaded usually wins for now.

**Future Development**
---------------------

* Better move ordering & pruning to reach deeper searches
* Smarter caching / lower memory usage
* Proper handling of revealed cards and partial knowledge
* A much nicer frontend / GUI (the current one is very bare-bones)
* Implementing more advanced minimax tricks (aspiration windows, iterative deepening refinements, etc.)

**Contribution**
---------------------
I welcome contributions and suggestions to further enhance the AI's capabilities. <br><br>
**Especially interested in**:
* Anyone who wants to make a prettier game interface
* Better ways to explore / order moves
* Evaluation improvements
* Performance wins
