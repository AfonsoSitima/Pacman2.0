#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4
#ifndef PATH_MAX
#define PATH_MAX 512
#endif
#define PACMAN 1
#define GHOST 2




//USAR MAIS ESTA FUNÇÃO PARA LER FICHEIROS
char* readFile(int fd, ssize_t* byte_count, char *filename) { //talvez adicionar o numero de bytes lidos
    char* buffer = NULL;
    if(fd == -1){
        perror("openfile LevelFile");
        //se o programa acabar aqui o que tem de se ter em conta? close's?
        //temos que criar função para dar free a tudo :(
    }

    off_t size = lseek(fd, 0, SEEK_END); //get size of file
    if (size == -1){
        perror("lseek");
        //ver o que fechar
    }

    if (lseek(fd, 0, SEEK_SET) == -1) { //reset file offset to beginning
        perror("lseek");
    }
    
    buffer = malloc(size + 1);
    if(!buffer){
        perror("malloc");
        //ver o que fechar
    }
 
    ssize_t total = 0;
    while (total < (ssize_t) size) {  //fazemos isto em loop porque é boa prática e garantir que lemos tudo
        ssize_t bytesRead = read(fd, buffer, size - total);
        if (bytesRead == -1) {
            perror("read");
            free(buffer);
            //ver o que fechar
            return NULL;
        }
        if (bytesRead == 0) {
            break; // EOF
        }
        total += bytesRead;
    }
    buffer[total] = '\0'; // null-terminate the string
    *byte_count = total;
    return buffer;
}

char** getBufferLines(char* buffer, ssize_t byte_count, int* line_count){
    char** lines = malloc(sizeof(char*) * (byte_count + 1)); //alocar espaço para as linhas (suposição de tamanho máximo)
    char* line = strtok(buffer, "\n"); //separar por linhas
    while (line != NULL) {
        lines[(*line_count)++] = line; //guardar cada linha no array
        line = strtok(NULL, "\n");  
    }
    return lines;
}

ghost_t* parseMonster(char* filename, char* dirpath){ //flag para distinguir se é monstro ou pacman
    ghost_t *monster = NULL;
    char* buffer = NULL;
    char** lines = NULL;
    ssize_t byte_count = 0;
    int line_count = 0;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", dirpath, filename); //construir o path completo

    int fd = open(path, O_RDONLY);

    buffer = readFile(fd, &byte_count, filename);

    lines = getBufferLines(buffer, byte_count, &line_count);

    monster = (ghost_t*)malloc(sizeof(ghost_t));
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
    snprintf(path, sizeof(path), "%s/%s", dirpath, filename); //construir o path completo

    int fd = open(path, O_RDONLY);

    buffer = readFile(fd, &byte_count, filename);

    lines = getBufferLines(buffer, byte_count, &line_count);

    pacman = (pacman_t*)malloc(sizeof(pacman_t));
    pacman->alive = true;
    pacman->points = 0;
    pacman->waiting = 0;
    pacman->n_moves = 0;
    pacman->current_move = 0;
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
            sscanf(lines[i], "%c%d", &pacman->moves[pacman->n_moves].command, &pacman->moves[pacman->n_moves].turns_left);
            pacman->n_moves++;
        }
    }

    free(lines);
    free(buffer);
    close(fd); //close mosnter file
    return pacman;
}


//VER SE HÁ MALLOC ERRORS AQUI
board_t* parseLvl(char* filename, char* dirpath){ //pela forma que estamos a fazer no handle_files
    //novo nível
    char* buffer = NULL;
    char** lines = NULL;
    ssize_t byte_count = 0;
    int line_count = 0;
    int matrix_index = 0;
    board_t *lvl = NULL;  //ver se há malloc error
    
    int fd = open(filename, O_RDONLY);
    
    buffer = readFile(fd, &byte_count, filename);
    
    lines = getBufferLines(buffer, byte_count, &line_count);
   
    lvl = (board_t*)malloc(sizeof(board_t));  //ver se há malloc error
    strcpy(lvl->level_name ,filename); //inicializar o nome do nível
    lvl->n_pacmans = 0; //não sei se pode haver mais que 1 pacman
    lvl->n_ghosts = 0;
    lvl->pacmans = NULL;
    lvl->ghosts = NULL;
    lvl->board = NULL;
    //Ya bro não sei se esta é a melhor solução
    //aqui tamos a ler linha por linha
    for (int i = 0; i < line_count; i++) {
        if (strncmp(lines[i], "DIM", 3) == 0) {
            sscanf(lines[i], "DIM %d %d", &lvl->width, &lvl->height);
            lvl->board = malloc(lvl->width * lvl->height * sizeof(board_pos_t)); //alocar espaço para o board
        }
        else if (strncmp(lines[i], "TEMPO", 5) == 0) 
            sscanf(lines[i], "TEMPO %d", &lvl->tempo);
        
        else if (strncmp(lines[i], "PAC", 3) == 0) { 
            sscanf(lines[i], "PAC %s", lvl->pacman_file);
            lvl->pacmans = realloc(lvl->pacmans, (lvl->n_pacmans + 1) * sizeof(pacman_t));
            lvl->pacmans[0] = *parsePacman(lvl->pacman_file, dirpath); //ver se há malloc error aqui
            lvl->n_pacmans++; //não tenho a certeza se é suposto haver mais que 1 pacman
        }
        
        else if (strncmp(lines[i], "MON", 3) == 0) {
            char ghost_files[256 * MAX_GHOSTS]; //array para guardar os nomes dos ficheiros (como está na struct board_t)
            char *ghost_file;
            
            strcpy(ghost_files, lines[i] + 4); //ignorar o "MON "
            ghost_file = strtok(ghost_files, " ");

            while (ghost_file != NULL) {
                lvl->ghosts = realloc(lvl->ghosts, (lvl->n_ghosts + 1) * sizeof(ghost_t));
                lvl->ghosts[lvl->n_ghosts] = *parseMonster(ghost_file, dirpath); //ver isto
                strcpy(lvl->ghosts_files[lvl->n_ghosts++], ghost_file);
                ghost_file = strtok(NULL, " ");
            }
        }    
        else if (strncmp(lines[i], "#", 1) == 0) {
            continue;
        }
        else {
            for(int j = 0; j < lvl->width; j++) {
                if (lvl->board[matrix_index].content != 'P' && lvl->board[matrix_index].content != 'M') { //se já tiver sido inicializado por um pacman ou monstro, não sobrescrever
                    lvl->board[matrix_index].content = lines[i][j];
                    lvl->board[matrix_index].has_dot = (lines[i][j] == 'o') ? true : false;
                    lvl->board[matrix_index].has_portal = (lines[i][j] == '@') ? true : false;
                }
                matrix_index++;
            }
        }
    }
    free(lines);
    free(buffer);
    close(fd); //close level file
    return lvl;
}


//retornar lista de níveis
board_t** handle_files(char* dirpath){   //alterei isto para ser mais facil construir os paths
    DIR *dirStream;   
    struct dirent *dp;
    board_t **levels = NULL; //array de ponteiros com todos os níveis que vão ser lidos
    int numLevels = 0;

    dirStream = opendir(dirpath);  //abrir isto aqui dentro para ter mais controlo
    if (dirStream == NULL) {
        perror("Open dir falhou");
        //perror("opendir failed on '%s'", dirpath);  //copiei isto do prof
        //dar frreeeeee
    }
    for(;;){ //iterate thru all files in dir
        errno = 0;
        dp = readdir(dirStream);
        if(dp == NULL) {
            break;
        }

        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
            continue; 

        //decidir se se adiciona verificação para saltar os ficheiros com nome . e .. (pq isso o case já faz)
        //talvez criar sitio para guardar vários níveis (cada nível é guardado na struct board_t)
        //case para saber que função deve ler
        //caso seja .lvl
        // quando se ler o .lvl é que se vai procurar os .p e .m correspondentes;
        char *extension = strchr(dp->d_name, '.'); //file extension    
        if(strcmp(extension, ".lvl") == 0){
            //função de parse para lvl -> que por sua vez vai dar chamar o parse dos monstros e pac;
            //dar realloc à estrutura de níveis cada vez que se cria um novo
            
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dirpath, dp->d_name); //construir o path completo
            //não sei se podemos fazer isto ja que snprintf é do stdio.h
            board_t **tempLevels = realloc(levels, (numLevels + 1) * sizeof(board_t*));  //talvez mudar isto porque é lento (pela minha exp de iaed)
            if(!tempLevels){
                perror("realloc");
                //ver o que fazer aqui
            }
            levels = tempLevels; //realoc to original array;
            levels[numLevels] = parseLvl(path, dirpath);
            numLevels++; //novo nível
        }

    }
    if(errno != 0){
        perror("readdir");
        closedir(dirStream);
        //ver o que fazer aqui
    }

    closedir(dirStream); //é preciso verificar se close foi feito com sucesso?

    return levels;
}

int main(){
    char *directory = "levels";
    board_t** game = handle_files(directory);
    FILE *f = fopen("testOutput.txt", "w");
    
    for(int level = 0; game[level] ; level++){
        fprintf(f, "-----%s-----\n", game[level]->level_name);
        fprintf(f, "Width : %d ; Height : %d\n", game[level]->width, game[level]->height);
        //print board here
        int sizeMatrix = game[level]->width * game[level]->height;
        int count = 0; //quebrar linha no output das matrizes
        for(int index = 0; index < sizeMatrix; index++){
            
            if(count == (game[level]->width)){
                fprintf(f, "\n");
                count = 0;
            }
            fprintf(f, "%c", game[level]->board[index].content);
            count++;
        }
        fprintf(f, "\n\n-----PACMANS-----\n");
        fprintf(f, "\nNº pacmans : %d\n", game[level]->n_pacmans);
        //se n for dado ficheiro p também damos print?
        if(game[level]->n_pacmans == 1){
            fprintf(f, "POSX: %d ; POSY: %d\n", game[level]->pacmans[0].pos_x, game[level]->pacmans[0].pos_y);
            fprintf(f, "Alive: %d\n", game[level]->pacmans[0].alive);
            fprintf(f, "PASSO: %d\n", game[level]->pacmans[0].passo);
            //moves
            fprintf(f, "Moves: ");
            for(int move = 0; move < game[level]->pacmans[0].n_moves ;move++){
                fprintf(f, "%c", game[level]->pacmans[0].moves[move].command);
                //contemplar os turns também
                if(game[level]->pacmans[0].moves[move].command == 'T'){
                    fprintf(f, "%d", game[level]->pacmans[0].moves[move].turns_left);
                }
                fprintf(f, "; ");
            }
            fprintf(f, "\nCurrent_Move: %d\n", game[level]->pacmans[0].current_move);
            fprintf(f, "Moves: %d\n", game[level]->pacmans[0].n_moves);
        }
        fprintf(f, "\n-----GHOSTS-----\n");
        fprintf(f, "Nº ghosts : %d\n", game[level]->n_ghosts);
        //print ghosts info 
    
        for(int ghost = 0; ghost < game[level]->n_ghosts; ghost++){
            fprintf(f, "--GHOST %d--\n", ghost + 1);
            fprintf(f, "POSX: %d ; POSY: %d\n", game[level]->ghosts[ghost].pos_x, game[level]->ghosts[ghost].pos_y);
            fprintf(f, "Passo: %d\n", game[level]->ghosts[ghost].passo);
            //comandos dos fantasmas
            fprintf(f, "Moves: ");
            for(int move = 0; move < game[level]->ghosts[ghost].n_moves; move++){
                fprintf(f, "%c", game[level]->ghosts[ghost].moves[move].command);
                //contemplar os turns também
                if(game[level]->ghosts[ghost].moves[move].command == 'T'){
                    fprintf(f, "%d", game[level]->ghosts[ghost].moves[move].turns_left);
                }
                fprintf(f, "; ");
            
            }
            fprintf(f, "\nNº Moves: %d\n", game[level]->ghosts[ghost].n_moves);
            fprintf(f, "Curr Move: %d\n", game[level]->ghosts[ghost].current_move);
            //fprintf(f, "Wating: %d\n", game[level]->ghosts[ghost].waiting);
            //fprintf(f, "Charged: %d\n", game[level]->ghosts[ghost].charged);
        }
        fprintf(f, "\n--FILE INFO--\n");
        fprintf(f, "Filename : %s\n", game[level]->level_name);
        fprintf(f, "Pac File : %s\n", game[level]->pacman_file);
        //O GHOST FILE É MATRIX
        fprintf(f, "Ghost Files : ");
        for(int ghostFile = 0; ghostFile < game[level]->n_ghosts; ghostFile++){
            fprintf(f, "%s", game[level]->ghosts_files[ghostFile]);
        }
        fprintf(f, "\nTime : %d\n", game[level]->tempo);
        fprintf(f, "\n-----END LEVEL: %s-----\n\n\n", game[level]->level_name);
    

    }   
    fclose(f);
    //criar funão que dá free
    return 0; 

}