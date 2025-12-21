#include "parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>


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

    monster->charged = 0;
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
    pacman->alive = 1;
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
                lvl->board[matrix_index].has_dot = (lines[i][j] == 'o') ? 1 : 0;
                lvl->board[matrix_index].has_portal = (lines[i][j] == '@') ? 1 : 0;
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
