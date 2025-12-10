#include "board.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
FILE * debugfile;


void locksOrder(int new_index, int old_index, board_t* board){
    if(new_index > old_index){
        pthread_rwlock_wrlock(&board->board[new_index].lock); //lock posição de destino
        pthread_rwlock_wrlock(&board->board[old_index].lock); //lock posição atual
    }
    else{
        pthread_rwlock_wrlock(&board->board[old_index].lock); //lock posição atual
        pthread_rwlock_wrlock(&board->board[new_index].lock); //lock posição de destino
    }
}

void unlockOrder(int new_index, int old_index, board_t* board){
    if(new_index > old_index){
        pthread_rwlock_unlock(&board->board[old_index].lock); //lock posição de saída
        pthread_rwlock_unlock(&board->board[new_index].lock);
    }
    else{
        pthread_rwlock_unlock(&board->board[new_index].lock); //lock posição de saída
        pthread_rwlock_unlock(&board->board[old_index].lock);
    }
}
// Helper private function to find and kill pacman at specific position
static int find_and_kill_pacman(board_t* board, int new_x, int new_y) {
    for (int p = 0; p < board->n_pacmans; p++) {
        pacman_t* pac = &board->pacmans[p];
        pthread_rwlock_rdlock(&pac->lock);
        int px = pac->pos_x;
        int py = pac->pos_y;
        int alive = pac->alive;
        if (px == new_x && py == new_y && alive) {
            pthread_rwlock_unlock(&pac->lock);
            pthread_rwlock_wrlock(&pac->lock);
            pac->alive = 0;
            pthread_rwlock_unlock(&pac->lock);
            kill_pacman(board, p);
            return DEAD_PACMAN;
        }
        pthread_rwlock_unlock(&pac->lock);
    }
    return VALID_MOVE;
}

// Helper private function for getting board position index
//FIXMEEEEE
int get_board_index(board_t* board, int x, int y) {
    return y * board->width + x;
}

// Helper private function for checking valid position
static inline int is_valid_position(board_t* board, int x, int y) {
    return (x >= 0 && x < board->width) && (y >= 0 && y < board->height); // Inside of the board boundaries
}

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int move_pacman(board_t* board, int pacman_index, command_t* command) {

    pthread_rwlock_rdlock(&board->pacmans[pacman_index].lock);
    if (pacman_index < 0 || !board->pacmans[pacman_index].alive) {
        pthread_rwlock_unlock(&board->pacmans[pacman_index].lock);
        return DEAD_PACMAN; // Invalid or dead pacman
    }
    pthread_rwlock_unlock(&board->pacmans[pacman_index].lock);


    pacman_t* pac = &board->pacmans[pacman_index];
    debug("%d %d\n", pac->pos_x ,pac->pos_y);
    pthread_rwlock_wrlock(&pac->lock);
    int new_x = pac->pos_x;
    int new_y = pac->pos_y;
    pthread_rwlock_unlock(&pac->lock);

    // check passo
    if (pac->waiting > 0) {
        pac->waiting -= 1;
        return VALID_MOVE;        
    }
    pac->waiting = pac->passo;

    char direction = command->command;

    if (direction == 'R') {
        char directions[] = {'W', 'S', 'A', 'D'};
        direction = directions[rand() % 4];
    }

    // Calculate new position based on direction
    switch (direction) {
        case 'W': // Up
            new_y--;
            break;
        case 'S': // Down
            new_y++;
            break;
        case 'A': // Left
            new_x--;
            break;
        case 'D': // Right
            new_x++;
            break;
        case 'T': // Wait
            if (command->turns_left == 1) {
                pac->current_move += 1; // move on
                command->turns_left = command->turns;
            }
            else command->turns_left -= 1;
            return VALID_MOVE;
        default:
            return INVALID_MOVE; // Invalid direction
    }

    // Logic for the WASD movement
    pac->current_move+=1;

    // Check boundaries
    if (!is_valid_position(board, new_x, new_y)) {
        debug("%d %d\n",new_x, new_y);
        return INVALID_MOVE;
    }



    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, pac->pos_x, pac->pos_y);

    locksOrder(new_index, old_index, board);

    char target_content = board->board[new_index].content;
 
    if (board->board[new_index].has_portal) {
        board->board[old_index].content = 'o';
        board->board[new_index].content = 'P';
        unlockOrder(new_index, old_index, board);
        return REACHED_PORTAL;
    }

    // Check for walls
    if (target_content == 'X') {
        unlockOrder(new_index, old_index, board);
        return INVALID_MOVE;
    }

    // Check for ghosts
    if (target_content == 'M') {
        kill_pacman(board, pacman_index);
        unlockOrder(new_index, old_index, board);
        return DEAD_PACMAN;
    }

    // Collect points
    if (board->board[new_index].has_dot) {
        pac->points++;
        board->board[new_index].has_dot = 0;
    }

    board->board[old_index].content = 'o';
    pac->pos_x = new_x;
    pac->pos_y = new_y;
    board->board[new_index].content = 'P';
    unlockOrder(new_index, old_index, board);

    sleep_ms(board->tempo);
    return VALID_MOVE;
}

// Helper private function for charged ghost movement in one direction
static int move_ghost_charged_direction(board_t* board, ghost_t* ghost, char direction, int* new_x, int* new_y) {
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    *new_x = x;
    *new_y = y;
    
    switch (direction) {
        case 'W': // Up
            if (y == 0) return INVALID_MOVE;
            *new_y = 0; // In case there is no colision
            for (int i = y - 1; i >= 0; i--) {
                pthread_rwlock_wrlock(&board->board[get_board_index(board, x, i)].lock);
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'X' || target_content == 'M') {
                    *new_y = i + 1; // stop before colision
                    pthread_rwlock_unlock(&board->board[get_board_index(board, x, i)].lock);
                    return VALID_MOVE;
                }
                else if (target_content == 'P') {
                    *new_y = i;
                    int result = find_and_kill_pacman(board, *new_x, *new_y);
                    pthread_rwlock_unlock(&board->board[get_board_index(board, x, i)].lock);
                    return result;
                }
                pthread_rwlock_unlock(&board->board[get_board_index(board, x, i)].lock);
            }
            break;

        case 'S': // Down
            if (y == board->height - 1) return INVALID_MOVE;
            *new_y = board->height - 1; // In case there is no colision
            for (int i = y + 1; i < board->height; i++) {
                pthread_rwlock_wrlock(&board->board[get_board_index(board, x, i)].lock);
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'X' || target_content == 'M') {
                    *new_y = i - 1; // stop before colision
                    pthread_rwlock_unlock(&board->board[get_board_index(board, x, i)].lock);
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_y = i;
                    int result = find_and_kill_pacman(board, *new_x, *new_y);
                    pthread_rwlock_unlock(&board->board[get_board_index(board, x, i)].lock);
                    return result;
                }
                pthread_rwlock_unlock(&board->board[get_board_index(board, x, i)].lock);
            }
            break;

        case 'A': // Left
            if (x == 0) return INVALID_MOVE;
            *new_x = 0; // In case there is no colision
            for (int j = x - 1; j >= 0; j--) {
                pthread_rwlock_wrlock(&board->board[get_board_index(board, j, y)].lock);
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'X' || target_content == 'M') {
                    *new_x = j + 1; // stop before colision
                    pthread_rwlock_unlock(&board->board[get_board_index(board, j, y)].lock);
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    int result = find_and_kill_pacman(board, *new_x, *new_y);
                    pthread_rwlock_unlock(&board->board[get_board_index(board, j, y)].lock);
                    return result;
                }
                pthread_rwlock_unlock(&board->board[get_board_index(board, j, y)].lock);
            }
            break;

        case 'D': // Right
            if (x == board->width - 1) return INVALID_MOVE;
            *new_x = board->width - 1; // In case there is no colision
            for (int j = x + 1; j < board->width; j++) {
                pthread_rwlock_wrlock(&board->board[get_board_index(board, j, y)].lock);
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'X' || target_content == 'M') {
                    *new_x = j - 1; // stop before colision
                    pthread_rwlock_unlock(&board->board[get_board_index(board, j, y)].lock);
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    int result = find_and_kill_pacman(board, *new_x, *new_y);
                    pthread_rwlock_unlock(&board->board[get_board_index(board, j, y)].lock);
                    return result;
                }
                pthread_rwlock_unlock(&board->board[get_board_index(board, j, y)].lock);
            }
            break;
        default:
            debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
            return INVALID_MOVE;
    }

    return VALID_MOVE;
}   

int move_ghost_charged(board_t* board, int ghost_index, char direction) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    int new_x = x;
    int new_y = y;

    ghost->charged = 0; //uncharge

    int result = move_ghost_charged_direction(board, ghost, direction, &new_x, &new_y);

    if (result == INVALID_MOVE) {
        debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
        return INVALID_MOVE;
    }


    // Get board indices
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    int new_index = get_board_index(board, new_x, new_y);

    locksOrder(new_index, old_index, board);

    // Update board - clear old position (restore what was there)
    board->board[old_index].content = 'o'; // Or restore the dot if ghost was on one
    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;
    // Update board - set new position
    pthread_mutex_lock(&board->ncurses_lock);
    board->board[new_index].content = 'M';
    pthread_mutex_unlock(&board->ncurses_lock);
    unlockOrder(new_index, old_index, board);

    return result;
}

int move_ghost(board_t* board, int ghost_index, command_t* command) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    
    int new_x = ghost->pos_x;
    int new_y = ghost->pos_y;

    // check passo
    if (ghost->waiting > 0) {
        ghost->waiting -= 1;
        return VALID_MOVE;
    }
    ghost->waiting = ghost->passo;

    char direction = command->command;
    
    if (direction == 'R') {
        char directions[] = {'W', 'S', 'A', 'D'};
        direction = directions[rand() % 4];
    }

    // Calculate new position based on direction
    switch (direction) {
        case 'W': // Up
            new_y--;
            break;
        case 'S': // Down
            new_y++;
            break;
        case 'A': // Left
            new_x--;
            break;
        case 'D': // Right
            new_x++;
            break;
        case 'C': // Charge
            ghost->current_move += 1;
            ghost->charged = 1;
            return VALID_MOVE;
        case 'T': // Wait
            if (command->turns_left == 1) {
                ghost->current_move += 1; // move on
                command->turns_left = command->turns;
            }
            else command->turns_left -= 1;
            return VALID_MOVE;
        default:
            return INVALID_MOVE; // Invalid direction
    }

    // Logic for the WASD movement
    ghost->current_move++;
    if (ghost->charged)
       return move_ghost_charged(board, ghost_index, direction);

    // Check boundaries
    if (!is_valid_position(board, new_x, new_y)) {
        return INVALID_MOVE;
    }

    // Check board position
    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    locksOrder(new_index, old_index, board);

    char target_content = board->board[new_index].content;

    // Check for walls and ghosts
    if (target_content == 'X' || target_content == 'M') {
        unlockOrder(new_index, old_index, board);
        return INVALID_MOVE;
    }

    int result = VALID_MOVE;
    
    // Check for pacman
    if (target_content == 'P') {
        result = find_and_kill_pacman(board, new_x, new_y);
    }
    //pthread_mutex_lock(&board->ncurses_lock);
    // Update board - clear old position (restore what was there)
    board->board[old_index].content = 'o'; // Or restore the dot if ghost was on one
    //pthread_mutex_unlock(&board->ncurses_lock);
    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;

    // Update board - set new position
    board->board[new_index].content = 'M';

    unlockOrder(new_index, old_index, board);
    return result;
}


//É obrigatório ter esta função
//ver onde por locks
//thread a desbloquear a casa onde estava e a bloquear a casa para onde vai
void* ghost_thread(void* thread_data) {
    thread_ghost_t* data = (thread_ghost_t*)thread_data;
    board_t* board = data->board;
    int ghost_index = data->index; 
    ghost_t* ghost = &board->ghosts[ghost_index];
    while(1) { //Arranjar forma de ver se o pacman entrou no portal
        pthread_mutex_lock(&board->state_lock);
        int active = board->active;
        pthread_mutex_unlock(&board->state_lock);
        if (!active) break;
        move_ghost(board, ghost_index, &ghost->moves[ghost->current_move % ghost->n_moves]);

        sleep_ms(board->tempo);
    }
    free(data);
    return NULL;
}

void* pacman_thread(void* thread_data) {
    thread_pacman_t* data = (thread_pacman_t*)thread_data;
    board_t* board = data->board;
    int pac_index = data->index;
    pacman_t* pac = &board->pacmans[pac_index];
    while(board->active && pac->alive) { 
        //acho que é preciso distinguir entre pacman utilizador e file 
        //SE N METER CONTROLO PARA QUANDO É UTILIZADO A DIVISAO VAI DAR ERRO

        move_pacman(board, pac_index, &pac->moves[pac->current_move % pac->n_moves]);
        sleep_ms(board->tempo);
        debug("Pacman dormiu");

    }
    free(data);
    return NULL;
}


void kill_pacman(board_t* board, int pacman_index) {
    debug("Killing %d pacman\n\n", pacman_index);
    pacman_t* pac = &board->pacmans[pacman_index];
    pthread_rwlock_rdlock(&pac->lock);
    int index = pac->pos_y * board->width + pac->pos_x;
    pthread_rwlock_unlock(&pac->lock);

    //sempre que chamamos esta função já temos o lock para esta posição
    board->board[index].content = 'o';
    //pthread_mutex_unlock(&board->ncurses_lock);
    
    // Mark pacman as dead
    pthread_rwlock_wrlock(&pac->lock);
    pac->alive = 0;
    pthread_rwlock_unlock(&pac->lock);
}

//aux 1ª casa livre
int findFirstFreeSpot(board_t* board){
    //ver caso n haja free spot ? 
    int freeIndex = 0;
    for(int spot = 0; spot < (board->height * board->width) ; spot++){
    
        if(board->board[spot].content == 'o' && board->board[spot].has_dot){
            freeIndex = spot;
            break;
        }
    }
    //o programa corre aqui ainda
    return freeIndex;
}



//Loading pacman points
int load_pacman(board_t* board, int points) {
    if(!strcmp(board->pacman_file, "")){
        board->n_pacmans = 1;
        board->pacmans = (pacman_t*)calloc(board->n_pacmans, sizeof(pacman_t));
        pacman_t *pacman = (pacman_t*)calloc(1, sizeof(pacman_t)); // Cria um Pacman inicializado
        int startIndex = findFirstFreeSpot(board);
        pacman->pos_y = startIndex / board->width;
        pacman->pos_x = startIndex % board->width;
        debug("%d %d\n", pacman->pos_x, pacman->pos_y);
        pacman->alive = 1;
        board->board[startIndex].content = 'P';
        board->pacmans[0] = *pacman;
        pthread_rwlock_init(&pacman->lock, NULL);


    }else{
        
        board->board[get_board_index(board,board->pacmans[0].pos_x, board->pacmans[0].pos_y)].content = 'P'; // Pacman
        board->pacmans[0].points = points;
    }
    return 0;
}

int load_ghost(board_t* board, ghost_t* ghost){
    
    board->board[get_board_index(board, ghost->pos_x, ghost->pos_y)].content = 'M';

    return 0;
}

int load_level(board_t *board, int points) {
    load_pacman(board, points);
    for(int i = 0; i < board->n_ghosts; i++){
        load_ghost(board, &board->ghosts[i]);
    }
    return 0;
}



//free level
void freeLevel(board_t *level){
    if (level->board != NULL){
        for(int i = 0; i < level->width * level->height; i++){
            pthread_rwlock_destroy(&level->board[i].lock);
            
        }
        //pthread_mutex_destroy(&level->ncurses_lock); // destruir o lock
        free(level->board); 
    }
    if (level->pacmans != NULL) free(level->pacmans);
    if (level->ghosts != NULL) free(level->ghosts);
    if (level->tid != NULL) free(level->tid);
    pthread_mutex_destroy(&level->ncurses_lock); // destruir o ncurses lock
    free(level);
}
void unload_level(board_t *level) {
    freeLevel(level);
    
}
void unload_allLevels(board_t **levels, int currentLevel){
    for(int level = currentLevel; levels[level]; level++){
        levels[level] = NULL;
    }
    free(levels);
}

void open_debug_file(char *filename) {
    debugfile = fopen(filename, "w");
}

void close_debug_file() {
    fclose(debugfile);
}

void debug(const char * format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(debugfile, format, args);
    va_end(args);

    fflush(debugfile);
}

void print_board(board_t *board) {
    if (!board || !board->board) {
        debug("[%d] Board is empty or not initialized.\n", getpid());
        return;
    }

    // Large buffer to accumulate the whole output
    char buffer[8192];
    size_t offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "=== [%d] LEVEL INFO ===\n"
                       "Dimensions: %d x %d\n"
                       "Tempo: %d\n"
                       "Pacman file: %s\n",
                       getpid(), board->height, board->width, board->tempo, board->pacman_file);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "Monster files (%d):\n", board->n_ghosts);

    for (int i = 0; i < board->n_ghosts; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "  - %s\n", board->ghosts_files[i]);
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n=== BOARD ===\n");

    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            int idx = y * board->width + x;
            if (offset < sizeof(buffer) - 2) {
                buffer[offset++] = board->board[idx].content;
            }
        }
        if (offset < sizeof(buffer) - 2) {
            buffer[offset++] = '\n';
        }
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "==================\n");

    buffer[offset] = '\0';

    debug("%s", buffer);
}
