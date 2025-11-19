#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4



void screen_refresh(board_t * game_board, int mode) {
    debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

int play_board(board_t * game_board) {
    pacman_t* pacman = &game_board->pacmans[0];
    command_t* play;
    if (pacman->n_moves == 0) { // if is user input
        command_t c; 
        c.command = get_input();

        if(c.command == '\0')
            return CONTINUE_PLAY;

        c.turns = 1;
        play = &c;
    }
    else { // else if the moves are pre-defined in the file
        // avoid buffer overflow wrapping around with modulo of n_moves
        // this ensures that we always access a valid move for the pacman
        play = &pacman->moves[pacman->current_move%pacman->n_moves];
    }

    debug("KEY %c\n", play->command);

    if (play->command == 'Q') {
        return QUIT_GAME;
    }

    int result = move_pacman(game_board, 0, play);
    if (result == REACHED_PORTAL) {
        // Next level
        return NEXT_LEVEL;
    }

    if(result == DEAD_PACMAN) {
        return QUIT_GAME;
    }
    
    for (int i = 0; i < game_board->n_ghosts; i++) {
        ghost_t* ghost = &game_board->ghosts[i];
        // avoid buffer overflow wrapping around with modulo of n_moves
        // this ensures that we always access a valid move for the ghost
        move_ghost(game_board, i, &ghost->moves[ghost->current_move%ghost->n_moves]);
    }

    if (!game_board->pacmans[0].alive) {
        return QUIT_GAME;
    }      

    return CONTINUE_PLAY;  
}


/*
* Opens the directory given
* @param level_directory path of the directory given in argv
* @return pointer to the dir stream;
*/
DIR* handle_input(char* level_directory) {
    DIR *dir = opendir(level_directory);

    if(!dir){
        perror("opendir");
        return 1;
    }
    
    return dir;
}   

//não sei qual deve ser o output da função
void handle_files(DIR* dirStream){
    struct dirent *dp;

    for(;;){ //iterate thru all files in dir
        errno = 0;
        dp = readdir(dirStream);
        if(dp == NULL) {
            break;
        }

        //decidir se se adiciona verificação para saltar os ficheiros com nome . e .. (pq isso o case já faz)

        //case para saber que função deve ler
        //caso seja .lvl
        //caso seja .m ou .p
        char *extension = strchr(dp->d_name, '.'); //file extension
        if(strcmp(extension, ".lvl")){
            //função de parse para lvl
            //nestas funções passar a struct dirent para poder dar open;
            continue;
        }
        else if(strcmp(extension, ".m")){
            //função de parse para monstro
            continue;
        }
        else if(strcmp(extension, ".p")){
            //função de parse para pacman 
            //a função para o pacman e monstros pode ser igual 
            //(talvez adicionar flag para distinguir a struct a usar)
            continue;
        }

    }
    if(errno != 0){
        perror("readdir");
        closedir(dirStream);
        return 1;
    }

    closedir(dirStream); //é preciso verificar se close foi feito com sucesso?

    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <level_directory>\n", argv[0]);
        // TODO receive inputs
        handle_input(argv[1]); //recebe o diretório dos níveis
    }

    // Random seed for any random movements
    srand((unsigned int)time(NULL));

    open_debug_file("debug.log");

    terminal_init();
    
    int accumulated_points = 0;
    bool end_game = false;
    board_t game_board; //aqui colocar função de parse para recolher os file inputs

    while (!end_game) {
        load_level(&game_board, accumulated_points);
        draw_board(&game_board, DRAW_MENU);
        refresh_screen();

        while(true) {
            int result = play_board(&game_board); 

            if(result == NEXT_LEVEL) {
                screen_refresh(&game_board, DRAW_WIN);
                sleep_ms(game_board.tempo);
                break;
            }

            if(result == QUIT_GAME) {
                screen_refresh(&game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board.tempo);
                end_game = true;
                break;
            }
    
            screen_refresh(&game_board, DRAW_MENU); 

            accumulated_points = game_board.pacmans[0].points;      
        }
        print_board(&game_board);
        unload_level(&game_board);
    }    

    terminal_cleanup();

    close_debug_file();

    return 0;
}
