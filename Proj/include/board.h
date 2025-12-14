#ifndef BOARD_H
#define BOARD_H

#include <pthread.h>

#define MAX_MOVES 20
#define MAX_LEVELS 20
#define MAX_FILENAME 256
#define MAX_GHOSTS 25

typedef enum {
    REACHED_PORTAL = 1,
    VALID_MOVE = 0,
    INVALID_MOVE = -1,
    DEAD_PACMAN = -2,
} move_t;

typedef struct {
    char command;
    int turns;
    int turns_left;
} command_t;

typedef struct {
    int pos_x, pos_y; //current position
    int alive; // if is alive
    int points; // how many points have been collected
    int passo; // number of plays to wait before starting
    command_t moves[MAX_MOVES];
    int current_move;
    int n_moves; // number of predefined moves, 0 if controlled by user, >0 if readed from level file
    int waiting;
    pthread_rwlock_t lock; //lock for pacman
} pacman_t;

typedef struct {
    int pos_x, pos_y; //current position
    int passo; // number of plays to wait between each move
    command_t moves[MAX_MOVES];
    int n_moves; // number of predefined moves from level file
    int current_move;
    int waiting;
    int charged;
} ghost_t;

typedef struct {
    char content;   // stuff like 'P' for pacman 'M' for monster/ghost and 'W' for wall
    int has_dot;    // whether there is a dot in this position or not
    int has_portal; // whether there is a portal in this position or not
    pthread_rwlock_t lock; //lock for each board cell
} board_pos_t;




typedef struct {
    int width, height;      // dimensions of the board
    board_pos_t* board;     // actual board, a row-major matrix
    int n_pacmans;          // number of pacmans in the board
    pacman_t* pacmans;      // array containing every pacman in the board to iterate through when processing (Just 1)
    int n_ghosts;           // number of ghosts in the board
    ghost_t* ghosts;        // array containing every ghost in the board to iterate through when processing
    char level_name[256];   //name for the level file to keep track of which will be the next
    char pacman_file[256];  // file with pacman movements
    char ghosts_files[MAX_GHOSTS][256]; // files with monster movements
    int tempo;              // Duration of each play
    pthread_t* tid;         //Thread id of every ghost thread
    pthread_mutex_t ncurses_lock; //lock for ncurses
    pthread_rwlock_t board_lock;  //lock for board
    int active;             //flag to check if level changed
    pthread_t pacTid;       //Thread id of pacman thread
    pthread_t ncursesTid;   //Thread id of ncurses thread
    int result;             //Store flag that decides next game action    
    int* hasBackup;         //Flag to save if there is a backup
    int accumulated_points; //Game points 

} board_t;



/*Makes the current thread sleep for 'int milliseconds' miliseconds*/
void sleep_ms(int milliseconds);

/*Processes a command for Pacman or Ghost(Monster)
*_index - corresponding index in board's pacman_t/ghost_t array
command - command to be processed*/
int move_pacman(board_t* board, int pacman_index, command_t* command);
int move_ghost(board_t* board, int ghost_index, command_t* command);


/*Process the death of a Pacman*/
void kill_pacman(board_t* board, int pacman_index);

/**
 * @brief aux to find the first free cell to place user pacman.
 * @param board pointer to the current board.
 */
int findFirstFreeSpot(board_t* board);

/**
 * @brief loads pacman to the board.
 * @param board pointer to the current level.
 */
void load_pacman(board_t* board);

/**
 * @brief loads a monster to the board.
 * @param board pointer to the current board.
 * @param ghost ghost to be placed.
 */
void load_ghost(board_t* board, ghost_t* ghost);

/**
 * @brief load a level to be played.
 * @param board pointer to level to be loaded. 
 * @param hasBackup flag to control if there is a current game backup. 
 * @param accPoints points from last level.
 */
void load_level(board_t* board, int* hasBackup, int accPoints);

/**
 * @brief Frees level resources.
 * @param level Pointer to the level. 
 */
void freeLevel(board_t *level);

/**
 * @brief Unloads a level from the game.
 * @param level Pointer to the level to unload.
 */
void unload_level(board_t * board);

/**
 * @brief Unloads all levels from the game.
 * @param levels Array of pointers to levels to unload.
 */
void unload_allLevels(board_t **levels);

/**
 * @brief consistent order for locking cells based on index
 * @param new_index index where entity is moving
 * @param old_index index where entity was before moving
 * @param board pointer to current level
 */
void locksOrder(int new_index, int old_index, board_t* board);

/**
 * @brief consistent order for unlocking cells based on index
 * @param new_index index where entity is moving
 * @param old_index index where entity was before moving
 * @param board pointer to current level
 */
void unlockOrder(int new_index, int old_index, board_t* board);

/*Opens the debug file*/
void open_debug_file(char *filename);

/*Closes the debug file*/
void close_debug_file();

/*Writes to the open debug file*/
void debug(const char * format, ...);

/*Writes the board and its contents to the open debug file*/
void print_board(board_t* board);

#endif
