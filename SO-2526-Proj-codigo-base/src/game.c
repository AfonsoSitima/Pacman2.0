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
    //debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

int play_board(board_t * game_board) {
    pacman_t* pacman = &game_board->pacmans[0];
    command_t* play;
    command_t c;
    if (pacman->n_moves == 0) { // if is user input
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

    if (play->command == 'G'){
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
        // Next level
        return NEXT_LEVEL;
    }

    if(result == DEAD_PACMAN) {
        if (*game_board->hasBackup) return LOAD_BACKUP;  //só funciona se o hasBackUp for aqui idk y
        return QUIT_GAME;
    }
            
    pthread_rwlock_rdlock(&pacman->lock);
    int alive = pacman->alive;

    if (!alive) {
        pthread_rwlock_unlock(&pacman->lock);
        if (*game_board->hasBackup) return LOAD_BACKUP;
        return QUIT_GAME;
    }      //NAO SEI SE ISTO É PRECISO AQUI
    pthread_rwlock_unlock(&pacman->lock);
    return CONTINUE_PLAY;  
}




int createBackup(board_t* board) {
    pid_t pid;
    int status;

    terminal_cleanup();
    pid = fork();
    if(pid == -1){
        perror("Fork Error");
        //falta os frees
    }
    *board->hasBackup = 1;
    if(pid == 0){
        terminal_init();
        /*start_pacman_thread(board);
        start_ghost_threads(board);
        start_ncurses_thread(board);*/
    }
    else{
        wait(&status); 
        //debug("Backup process ended with status: %d\n", WEXITSTATUS(status));
        if (WIFEXITED(status)) {                 // saiu normalmente
            int code = WEXITSTATUS(status);      // código do exit
            if (code == 0) {
                return END_GAME;
            } 
            else if (code == 1) {
                terminal_init();
                /*start_pacman_thread(board);
                start_ghost_threads(board);
                start_ncurses_thread(board);*/
                *board->hasBackup = 0;
            }
        }
    }
    return CONTINUE_PLAY;
}


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

char** getBufferLines(char* buffer, ssize_t byte_count, int* line_count){
    char** lines = malloc(sizeof(char*) * (byte_count + 1)); //alocar espaço para as linhas (suposição de tamanho máximo)
    char* line = strtok(buffer, "\n"); //separar por linhas
    while (line != NULL) {
        lines[(*line_count)++] = line; //guardar cada linha no array
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
    snprintf(path, sizeof(path), "%s/%s", dirpath, filename); //construir o path completo
    int fd = open(path, O_RDONLY);

    buffer = readFile(fd, &byte_count);

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

    buffer = readFile(fd, &byte_count);

    lines = getBufferLines(buffer, byte_count, &line_count);

    pacman = (pacman_t*)malloc(sizeof(pacman_t));
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
    
    buffer = readFile(fd, &byte_count);
    
    lines = getBufferLines(buffer, byte_count, &line_count);
   
    lvl = (board_t*)malloc(sizeof(board_t));  //ver se há malloc error
    strcpy(lvl->level_name ,filename); //inicializar o nome do nível
    lvl->n_pacmans = 0; //não sei se pode haver mais que 1 pacman
    lvl->n_ghosts = 0;
    lvl->pacmans = NULL;
    lvl->ghosts = NULL;
    lvl->board = NULL;
    lvl->tid = NULL;
    //lvl->ncursesDraw = DRAW_MENU; //inicial
    lvl->active = 1;
    lvl->result = CONTINUE_PLAY;
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
            char ghost_files[256 * MAX_GHOSTS];
            char *ghost_file;
            char *saveptr;                // estado da strtok_r

            strcpy(ghost_files, lines[i] + 4);   // ignorar o "MON "

            ghost_file = strtok_r(ghost_files, " ", &saveptr);

            while (ghost_file != NULL) {
                lvl->ghosts = realloc(lvl->ghosts, (lvl->n_ghosts + 1) * sizeof(ghost_t));

                if (!lvl->ghosts) {
                    // trata erro de memória se quiseres
                    break;
                }


                lvl->ghosts[lvl->n_ghosts] = *parseMonster(ghost_file, dirpath);

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
                lvl->board[matrix_index].content = (lines[i][j] == '@') ? 'o' : lines[i][j]; //tratar portal como casa normal
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
    lvl->tid = malloc(lvl->n_ghosts * sizeof(pthread_t)); //alocar espaço para os tids das threads Ghosts
    pthread_mutex_init(&lvl->ncurses_lock, NULL); //lock do ncurses
    pthread_mutex_init(&lvl->state_lock, NULL);

    free(lines);
    free(buffer);
    close(fd); //close level file
    return lvl;
}

//inicializa pacman quando não há ficheiro .p
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
    levels = realloc(levels, (numLevels + 1) * sizeof(board_t*));
    levels[numLevels] = NULL;

    closedir(dirStream); //é preciso verificar se close foi feito com sucesso?

    return levels;
}

void start_ghost_threads(board_t* board) {    //trata de todas a threads dos ghosts
    for (int i = 0; i < board->n_ghosts; i++) {
        thread_ghost_t* thread_data = malloc(sizeof(thread_ghost_t));
        thread_data->index = i;
        thread_data->board = board;
        thread_data->moves = board->ghosts[i].moves;
        pthread_create(&board->tid[i], NULL, (void*) ghost_thread, thread_data);
    }
}
void stop_ghost_threads(board_t* board) {
    for(int i = 0; i < board->n_ghosts; i++) {
        pthread_join(board->tid[i], NULL);
    }

}

/*void start_pacman_thread(board_t* board) {
    
    thread_pacman_t* thread_data = malloc(sizeof(thread_pacman_t));
    thread_data->index = 0; 
    thread_data->board = board;
    thread_data->moves = board->pacmans[0].moves;
    pthread_create(&board->pacTid, NULL, (void*) pacman_thread, thread_data);
    
}
void stop_pacman_thread(board_t* board) {
    pthread_join(board->pacTid, NULL);
    
}*/



void* ncurses_thread(void* arg) {
    thread_ncurses* data = (thread_ncurses *)arg;
    board_t* board = data->board;

    while (board->active) {
        sleep_ms(board->tempo);
        screen_refresh(board, DRAW_MENU);
    }
    return NULL;
}

//É obrigatório ter esta função
//ver onde por locks
//thread a desbloquear a casa onde estava e a bloquear a casa para onde vai
void* ghost_thread(void* thread_data) {
    thread_ghost_t* data = (thread_ghost_t*)thread_data;
    board_t* board = data->board;
    int ghost_index = data->index; 
    ghost_t* ghost = &board->ghosts[ghost_index];
    while(board->active) { //Arranjar forma de ver se o pacman entrou no portal
        sleep_ms(ghost->passo);
        move_ghost(board, ghost_index, &ghost->moves[ghost->current_move % ghost->n_moves]);
    }
    free(data);
    return NULL;
}


void start_ncurses_thread(board_t* board) {
    thread_ncurses* thread_data = malloc(sizeof(thread_ncurses));
    thread_data->board = board;
    thread_data->running = 1;
    pthread_create(&board->ncursesTid, NULL, (void*) ncurses_thread, thread_data);
}

void* pacman_thread(void* arg) {
    thread_pacman_t* data = arg;
    board_t* board = data->board;

    while (1) {
        sleep_ms(board->pacmans[0].passo);        //pthread_mutex_lock(&board->state_lock);
        int play = play_board(board); //talvez mudar isto para true se quiser backup automático
        //pthread_mutex_unlock(&board->state_lock);
        if (play == CONTINUE_PLAY)
            continue;
        
        board->result = play;
        return NULL;
    }
}


void start_pacman_thread(board_t* board) {

    thread_pacman_t* thread_data = malloc(sizeof(thread_pacman_t));
    thread_data->index = 0; 
    thread_data->board = board;
    thread_data->moves = board->pacmans[0].moves;
    thread_data->hasBackup = 0;
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

    terminal_init();
    int result;
    int indexLevel = 0;
    board_t *game_board = NULL;
    int accumulated_points = 0;
    bool end_game = false;
    int* hasBackUp = malloc(sizeof(int));
    *hasBackUp = 0;

    
    board_t** levels = handle_files(argv[1]);
    
    
    while (!end_game) {

        if (levels[indexLevel] == NULL) {
            end_game = true;
            break; 
        }
        game_board = levels[indexLevel];
        game_board->hasBackup = hasBackUp;

        
        load_level(game_board, accumulated_points, hasBackUp); //NO NOVO MÉTODO TEM DE ACUMULAR PONTOS

        //while(true)
        start_ncurses_thread(game_board);
        start_pacman_thread(game_board);
        start_ghost_threads(game_board);

        pthread_join(game_board->pacTid, NULL); //esperar pela thread do pacman

        result = game_board->result;

        switch (result) {
            case NEXT_LEVEL:
                game_board->active = 0;
                pthread_join(game_board->ncursesTid, NULL);
                stop_ghost_threads(game_board);
                screen_refresh(game_board, DRAW_WIN);
                sleep_ms(game_board->tempo); 
                indexLevel++;
                //unload_level(game_board);
                break;
            
            case LOAD_BACKUP:
                game_board->active = 0;
                pthread_join(game_board->ncursesTid, NULL);
                stop_ghost_threads(game_board);
                terminal_cleanup();
                exit(1);  //o filho morre
                break;
            case CREATE_BACKUP:
                game_board->active = 0;
                pthread_join(game_board->ncursesTid, NULL);
                stop_ghost_threads(game_board);
                end_game = (createBackup(game_board) == 1) ? QUIT_GAME : CONTINUE_PLAY;
                break;
            //JÀ TA FEiTO
            case QUIT_GAME:
                game_board->active = 0; 
                pthread_join(game_board->ncursesTid, NULL);
                stop_ghost_threads(game_board);
                unload_level(game_board);
                screen_refresh(game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board->tempo);
                end_game = true;
                break;
            default:
                break;
        }

        //print_board(game_board);
    }
    unload_allLevels(levels, indexLevel);
    free(hasBackUp);
    terminal_cleanup();
    close_debug_file();

    return 0;
}