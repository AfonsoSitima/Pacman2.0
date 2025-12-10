#ifndef GAME_H
#define GAME_H

void start_ghost_threads(board_t* board);
void stop_ghost_threads(board_t* board);

void* ncurses_thread(void* arg);

void start_ncurses_thread(board_t* board);

void start_pacman_thread(board_t* board);

void* pacman_thread(void* arg);


#endif