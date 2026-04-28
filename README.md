**Rainet Access Battlers AI**
=============================

This project is my attempt to build a strong AI player for **Rai-Net Access Battlers** using the minimax algorithm to pick the best possible move in any given position.

## Project Dependencies

This project is written in **C** and relies on the following external libraries:

- **GLFW 3** (`libglfw3`)  
  When simple GUI is selected, used for creating the window, handling input, and rendering the simple graphical interface / board visualization.

- **rapidhash**  
  Provides a high quality hash at a good speed.

- **stb_sprintf**  
  Provides the _printf interface.

## Integration

Compilation instructions can be found at: [Compilation Guide](docs/compile_instructions.md)<br>
The library can also be directly integrated, see: [Direct Integration Guide](docs/rnab_h.md)<br>
This project can be compiled into a standalone library with a simple interface (just 80KB for x86-64) which can be used anywhere else. See [Integration Guide](docs/rnab_usage.md)<br>

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
