#ifndef GAME_H
#define GAME_H

void start_ghost_threads(board_t* board);
void stop_ghost_threads(board_t* board);

void* ncurses_thread(void* arg);

#endif