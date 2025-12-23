#include "board.h"
#include "api2.h"
#include "display.h"
#include "game.h"
#include "parse.h"
#include "protocol2.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4
#define END_GAME 1
#define PATH_MAX 512
#define PACMAN 1
#define GHOST 2

pthread_t serverId;

void screen_refresh(board_t * game_board, int mode) {
    pthread_mutex_lock(&game_board->ncurses_lock);
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);    
    pthread_mutex_unlock(&game_board->ncurses_lock);   
}

int play_board(board_t * game_board, session_t* game_s) {
    pacman_t* pacman = &game_board->pacmans[0];
    command_t* play;
    command_t c;

    pthread_rwlock_rdlock(&pacman->lock);
    int alive = pacman->alive;
    pthread_rwlock_unlock(&pacman->lock);

    if (!alive) {
        if (*game_board->hasBackup) return LOAD_BACKUP;
        return QUIT_GAME;
    }

    /*if (pacman->n_moves == 0) { // if is user input
        pthread_mutex_lock(&game_board->ncurses_lock);
        c.command = get_input();
        pthread_mutex_unlock(&game_board->ncurses_lock);

        if(c.command == '\0') {
            debug("NO INPUT\n");
            return CONTINUE_PLAY;
        }
        c.turns = 1;
        play = &c;
    }
    else { // else if the moves are pre-defined in the file
        // avoid buffer overflow wrapping around with modulo of n_moves
        // this ensures that we always access a valid move for the pacman
        play = &pacman->moves[pacman->current_move%pacman->n_moves];
    }*/

    //não sei como é que funciona a cena de turns com a implementação com servers
    play->command = get_pacman_command(game_s);
    play->turns = 1;
    play->turns_left = 0;

    debug("KEY %c\n", play->command);

    if (play->command == 'G'){
        if (pacman->n_moves != 0) pacman->current_move++;
        if(!*game_board->hasBackup){
            return CREATE_BACKUP;
        }
        
    }

    if (play->command == 'Q') {
        return QUIT_GAME;
    }

    int result = move_pacman(game_board, 0, play);
    debug("%d\n", result);
    if (result == REACHED_PORTAL) {
        return NEXT_LEVEL;
    }

    if(result == DEAD_PACMAN) {
        if (*game_board->hasBackup) return LOAD_BACKUP;
        return QUIT_GAME;
    }
            
    pthread_rwlock_rdlock(&pacman->lock);
    alive = pacman->alive;

    if (!alive) {
        pthread_rwlock_unlock(&pacman->lock);
        if (*game_board->hasBackup) return LOAD_BACKUP;
        return QUIT_GAME;
    }
    pthread_rwlock_unlock(&pacman->lock);
    return CONTINUE_PLAY;  
}

int createBackup(board_t* board) {
    pid_t pid;
    int status;

    pid = fork();
    if(pid == -1){
        perror("Fork Error");
    }
    *board->hasBackup = 1;
    if(pid == 0){
        return CONTINUE_PLAY;
    }
    else{
        wait(&status);
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code == 0) {
                return END_GAME;
            } 
            else if (code == 1) {
                *board->hasBackup = 0;
            }
        }
    }
    return CONTINUE_PLAY;
}

void* ghost_thread(void* thread_data) {
    thread_ghost_t* data = (thread_ghost_t*)thread_data;
    board_t* board = data->board;
    int ghost_index = data->index; 
    ghost_t* ghost = &board->ghosts[ghost_index];
    int active = 1;
    while(1) {
        sleep_ms(board->tempo);
        move_ghost(board, ghost_index, &ghost->moves[ghost->current_move % ghost->n_moves]);
        pthread_rwlock_rdlock(&board->board_lock);
        active = board->active;
        pthread_rwlock_unlock(&board->board_lock);
        if (!active) {
            break;
        }
    }
    free(data);
    return NULL;
}

void start_ghost_threads(board_t* board) {
    for (int i = 0; i < board->n_ghosts; i++) {
        thread_ghost_t* thread_data = malloc(sizeof(thread_ghost_t));
        thread_data->index = i;
        thread_data->board = board;
        pthread_create(&board->tid[i], NULL, (void*) ghost_thread, thread_data);
    }
}

void stop_ghost_threads(board_t* board) {
    for(int i = 0; i < board->n_ghosts; i++) {
        pthread_join(board->tid[i], NULL);
    }
}

void* ncurses_thread(void* arg) {
    thread_ncurses* data = (thread_ncurses *)arg;
    board_t* board = data->board;
    int active = 1;
    while (1) {
        sleep_ms(board->tempo);
        screen_refresh(board, DRAW_MENU);
        pthread_rwlock_rdlock(&board->board_lock);
        active = board->active;
        pthread_rwlock_unlock(&board->board_lock);
        if (!active) {
            break;
        }
    }
    free(data);
    return NULL;
}

void start_ncurses_thread(board_t* board) {
    thread_ncurses* thread_data = malloc(sizeof(thread_ncurses));
    thread_data->board = board;
    pthread_create(&board->ncursesTid, NULL, (void*) ncurses_thread, thread_data);
}

void* pacman_thread(void* arg) {
    thread_pacman_t* data = arg;
    board_t* board = data->board;
    session_t* game_s = data->game_s;
    while (1) {
        sleep_ms(board->tempo); 
        int play = play_board(board, game_s);
        if (play == CONTINUE_PLAY)
            continue;
        board->result = play;
        free(data);
        return NULL;
    }
}

void start_pacman_thread(board_t* board, session_t* game_s) {
    thread_pacman_t* thread_data = malloc(sizeof(thread_pacman_t));
    thread_data->board = board;
    thread_data->game_s = game_s;
    pthread_create(&board->pacTid, NULL, pacman_thread, thread_data);
}

char* boardToChar(board_t* board){
    char* boardChar = malloc((board->height * board->width) * sizeof(char));
    for(int i = 0, k = 0; i < board->width * board->height; i++, k++){
        boardChar[k] = board->board[i].content;
    }
    return boardChar;
}

void* server_thread(void* arg){
    thread_server_t* data = arg;
    board_t* board = data->board;
    session_t* game_s = data->game_s;
    //atualiza periodicamente
    sleep(5);
    //LOCK TABULEIRO
    //read lock tabuleiro
    //Função para meter tabuleiro em char
    pthread_rwlock_rdlock(&board->board_lock);
    char* boardChar = boardToChar(board);
    write_all(game_s->notif_pipe, boardChar, board->width * board->height);
    pthread_rwlock_unlock(&board->board_lock);
    debug("%s\n", boardChar);
    return NULL;
}

void start_server_thread(board_t* board, session_t* game_s){
    thread_server_t* thread_data = malloc(sizeof(thread_server_t));
    thread_data->board = board;
    thread_data->game_s = game_s;
    pthread_create(&serverId, NULL, server_thread, thread_data);
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr,
            "Usage: %s <levels_dir> <max_games> <register_pipe>\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }
    // Random seed for any random movements
    srand((unsigned int)time(NULL));
    open_debug_file("debug.log");

        //ETAPA 1.1
    //assumir sempre max_games = 1
    int numS = 0;
    session_t* game_s = innit_session(argv[3], &numS, 1);
    
    board_t** levels = handle_files(argv[1]);

    terminal_init();
    int result;
    int indexLevel = 0;
    int tempPoints = 0; //acumulated points between levels
    board_t *game_board = NULL;
    bool end_game = false;
    int* hasBackUp = malloc(sizeof(int));
    *hasBackUp = 0;



    while (!end_game) {

        if (levels[indexLevel] == NULL) {
            end_game = true;
            break; 
        }
        game_board = levels[indexLevel];
        game_board->hasBackup = hasBackUp;

        
        load_level(game_board, hasBackUp, tempPoints); 

        start_server_thread(game_board, game_s);
        start_ncurses_thread(game_board);
        start_pacman_thread(game_board, game_s);
        start_ghost_threads(game_board);

        pthread_join(game_board->pacTid, NULL); //waits for pacman thread to end

        result = game_board->result; //get result of the game

        pthread_rwlock_wrlock(&game_board->board_lock);
        game_board->active = 0;   //stop other threads
        pthread_rwlock_unlock(&game_board->board_lock);
        
        pthread_join(game_board->ncursesTid, NULL);
        stop_ghost_threads(game_board);

        switch (result) {
            case NEXT_LEVEL:
                screen_refresh(game_board, DRAW_WIN);
                sleep_ms(game_board->tempo); 
                tempPoints = game_board->pacmans[0].points;
                indexLevel++;
                break;
            
            case LOAD_BACKUP:
                exit(1);
                break;
            case CREATE_BACKUP:
                tempPoints = game_board->pacmans[0].points;
                end_game = (createBackup(game_board) == 1) ? true : false;
                break;
            case QUIT_GAME:
                screen_refresh(game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board->tempo);
                end_game = true;
                break;
            default:
                break;
        }
        print_board(game_board);
    }
    unload_allLevels(levels);
    free(hasBackUp);
    free_session(game_s);
    terminal_cleanup();
    close_debug_file();

    return 0;
}
