#include "board.h"
#include "display.h"
#include "game.h"
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


void screen_refresh(board_t * game_board, int mode) {
    pthread_mutex_lock(&game_board->ncurses_lock);
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);    
    pthread_mutex_unlock(&game_board->ncurses_lock);   
}

int play_board(board_t * game_board) {
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

    if (pacman->n_moves == 0) { // if is user input
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
    }

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

char* readFile(int fd, ssize_t* byte_count) {
    char* buffer = NULL;
    if(fd == -1){
        perror("openfile LevelFile");
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1){
        fprintf(stderr, "lseek error\n");
        return NULL; 
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
        fprintf(stderr, "lseek erro\n");
        return NULL;
    }
    
    buffer = calloc(size + 1, sizeof(char));
    if(!buffer){
        fprintf(stderr, "calloc error\n");
        return NULL;
        
    }
 
    ssize_t total = 0;
    while (total < (ssize_t) size) {
        ssize_t bytesRead = read(fd, buffer + total, size - total);
        if (bytesRead == -1) {
            fprintf(stderr, "read file error\n");
            free(buffer);
            return NULL;
        }
        if (bytesRead == 0) {
            break;
        }
        total += bytesRead;
    }
    buffer[total] = '\0'; // null-terminate the string
    *byte_count = total;
    return buffer;
}

char** getBufferLines(char* buffer, ssize_t byte_count, int* line_count){
    char** lines = malloc(sizeof(char*) * (byte_count + 1));
    char* line = strtok(buffer, "\n");
    while (line != NULL) {
        lines[(*line_count)++] = line;
        line = strtok(NULL, "\n");  
    }
    return lines;
}

ghost_t* parseMonster(char* filename, char* dirpath){ 
    ghost_t *monster = NULL;
    char* buffer = NULL;
    char** lines = NULL;
    ssize_t byte_count = 0;
    int line_count = 0;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dirpath, filename); //builds the full path
    int fd = open(path, O_RDONLY);
    if(fd == -1){
        fprintf(stderr, "openfile MonsterFile failed\n");
        return NULL;
    }


    buffer = readFile(fd, &byte_count);
    if(!buffer){
        return NULL;
    }

    lines = getBufferLines(buffer, byte_count, &line_count);

    monster = (ghost_t*)malloc(sizeof(ghost_t));
    if(!monster){
        fprintf(stderr, "Monster malloc failed\n");
        free(lines);
        free(buffer);
        close(fd);
        return NULL;
    }

    monster->charged = false;
    monster->waiting = 0;
    monster->n_moves = 0;
    monster->current_move = 0;
    for(int i = 0; i < line_count; i++) {
        if (strncmp(lines[i], "PASSO", 5) == 0) {
            sscanf(lines[i], "PASSO %d", &monster->passo);
        }
        else if(strncmp(lines[i], "POS", 3) == 0) {
            sscanf(lines[i], "POS %d %d", &monster->pos_x, &monster->pos_y);
        }
        else if(strncmp(lines[i], "#", 1) == 0) {
            continue;
        }
        else {
            sscanf(lines[i], "%c%d", &monster->moves[monster->n_moves].command, &monster->moves[monster->n_moves].turns_left);
            monster->moves[monster->n_moves].turns = 1;
            monster->n_moves++;
        }
    }

    free(lines);
    free(buffer);
    close(fd); //close mosnter file
    return monster;
}

pacman_t* parsePacman(char* filename, char* dirpath){
    pacman_t *pacman = NULL;
    char* buffer = NULL;
    char** lines = NULL;
    ssize_t byte_count = 0;
    int line_count = 0;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dirpath, filename); //builds the full path

    int fd = open(path, O_RDONLY);
    if(fd == -1){
        fprintf(stderr, "openfile PacmanFile failed\n");
        return NULL;
    }

    buffer = readFile(fd, &byte_count);
    if(!buffer){
        return NULL;
    }

    lines = getBufferLines(buffer, byte_count, &line_count);

    pacman = (pacman_t*)malloc(sizeof(pacman_t));
    if(!pacman){
        fprintf(stderr, "Pacman malloc failed\n");
        free(lines);
        free(buffer);
        close(fd);
        return NULL;
    }
    pacman->alive = true;
    pacman->points = 0;
    pacman->waiting = 0;
    pacman->n_moves = 0;
    pacman->current_move = 0;
    pthread_rwlock_init(&pacman->lock, NULL);
    for(int i = 0; i < line_count; i++) {
        if (strncmp(lines[i], "PASSO", 5) == 0) {
            sscanf(lines[i], "PASSO %d", &pacman->passo);
        }
        else if(strncmp(lines[i], "POS", 3) == 0) {
            sscanf(lines[i], "POS %d %d", &pacman->pos_x, &pacman->pos_y);
        }
        else if(strncmp(lines[i], "#", 1) == 0) {
            continue;
        }
        else {
            sscanf(lines[i], "%c %d", &pacman->moves[pacman->n_moves].command, &pacman->moves[pacman->n_moves].turns_left);
            pacman->moves[pacman->n_moves].turns = 1;
            pacman->n_moves++;
        }
    }

    free(lines);
    free(buffer);
    close(fd);
    return pacman;
}

board_t* parseLvl(char* filename, char* dirpath){ 
    char* buffer = NULL;
    char** lines = NULL;
    ssize_t byte_count = 0;
    int line_count = 0;
    int matrix_index = 0;
    board_t *lvl = NULL;
    
    int fd = open(filename, O_RDONLY);
    if(fd == -1){
        fprintf(stderr, "openfile LevelFile failed\n");
        return NULL;
    }
    
    buffer = readFile(fd, &byte_count);
    if(!buffer){
        return NULL;
    }
    
    lines = getBufferLines(buffer, byte_count, &line_count);
   
    lvl = (board_t*)calloc(1, sizeof(board_t));  
    if(!lvl){
        fprintf(stderr, "Calloc new level failed\n");
        free(lines);
        free(buffer);
        close(fd);
        return NULL;
    }
    strcpy(lvl->level_name ,filename);       
    lvl->n_pacmans = 0; 
    lvl->n_ghosts = 0;
    lvl->pacmans = NULL;
    lvl->ghosts = NULL;
    lvl->board = NULL;
    lvl->tid = NULL;
    lvl->active = 1;
    lvl->result = CONTINUE_PLAY;
    lvl->accumulated_points = 0;

    //read line by line
    for (int i = 0; i < line_count; i++) {
        if (strncmp(lines[i], "DIM", 3) == 0) {
            sscanf(lines[i], "DIM %d %d", &lvl->width, &lvl->height);
            lvl->board = malloc(lvl->width * lvl->height * sizeof(board_pos_t));
        }
        else if (strncmp(lines[i], "TEMPO", 5) == 0) 
            sscanf(lines[i], "TEMPO %d", &lvl->tempo);
        
        else if (strncmp(lines[i], "PAC", 3) == 0) { 
            sscanf(lines[i], "PAC %s", lvl->pacman_file);
            pacman_t* tempPacman = parsePacman(lvl->pacman_file, dirpath);
            if(tempPacman){
                lvl->pacmans = realloc(lvl->pacmans, (lvl->n_pacmans + 1) * sizeof(pacman_t));
                lvl->pacmans[lvl->n_pacmans] = *tempPacman;
                free(tempPacman);
                lvl->n_pacmans++;
            }
            else{
                fprintf(stderr, "parsePacman failed\n");
                freeLevel(lvl);
                free(lines);
                free(buffer);
                close(fd);
                return NULL;
            }
        }
        
        else if (strncmp(lines[i], "MON", 3) == 0) {
            char ghost_files[256 * MAX_GHOSTS];
            char *ghost_file;
            char *saveptr;             

            strcpy(ghost_files, lines[i] + 4);   // ignores "MON "

            ghost_file = strtok_r(ghost_files, " ", &saveptr);

            while (ghost_file != NULL) {
                ghost_t* tempGhost = parseMonster(ghost_file, dirpath);
                if(tempGhost){
                    lvl->ghosts = realloc(lvl->ghosts, (lvl->n_ghosts + 1) * sizeof(ghost_t));
                    lvl->ghosts[lvl->n_ghosts] = *tempGhost;
                    free(tempGhost);
                }else{
                    fprintf(stderr, "parseMonster failed\n");
                    freeLevel(lvl);
                    free(lines);
                    free(buffer);
                    close(fd);
                    return NULL;
                } 
                strcpy(lvl->ghosts_files[lvl->n_ghosts], ghost_file);
                lvl->n_ghosts++;

                ghost_file = strtok_r(NULL, " ", &saveptr);
            }
        }   
        else if (strncmp(lines[i], "#", 1) == 0) {
            continue;
        }
        else {
            for(int j = 0; j < lvl->width; j++) {
                pthread_rwlock_init(&lvl->board[matrix_index].lock, NULL);
                lvl->board[matrix_index].content = (lines[i][j] == '@') ? 'o' : lines[i][j];
                lvl->board[matrix_index].has_dot = (lines[i][j] == 'o') ? true : false;
                lvl->board[matrix_index].has_portal = (lines[i][j] == '@') ? true : false;
                matrix_index++;
            }
        }
    }
    //user input pacman initialization
    if(strcmp(lvl->pacman_file, "") == 0){
        userPacman(lvl);
    }
    lvl->tid = malloc(lvl->n_ghosts * sizeof(pthread_t));
    pthread_mutex_init(&lvl->ncurses_lock, NULL);           
    pthread_rwlock_init(&lvl->board_lock, NULL);           

    free(lines);
    free(buffer);
    close(fd); //close level file
    return lvl;
}

void userPacman(board_t* board){
    board->n_pacmans = 1;
    board->pacmans = (pacman_t*)calloc(1, sizeof(pacman_t));
    pacman_t *pacman = &board->pacmans[0];

    int startIndex = findFirstFreeSpot(board);

    pacman->pos_y = startIndex / board->width;
    pacman->pos_x = startIndex % board->width;
    pacman->alive = 1;
    pacman->points = 0;
    pacman->waiting = 0;
    pacman->n_moves = 0;
    pacman->current_move = 0;
    pacman->passo = 0;

    pthread_rwlock_init(&pacman->lock, NULL);

    board->board[startIndex].content = 'P';
}

board_t** handle_files(char* dirpath){   
    DIR *dirStream;   
    struct dirent *dp;
    board_t **levels = NULL;
    int numLevels = 0;

    dirStream = opendir(dirpath);  
    if (dirStream == NULL) {
        fprintf(stderr, "Opendir Failed\n");
        exit(EXIT_FAILURE);
    }
    for(;;){ //iterate thru all files in dir
        errno = 0;
        dp = readdir(dirStream);
        if(dp == NULL) {
            break;
        }

        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue; 

        char *extension = strchr(dp->d_name, '.'); //file extension    
        if(strcmp(extension, ".lvl") == 0){        
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dirpath, dp->d_name);
          
            board_t **tempLevels = realloc(levels, (numLevels + 1) * sizeof(board_t*));  
            if(!tempLevels){
                fprintf(stderr, "realloc new level failed\n");
                closedir(dirStream);
                unload_allLevels(levels);
                exit(EXIT_FAILURE);
            }
            levels = tempLevels; //realoc to original array;
            levels[numLevels] = NULL;
            board_t* tempLevel = parseLvl(path, dirpath); 
            if(tempLevel){
                levels[numLevels] = tempLevel;
            }
            else{
                fprintf(stderr, "parseLvl failed\n");
                closedir(dirStream);
                unload_allLevels(levels);
                exit(EXIT_FAILURE);
            }
            numLevels++;
        }
    }
    

    board_t** tempLevels = realloc(levels, (numLevels + 1) * sizeof(board_t*));
    if(!tempLevels){
        fprintf(stderr, "Realloc new level failed\n");
        closedir(dirStream);
        unload_allLevels(levels); //liberta tudo antes de falhar
        exit(EXIT_FAILURE);
    }else{
        levels = tempLevels;
        levels[numLevels] = NULL;
    }
    


    if(closedir(dirStream) == -1){
        fprintf(stderr, "Closedir Failed\n");
        unload_allLevels(levels);
        exit(EXIT_FAILURE);
    }   

    return levels;
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
    while (1) {
        sleep_ms(board->tempo); 
        int play = play_board(board);
        if (play == CONTINUE_PLAY)
            continue;
        board->result = play;
        free(data);
        return NULL;
    }
}

void start_pacman_thread(board_t* board) {
    thread_pacman_t* thread_data = malloc(sizeof(thread_pacman_t));
    thread_data->board = board;
    pthread_create(&board->pacTid, NULL, pacman_thread, thread_data);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <level_directory>\n", argv[0]);
        // TODO receive inputs
    }
    // Random seed for any random movements
    srand((unsigned int)time(NULL));

    open_debug_file("debug.log");
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

        start_ncurses_thread(game_board);
        start_pacman_thread(game_board);
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
    terminal_cleanup();
    close_debug_file();

    return 0;
}
