#ifndef GAME_H
#define GAME_H

#include "board.h"

typedef struct {
    int index;
    board_t* board;
    ghost_t* ghost;
    command_t* moves;
} thread_ghost_t;

typedef struct {
    board_t* board; 
    int running;
} thread_ncurses;

typedef struct {
    int index;
    int hasBackup;
    board_t* board;
    pacman_t* pacman;
    command_t* moves;
    int result;
} thread_pacman_t;

void stop_ghost_threads(board_t* board);

void* ncurses_thread(void* arg);

void start_ncurses_thread(board_t* board);

void start_pacman_thread(board_t* board);

void* pacman_thread(void* arg);

void userPacman(board_t* board);


#endif