#ifndef GAME_H
#define GAME_H

#include "board.h"

typedef struct {
    int index;
    board_t* board;
} thread_ghost_t;

typedef struct {
    board_t* board; 
} thread_ncurses;

typedef struct {
    board_t* board;
} thread_pacman_t;

/**
 * @brief starts a thread for every ghost
 * @param board pointer to current level
 */
void start_ghost_threads(board_t* board);

/**
 * @brief stops every ghost thread that is running
 * @param board pointer to current level
 */
void stop_ghost_threads(board_t* board);

/**
 * @brief flux of ghosts thread actions 
 * @param thread_data thread data 
 */
void* ghost_thread(void* thread_data);

/**
 * @brief starts a thread dedicated to ncurses
 * @param board pointer to current level
 */
void start_ncurses_thread(board_t* board);

/**
 * @brief flux of ncurses thread actions
 * @param arg ncurses data
 */
void* ncurses_thread(void* arg);

/**
 * @brief starts a thread dedicated to pacman
 * @param board pointer to current level
 */
void start_pacman_thread(board_t* board);

/**
 * @brief flux o pacman thread actions
 * @param arg pacman data
 */
void* pacman_thread(void* arg);


#endif