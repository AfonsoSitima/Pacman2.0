#ifndef GAME_H
#define GAME_H

#include "api2.h"
#include "board.h"
#include <semaphore.h>

typedef struct {
    char req_pipe_path[40];
    char notif_pipe_path[40];
} client_request_t;

typedef struct {
    client_request_t* client_request;
    int head; //proximo a sair
    int tail; //proximo a entrar
    int max_size;
    pthread_mutex_t lock;
} p2c_t;  //producer to consumer

typedef struct {
    int index;  
    board_t* board;
} thread_ghost_t;

typedef struct {
    board_t* board; 
} thread_ncurses;

typedef struct {
    board_t* board;
    session_t* game_s;
} thread_pacman_t;

typedef struct{
    board_t* board;
    session_t* game_s;
} thread_server_t;

typedef struct {
    board_t** levels;
    session_t* game_s;
    sem_t* sem_games;
    sem_t* sem_slots;
    p2c_t* producerConsumer;
    int id;
} thread_game_t;

typedef struct {
    char* server_pipe_path;
    p2c_t* producerConsumer;
    sem_t* sem_games;
    sem_t* sem_slots;
} thread_host_t;

/**
 * @brief main game loop
 * @param game_board pointer to current level
 * @param game_s pointer to current session
 * @return result of the game
 */
int play_board(board_t * game_board, session_t* game_s);

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
void start_pacman_thread(board_t* board, session_t* game_s);

/**
 * @brief flux o pacman thread actions
 * @param arg pacman data
 */
void* pacman_thread(void* arg);


#endif