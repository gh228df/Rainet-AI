**Rainet Access Battlers AI**
=============================

This project is my attempt to build a strong AI player for **Rai-Net Access Battlers** using the minimax algorithm to pick the best possible move in any given position.

## Project Dependencies

This project is written in **C** and relies on the following external libraries:

- **GLFW 3** (`libglfw3`)  
  When simple GUI is selected, used for creating the window, handling input, and rendering the simple graphical interface / board visualization.

- **rapidhash**  
  Provides a high quality hash at a good speed.

Compilation instructions can be found at: [Compilation Guide](docs/compile_instructions.md)

## Integration

This project can be compiled into a standalone library with a simple interface (just 80KB for x86-64) which can be used anywhere else. See [Integration Guide](docs/rnab_usage.md)

## Current Limitations

This AI is already pretty capable, but the game is surprisingly deep and computationally expensive. 

The main problem is that unlike chess which has a similar complexity, pieces in this game are very slow and it takes 8-10 moves just to reach the opponent. Attack range is also very limited which makes it way harder to prune bad moves as the position remains surprisingly stable.

Here's what it's currently not great at:

- **Firewall** power-up 
  Firewall is only ever applied to cover **Link cards** or a **Boosted Virus** right now.

- **Revealed / covered card knowledge**  
  The engine doesn't yet factor in which cards have been revealed or are known to be covered.

**Targeted Analyzing Depth**
---------------------------

You can set the search depth anywhere from **4** to **30** plies.  
Deeper = stronger play, but also much more nodes explored.

**Sweet spot for most machines:** **10–14**  
Anything beyond 14 usually requires serious patience, though with the time limit you can set any depth target up to 30.

**Position Representation**
-------------------------

Everything lives inside a compact `field_t` struct:

```
typedef union
{
    uint64_t raw;                    // hack to manipulate raw memory
    struct
    {
        // location for the firewalls (mask is computed as 1ULL << firewall_var)
        uint8_t firewall_fir;
        uint8_t firewall_sec;

        uint8_t fir_link;            // captured links by the first player
        uint8_t sec_link;            // captured viruses by the first player
        uint8_t fir_virus;           // captured links by the second player
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

We lean heavily on __builtin_ctzll / __builtin_clzll to extract piece locations quickly, though preferring __builtin_clzll when possible.

**Position Evaluation**
---------------------

This is the heart of the AI, it decides what "good" looks like. Current version:

128 * 2<sup>captured_links_player_one</sup> - 64 * 2<sup>captured_viruses_player_one</sup> - 128 * 2<sup>captured_links_player_two</sup> + 64 * 2<sup>captured_viruses_player_two</sup> + player_one_total_advancement - player_two_total_advancement + 256 * is_swap_available_player_one - 256 * is_swap_available_player_two

**Player One** (fir) maximizes the score  
**Player Two** (sec) minimizes it

### Key Points
- Capturing **Link** cards is rewarded exponentially.
- Capturing **Virus** cards is punished exponentially (though defending links is prioritized).
- The **swap** power-up gets a huge bonus because it's often decisive in the late game, the AI avoids wasting it on small edges.
- Advancement (how far your pieces have moved forward) encourages aggressive play.

This evaluation function can be improved upon, and suggestions are welcome.

**Future Development**
---------------------

* Better move ordering & pruning to reach deeper searches (it looks like the biggest bottleneck is the evaluation function which doesn't generate many cutoffs)
* Proper handling of revealed cards and partial knowledge
* A much nicer frontend / GUI (the current one is very bare-bones)
* Implementing more advanced minimax tricks (SEE, Killer Moves, etc...)

**Contribution**
---------------------
I welcome contributions and suggestions to further enhance the AI's capabilities. <br><br>
**Especially interested in**:
* Anyone who wants to make a prettier game interface
* Better ways to explore / order moves
* Evaluation improvements
* Performance wins
