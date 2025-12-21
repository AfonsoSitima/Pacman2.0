PacmanIST – Operating Systems Project (Part I)

Overview
--------
PacmanIST is a bi-dimensional game developed for the Operating Systems (SO) 2025/26 course at Instituto Superior Técnico. The project extends a provided sequential Pacman game into a multi-threaded, file-driven and process-aware system, applying core OS concepts such as:

- POSIX file system APIs (file descriptors, directories)
- Multi-threading and synchronization (pthreads, mutexes, rwlocks)
- Process management (fork) and state recovery
- Inter-thread coordination with a non-thread-safe UI library (ncurses)

This repository corresponds to Part I of the project, which is divided into three exercises.

Game Description
----------------
The game is played on a rectangular grid (board) composed of:
- Walls (X)
- Free spaces (o)
- Dots (collectable points)
- A portal (@)
- Pacman (C on screen)
- Ghosts (M)

Pacman and ghosts move orthogonally (up, down, left, right). Pacman collects dots, dies when colliding with a ghost, and advances to the next level when crossing a portal.

Implemented Exercises
---------------------

Exercise 1 – File System Interaction
The game loads its configuration from files stored in a directory passed as a command-line argument.
Supported file types: .lvl, .m, .p

Only POSIX system calls are used (open, read, close, opendir, readdir).

Exercise 2 – Pacman Reincarnation
When the user presses G, the current game state is saved using fork().
Only one backup may exist at a time.

Exercise 3 – Multi-Threaded Execution
Each Pacman and ghost runs in its own thread.
ncurses is accessed by a single dedicated thread.

Makefile
--------
make        : builds the project
make run    : runs the game
make clean  : removes binaries and object files

Valgrind
--------
A ncurses suppression file is provided to avoid false positives.