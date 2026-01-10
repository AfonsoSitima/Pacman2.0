#ifndef GAME_H
#define GAME_H

#include "api2.h"
#include "board.h"
#include <semaphore.h>
#include <signal.h>


typedef struct client{
    char req_pipe_path[40];
    char notif_pipe_path[40];
    int id; //id cliente
    int slot; //index no array de active players
    struct client* next;
} client_request_t;

typedef struct {
    client_request_t* head;
    client_request_t* tail;
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
    p2c_t* producerConsumer;
    session_t** activeClients; // lista dos clients;
    pthread_mutex_t *clientsArrayLock;
    int id;
} thread_game_t;

typedef struct {
    char* server_pipe_path;
    p2c_t* producerConsumer;
    sem_t* sem_games;
    session_t** activeClients; //clients ativos (para depois ver pontuações)
    pthread_mutex_t *clientsArrayLock;
    int maxGames;
} thread_host_t;

typedef struct {
    int id;
    int points;
} score;

/**
 * @brief initializes the producer-consumer structure
 * @param p2c pointer to the producer-consumer structure
 * @param max_games maximum number of games
 */
void innit_p2c(p2c_t* p2c, int max_games);

/**
 * @brief destroys the producer-consumer structure
 * @param p2c pointer to the producer-consumer structure
 */
void destroy_p2c(p2c_t* p2c);

/**
 * @brief adds a client request to the producer-consumer queue
 * @param p2c pointer to the producer-consumer structure
 * @param request pointer to the client request to be added
 */
void enqueue_p2c(p2c_t* p2c, client_request_t* request);

/**
 * @brief removes a client request from the producer-consumer queue
 * @param p2c pointer to the producer-consumer structure
 * @return pointer to the removed client request
 */
client_request_t* pop_p2c(p2c_t* p2c);

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
 * @brief starts a thread dedicated to pacman
 * @param board pointer to current level
 */
void start_pacman_thread(board_t* board, session_t* game_s);

/**
 * @brief flux o pacman thread actions
 * @param arg pacman data
 */
void* pacman_thread(void* arg);

/**
 * @brief get session id from a pipe name (ex: /tmp/%s_request)
 * @return the user Id
 */
int getId(char buf[], int size);

/**
 * @brief converts the board to a char array for sending to client
 * @param board pointer to current level
 * @return char array representing the board
 */
char* boardToChar(board_t* board);

/**
 * @brief function for the server thread
 * @param arg thread data
 */
void* server_thread(void* arg);

/**
 * @brief starts a server thread to handle communication with the client
 * @param board pointer to current level
 * @param game_s pointer to current session
 * @param serverId pointer to store the created thread id
 */
void start_server_thread(board_t* board, session_t* game_s, pthread_t* serverId);

/**
 * @brief function for the game thread
 * @param arg thread data
 */
void* game_thread(void* arg);

/**
 * @brief starts game threads
 * @param max_games maximum number of concurrent games
 * @param gameTids array to store the thread ids
 * @param levels array of levels
 * @param producerConsumer pointer to the producer-consumer structure
 * @param sem_games semaphore to signal available games
 * @param activeClients array of active clients
 * @param clientsArrayLock mutex to protect the activeClients array
*/
 void start_game_threads(int max_games, pthread_t* gameTids, board_t** levels, p2c_t* producerConsumer, sem_t* sem_games, session_t** activeClients, pthread_mutex_t* clientsArrayLock);


/**
 * @brief comparison function for qsort to sort scores
 */
int maxPoints(const void* a, const void* b);
/**
 * @brief function to create the leaderboard
 * @param activeClients array of active clients
 * @param maxGames maximum number of games
 * @param lock mutex to protect the activeClients array
 * @return 0 on success, 1 on failure
 */
int leaderBoard(session_t** activeClients, int maxGames, pthread_mutex_t* lock);

/**
 * @brief function for the host thread
 * @param arg thread data
 */
void* host_thread(void* arg);

/**
 * @brief function to handle SIGUSR1 signal
 */
void handle_SIGUSR1();

/** 
* @brief function to handle shutdown signal 
*/
void handle_shutdown();

#endif