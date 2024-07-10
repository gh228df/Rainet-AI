**Rainet Access Battlers AI**
=============================

This AI project utilizes the minimax algorithm to calculate the best move for a given Rainet Access Battlers position.

**Limitations and Future Improvements**
------------------------------------

While this AI is capable of analyzing positions, there are some limitations to be aware of:

* **Firewall and 404 Not Found Powerup Calculations**: Due to exponentially large computational requirements, the AI cannot calculate these powerups for both players beyond the main branch.
* **Revealed/Covered Cards**: The AI does not take into account revealed or covered cards in its calculations.
* **Board Position Analysis**: To accurately analyze a real board position, the AI would need to consider up to 70 possible card combinations, which is computationally expensive. As a result, the analysis may be slow.
* **Game Complexity**: The game's complexity increases exponentially with exploration depth, making it challenging to optimize the AI's performance.

**Targeted Analyzing Depth**
---------------------------

The AI can analyze positions with a depth set between 4 and 20. However, please note that caching can make the analysis memory-hungry. A recommended analyzing depth is between 10 and 14.

**Position Representation**
-------------------------

The position representation can be found in the `field` struct. The following variables are used as bitboards to represent link/virus cards for both players:

* `firl`, `secl`, `firv`, `secv`

Additionally, `fir` and `sec` arrays are used to represent every card, with link cards stored as the first 4 elements followed by virus cards. Boosted cards should always be stored as the first card in their respective sections (index 0 for boosted link, index 4 for boosted virus).

If you have suggestions for better board representations, please feel free to share them.

**Position Evaluation**
---------------------

A good evaluation function is crucial for the AI's performance. It determines how the AI plays and which moves are selected over others. The current formula returns the y-coordinate of every card, prioritizing forward moves. The formula is:

`y coordinate + 1024 * links + 1024 * opponent viruses - 2048 * viruses - 2048 * opponent links`

This evaluation function can be improved upon, and suggestions are welcome.

**Position Analysis**
---------------------
The position analysis is a multi-pass process that ensures optimal performance:

1. **Initial Pass**: All possible moves are analyzed in parallel with reduced depth to select the best move for further analysis.
2. **Recursive Analysis**: The selected best move is recursively analyzed, starting with all moves at a reduced depth and then applying the minimax scout algorithm to explore positions with full depth in parallel. This approach eliminates unoptimized alpha/beta calculations and accelerates the analysis process.
3. **Final Pass**: The main minimax function analyzes all remaining positions using the minimax scout algorithm in parallel, ensuring optimal performance. Additionally, it detects and handles stalling threads, recalculating them as needed.

**Future Development**
============
* The performance could be improved, like the memory usage and caching algorithm.
* Better analysis for a given position with all cards covered

While there are limitations and areas for improvement, this project aims to provide optimal performance despite the complexity of the game. 

**Contribution**
---------------------
I welcome contributions and suggestions to further enhance the AI's capabilities.
