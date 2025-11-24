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
#define PATH_MAX 512 // o chat sugeriu este valor



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
        //ver o que fazer aqui
    }
    
    return dir;
}   


//USAR MAIS ESTA FUNÇÃO PARA LER FICHEIROS
char* readFile(int fd, ssize_t* byte_count) { //talvez adicionar o numero de bytes lidos
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




//VER SE HÁ MALLOC ERRORS AQUI
board_t* parseLvl(char* filename){ //pela forma que estamos a fazer no handle_files
    //novo nível
    char* buffer = NULL;
    char** lines = NULL;
    char* line = NULL;
    ssize_t byte_count = 0;
    int line_count = 0;
    board_t *lvl = (board_t*)malloc(sizeof(board_t));  //ver se há malloc error
    size_t size_board;

    int fd = open(filename, O_RDONLY);

    buffer = readFile(fd, &byte_count);

    lines = malloc(sizeof(char*) * (byte_count + 1)); //alocar espaço para as linhas (suposição de tamanho máximo)
    line = strtok(buffer, "\n"); //separar por linhas
    while (line != NULL) {
        lines[line_count++] = line; //guardar cada linha no array
        line = strtok(NULL, "\n");  
    }

    strcpy(lvl->level_name ,filename); //inicializar o nome do nível
    lvl->n_pacmans = 0; //não sei se pode haver mais que 1 pacman
    lvl->n_ghosts = 0;
    lvl->pacmans = NULL;
    lvl->ghosts = NULL;
    //Ya bro não sei se esta é a melhor solução
    //aqui tamos a ler linha por linha
    for (int i = 0; i < line_count; i++) {
        if (strncmp(lines[i], "DIM", 3) == 0) 
            sscanf(lines[i], "DIM %d %d", &lvl->width, &lvl->height);
        
        else if (strncmp(lines[i], "TEMPO", 5) == 0) 
            sscanf(lines[i], "TEMPO %d", &lvl->tempo);
        
        else if (strncmp(lines[i], "PAC", 3) == 0) { 
            sscanf(lines[i], "PAC %s", lvl->pacman_file);
            lvl->pacmans = realloc(lvl->pacmans, (lvl->n_pacmans + 1) * sizeof(pacman_t));
            lvl->pacmans[0] = *parsePacman(lvl->pacman_file); //ver se há malloc error aqui
            lvl->n_pacmans++; //não tenho a certeza se é suposto haver mais que 1 pacman
        }
        
        else if (strncmp(lines[i], "MON", 3) == 0) {
            char ghost_files[256 * MAX_GHOSTS]; //array para guardar os nomes dos ficheiros (como está na struct board_t)
            char *ghost_file;
            
            strcpy(ghost_files, lines[i] + 4); //ignorar o "MON "
            ghost_file = strtok(ghost_files, " ");

            while (ghost_file != NULL) {
                lvl->ghosts = realloc(lvl->ghosts, (lvl->n_ghosts + 1) * sizeof(ghost_t));
                lvl->ghosts[lvl->n_ghosts] = *parseMonster(ghost_file); //ver isto
                strcpy(lvl->ghosts_files[lvl->n_ghosts++], ghost_file);
                ghost_file = strtok(NULL, " ");
            }
        }   
        else {
            

        }

    }


    //criar cena para procurar os ficheiros .p e .m correspondentes ao nível


    close(fd); //close level file
    return lvl;
}

ghost_t* parseMonster(char* fileName){
    ghost_t *monster = (ghost_t*)malloc(sizeof(ghost_t));

    int fd = open(fileName, O_RDONLY);
    if(fd == -1){
        perror("openfile MonsterFile");
        //ver o que é preciso dar close aqui que vinha para trás;
    }

    //lógica
    
    close(fd); //close mosnter file
    return monster;
}

pacman_t* parsePacman(char* fileName){
    pacman_t *pacman = (pacman_t*)malloc(sizeof(pacman_t));

    int fd = open(fileName, O_RDONLY);
    if(fd == -1){
        perror("openfile PacmanFile");
        //ver o que pode ser preciso fechar para trás;
    }

    //lógica

    close(fd); //close pacman file
    return pacman;
}

//retornar lista de níveis
board_t** handle_files(char* dirpath){   //alterei isto para ser mais facil construir os paths
    DIR *dirStream;   
    struct dirent *dp;
    board_t **levels = NULL; //array de ponteiros com todos os níveis que vão ser lidos
    int numLevels = 0;

    dirStream = opendir(dirpath);  //abrir isto aqui dentro para ter mais controlo
    if (dirStream == NULL) {
        errMsg("opendir failed on '%s'", dirpath);  //copiei isto do prof
        return;
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
            if(tempLevels){
                perror("realloc");
                //ver o que fazer aqui
            }
            levels = tempLevels; //realoc to original array;
            levels[numLevels] = parseLvl(path);
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
    board_t game_board; //meter função que devolve lista de níveis
    //contador dentro do loop que vai passando de nível quando é suposto

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
