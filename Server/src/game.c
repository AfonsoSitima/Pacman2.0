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
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>


#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4
#define END_GAME 1
#define PATH_MAX 512
#define PACMAN 1
#define GHOST 2

volatile sig_atomic_t SIGUSR1_received = 0; //variável global para o estado do signal 


//pthread_t serverId;

//maybe fazer esta parte noutro ficheiro
void innit_p2c(p2c_t* p2c, int max_games) {
    p2c->client_request = malloc(max_games * sizeof(client_request_t));
    p2c->head = 0;
    p2c->tail = 0;
    p2c->max_size = max_games;
}

void destroy_p2c(p2c_t* p2c) {
    free(p2c->client_request);
}

void enqueue_p2c(p2c_t* p2c, client_request_t* request) {
    p2c->client_request[p2c->tail] = *request;
    p2c->tail = (p2c->tail + 1) % p2c->max_size;
}

client_request_t pop_p2c(p2c_t* p2c) {
    client_request_t request = p2c->client_request[p2c->head];
    p2c->head = (p2c->head + 1) % p2c->max_size;
    return request;
}

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
    command_t* play = malloc(sizeof(command_t));

    pthread_rwlock_rdlock(&pacman->lock);
    int alive = pacman->alive;
    pthread_rwlock_unlock(&pacman->lock);

    
    if (!alive) {
        free(play);
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
    debug("Received command: %c\n", play->command);
    play->turns = 1;
    play->turns_left = 0;


    debug("KEY %c\n", play->command);

    if (play->command == 'G'){
        if (pacman->n_moves != 0) pacman->current_move++;
        if(!*game_board->hasBackup){
            free(play);
            return CREATE_BACKUP;
        }
        
    }

    if (play->command == 'Q') {
        free(play);
        return QUIT_GAME;
    }

    int result = move_pacman(game_board, 0, play);
    free(play);
    debug("%d\n", result);
    if (result == REACHED_PORTAL) {
        if (game_board->can_win) {
            pthread_rwlock_wrlock(&pacman->lock);
            pacman->won = 1;
            pthread_rwlock_unlock(&pacman->lock);
        }
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
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);    
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




void* pacman_thread(void* arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
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
        board_pos_t cell = board->board[i];
        if(cell.content == 'o'){
            if(cell.has_portal){
                boardChar[k] = '@';
            }
            else if(cell.has_dot){
                boardChar[k] = '.';
            }
            else {
                boardChar[k] = ' ';
            }
        }
        else{
            boardChar[k] = cell.content;
        }
        
    }
    return boardChar;
}

void* server_thread(void* arg){
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    thread_server_t* data = arg;
    board_t* board = data->board;
    session_t* game_s = data->game_s;
    pacman_t* pacman = &board->pacmans[0];
    int game_over;
    int accumulated_points;
    int victory;
    //atualiza periodicamente
    while(1){
        sleep_ms(board->tempo);
        char buf[(sizeof(int) * 6) + 1];
        memset(buf, '\0', sizeof(buf));
        buf[0] = OP_CODE_BOARD;
        pthread_rwlock_rdlock(&pacman->lock);
        game_over = !pacman->alive;
        accumulated_points = pacman->points;
        game_s->points = pacman->points; //points da seesão 
        victory = pacman->won;
        pthread_rwlock_unlock(&pacman->lock);

        memcpy(buf + 1, &board->width, sizeof(int));
        memcpy(buf + 1 + sizeof(int), &board->height, sizeof(int));
        memcpy(buf + 1 + sizeof(int) * 2, &board->tempo, sizeof(int));
        memcpy(buf + 1 + sizeof(int) * 3, &victory,  sizeof(int));
        memcpy(buf + 1 + sizeof(int) * 4, &game_over, sizeof(int));
        memcpy(buf + 1 + sizeof(int) * 5, &accumulated_points, sizeof(int));

        write_all(game_s->notif_pipe, buf, sizeof(buf));
        //LOCK TABULEIRO
        //read lock tabuleiro
        //Função para meter tabuleiro em char
        char* boardChar = boardToChar(board);

        //debug("%s\n", buf);
        
        write_all(game_s->notif_pipe, boardChar, board->height * board->width);
        //pthread_rwlock_unlock(&board->board_lock);
        //debug("%s\n", boardChar);
        free(boardChar);

        pthread_rwlock_rdlock(&board->board_lock);
        int active = board->active;
        pthread_rwlock_unlock(&board->board_lock);
        if (!active) {
            break;
        }
    }
    free(data);
    return NULL;
}

void start_server_thread(board_t* board, session_t* game_s, pthread_t* serverId) {
    thread_server_t* thread_data = malloc(sizeof(thread_server_t));
    thread_data->board = board;
    thread_data->game_s = game_s;
    pthread_create(serverId, NULL, server_thread, thread_data);
}

void* game_thread(void* arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    thread_game_t* data = (thread_game_t*)arg;
    //session_t* game_s = data->game_s;
    p2c_t* producerConsumer = data->producerConsumer;
    board_t** levels = data->levels;
    sem_t* sem_games = data->sem_games;
    sem_t* sem_slots = data->sem_slots;
    session_t** activeClients = data->activeClients;
    pthread_mutex_t* lock = data->clientsArrayLock;
    int id = data->id;

    char req_file_path[40];
    char notif_file_path[40];
    while(1){
        sem_wait(sem_games); //espera por um novo jogo para iniciar

        pthread_mutex_lock(&producerConsumer->lock);     //ler dados do produtor consumidor
        client_request_t request = pop_p2c(producerConsumer);
        strncpy(req_file_path, request.req_pipe_path, 40);
        strncpy(notif_file_path, request.notif_pipe_path, 40);
        pthread_mutex_unlock(&producerConsumer->lock);

        sem_post(sem_slots); //há uma vaga para iniciar sessão

        session_t *game_s = malloc(sizeof(session_t));
        strncpy(game_s->req_pipe_path, req_file_path, 40);
        strncpy(game_s->notif_pipe_path, notif_file_path, 40);
        innit_session(game_s, id); //inicia sessão sem guardar o número de sessões

        //adicionar novo client
        pthread_mutex_lock(lock);
        activeClients[id] = game_s;
        pthread_mutex_unlock(lock);

        int indexLevel = 0;
        int tempPoints = 0; //acumulated points between levels
        board_t *game_board = NULL;
        bool end_game = false;
        int* hasBackUp = malloc(sizeof(int));
        *hasBackUp = 0;
        pthread_t serverId;
        //pthread_create(&serverId, NULL);

        while (!end_game) {

            game_board = level_copy(levels[indexLevel]);
            game_board->hasBackup = hasBackUp;

            if (levels[indexLevel + 1] == NULL) {
                end_game = true; //pode dar victory aqui
                game_board->can_win = 1;
            }
            
            load_level(game_board, hasBackUp, tempPoints); 

            start_server_thread(game_board, game_s, &serverId);
            start_pacman_thread(game_board, game_s);
            //start_server_thread(game_board, game_s);
            start_ghost_threads(game_board);

            pthread_join(game_board->pacTid, NULL); //waits for pacman thread to end

            int result = game_board->result; //get result of the game

            pthread_rwlock_wrlock(&game_board->board_lock);
            game_board->active = 0;   //stop other threads
            pthread_rwlock_unlock(&game_board->board_lock);
            
            //pthread_join(game_board->ncursesTid, NULL);
            pthread_join(serverId, NULL);
            stop_ghost_threads(game_board);

            switch (result) {
                case NEXT_LEVEL:
                    tempPoints = game_board->pacmans[0].points;
                    indexLevel++;
                    unload_level(game_board); //free level copy
                    break;
                case QUIT_GAME:
                    end_game = true;
                    unload_level(game_board); //free level copy
                    break;
                default:
                    break;
            }
        }
        free(hasBackUp);
        disconnect_session(game_s);
        pthread_mutex_lock(lock);
        activeClients[id] = NULL; // remove client dos ativos
        pthread_mutex_unlock(lock);
    }
    pthread_mutex_destroy(lock);
    free(data);
}


/*void start_game_threads(char * server_pipe_path, int max_games, board_t** levels) {
    int servfd;
    int nSessions = 0;
    int max = max_games;
    if (unlink(server_pipe_path) != 0 && errno != ENOENT) return NULL;
    if (mkfifo(server_pipe_path, 0640) != 0) return NULL; //cria o pipe do servidor

    servfd = open(server_pipe_path, O_RDONLY);  //abre o pipe do servidor
    if (servfd < 0) return NULL;

    sem_t* sem = malloc(sizeof(sem_t));
    sem_init(sem, 0, max_games); 

    pthread_t* games = malloc(max_games * sizeof(pthread_t));

    while(1) { //não sei quando acabar o loop
        //sem_wait(&sem);
        char buffer[1 + 40 + 40]; //1 para o id, 40 para o req pipe path, 40 para o notif pipe path 
        read_all(servfd, buffer, sizeof(buffer)); //lê o que o cliente enviou
        if(buffer[0] != OP_CODE_CONNECT) return; //error

        sem_wait(&sem); //espera por uma vaga para iniciar sessão

        session_t* session = malloc(sizeof(session_t));
        strncpy(session->req_pipe_path, buffer + 1, 40);
        strncpy(session->notif_pipe_path, buffer + 1 + 40, 40);

        innit_session(session, &nSessions);

        //comecar as threads
        pthread_t gameId;
        games[nSessions - 1] = gameId;
        pthread_innit(&gameId, NULL);

        thread_game_t* thread_data = malloc(sizeof(thread_game_t));
        thread_data->game_s = session;
        thread_data->levels = levels; //fazer deep copy se necessário
        thread_data->sem = sem;

        pthread_create(&gameId, NULL, game_thread, (void*)thread_data);
        //sem_post(&sem);
        if (nSessions == max - 1) {      //guarda espaço para mais jogos
            games = realloc(games, (max + max_games) * sizeof(pthread_t));
            max += max_games;
        }
    }
    for(int i = 0; i < nSessions; i++) {
        pthread_join(games[i], NULL);
    }
    close(servfd);
    sem_destroy(sem);
    unload_allLevels(levels);
    return;
}*/


void start_game_threads(/*char * server_pipe_path,*/ int max_games, pthread_t* gameTids, board_t** levels, p2c_t* producerConsumer, sem_t* sem_games, sem_t* sem_slots, session_t** activeClients, pthread_mutex_t* clientsArrayLock) {
    //pthread_t* games = malloc(max_games * sizeof(pthread_t));
    //int count_levels = get_levels_count(levels);
    for (int i = 0; i < max_games; i++) {

        /*pthread_t gameId;
        games[i] = gameId;
        pthread_innit(&gameId, NULL);
        session_t* session = malloc(sizeof(session_t));
        strncpy(session->req_pipe_path, producerConsumer + 1, 40);
        strncpy(session->notif_pipe_path, producerConsumer + 1 + 40, 40);
        innit_session(session, i); //inicia sessão sem guardar o número de sessões
        */
        thread_game_t* thread_data = malloc(sizeof(thread_game_t));
        //thread_data->game_s = session;
        thread_data->producerConsumer = producerConsumer;
        //thread_data->levels = levels; //fazer deep copy se necessário
        thread_data->levels = levels;
        thread_data->sem_games = sem_games;
        thread_data->sem_slots = sem_slots;
        thread_data->activeClients = activeClients;
        thread_data->clientsArrayLock = clientsArrayLock;
        thread_data->id = i;


        pthread_create(&gameTids[i], NULL, game_thread, (void*)thread_data);
    }
}

int maxPoints(const void* a, const void* b) {
    score* entryA = (score*)a;
    score* entryB = (score*)b;

    return (entryB->points - entryA->points);
}



void leaderBoard(session_t** activeClients, int maxGames, pthread_mutex_t* lock){ // clients connected array
    //generate leaderboard
    debug("GERANDO LEADERBOARD----");
    int count = 0;
    score temp[maxGames];
    pthread_mutex_lock(lock); //as threads do game podem estar a aceder
    for(int id = 0; id < maxGames; id++){
        //computar
        if(!activeClients[id]) continue;
        temp[count].id = activeClients[id]->id;
        temp[count].points = activeClients[id]->points;
        count++;
    }
    pthread_mutex_unlock(lock);
    qsort(temp, count, sizeof(score), maxPoints);

    int fd = open("leaderboard.txt", O_WRONLY | O_TRUNC | O_CREAT, 0644); 
    if(fd == -1){
        debug("[ERR]: LEADERBOARD FILE OPEN FAILED");
        //TRATAR ESTE ERRO MAIS TARDE
    }

    for(int id = 0; id < count; id ++){
        char buf[25];
        int len = snprintf(buf, 25, "%d - %d\n", temp[id].id, temp[id].points);
        write(fd, buf, len);
    }

    if(close(fd) == -1){
        debug("[ERR]: LEADERBOARD FILE CLOSE SYSCALL FAILED");
    }


}

void* host_thread(void* arg) {
    thread_host_t* data = (thread_host_t*)arg;
    p2c_t* producerConsumer = data->producerConsumer;
    sem_t* sem_games = data->sem_games;
    sem_t* sem_slots = data->sem_slots;
    session_t** activeClients = data->activeClients;    
    char* server_pipe_path = data->server_pipe_path;
    pthread_mutex_t* lock = data->clientsArrayLock;
    int maxGames = data->maxGames;
    char buf[1 + 40 + 40]; //1 para o id, 40 para o req pipe path, 40 para o notif pipe path

    int servfd;

    if (unlink(server_pipe_path) != 0 && errno != ENOENT) return NULL;
    if (mkfifo(server_pipe_path, 0640) != 0) return NULL; //cria o pipe do servidor
    servfd = open(server_pipe_path, O_RDWR);  //MUDEI PARA RDWR PARA NÃO BLOQUEAR
    if (servfd < 0) return NULL;

    while(1) {  
        if(SIGUSR1_received){
            write(1, "A\n", 2);
            //se o sinal foi recebido gera o relatório
            leaderBoard(activeClients, maxGames, lock);
            SIGUSR1_received = 0; //reseta
        }

        if(read_all(servfd, buf, sizeof(buf)) < 0 ) continue;
       
        if(buf[0] != OP_CODE_CONNECT) continue; //error

        client_request_t request;
        strncpy(request.req_pipe_path, buf + 1, 40);
        strncpy(request.notif_pipe_path, buf + 1 + 40, 40);

        sem_wait(sem_slots); //espera por uma vaga para iniciar sessão
        
        pthread_mutex_lock(&producerConsumer->lock);     //escrever dados no produtor consumidor
        enqueue_p2c(producerConsumer, &request);
        pthread_mutex_unlock(&producerConsumer->lock);


        sem_post(sem_games); //sinaliza que há um novo jogo para iniciar



        
    }
    free(data);
}


//-------------------RESOLVER-----------------------------------------------
//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
pthread_t hostId;
//AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA

void start_host(p2c_t* p2c, sem_t* sem_games, sem_t* sem_slots, char* server_pipe_path, session_t** activeClients, int maxGames, pthread_mutex_t* lock) {
    thread_host_t* data = malloc(sizeof(thread_host_t));
    data->producerConsumer = p2c;
    data->sem_games = sem_games;
    data->sem_slots = sem_slots;
    data->server_pipe_path = server_pipe_path;
    data->activeClients = activeClients;
    data->maxGames = maxGames;
    data->clientsArrayLock = lock;
    pthread_create(&hostId, NULL, host_thread, (void*)data);
}

void handle_SIGUSR1(){
    write(1, "A\n", 2);
    SIGUSR1_received = 1;
}
int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr,
            "Usage: %s <levels_dir> <max_games> <register_pipe>\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGPIPE, SIG_IGN); //caso o cliente feche a ligação, não dá erro quando tentamos escrever para lá 
    struct sigaction sa;
    sa.sa_handler = handle_SIGUSR1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Ajuda a que o read() não morra com erro
    sigaction(SIGUSR1, &sa, NULL); 
    //signal(SIGUSR1, handle_SIGUSR1);
    // Random seed for any random movements
    srand((unsigned int)time(NULL));
    open_debug_file("debug.log");


    sem_t* sem_games = malloc(sizeof(sem_t)); //ver se o jogo pode começar
    sem_t* sem_slots = malloc(sizeof(sem_t)); //ver se há lugares disponiveis para iniciar sessão
    board_t ** levels = handle_files(argv[1]);
    p2c_t* p2c = malloc(sizeof(p2c_t));
        

    int maxGames = atoi(argv[2]);
    //lista de sessões abertas
    session_t** activeClients = calloc(maxGames, sizeof(session_t*));

    pthread_mutex_init(&p2c->lock, NULL);
    innit_p2c(p2c, maxGames);
    sem_init(sem_games, 0, 0);
    sem_init(sem_slots, 0, maxGames);
    
    pthread_mutex_t clients_lock;
    pthread_mutex_init(&clients_lock, NULL);
    start_host(p2c, sem_games, sem_slots, argv[3], activeClients, maxGames, &clients_lock);
    pthread_t* gameTids = malloc(sizeof(pthread_t) * maxGames);
    
    start_game_threads(/*argv[3],*/ maxGames, gameTids, levels, p2c, sem_games, sem_slots, activeClients, &clients_lock);
    

    pthread_join(hostId, NULL);
    for(int i = 0 ; i < maxGames; i++){
        pthread_join(gameTids[i], NULL);
    }
    free(gameTids);//organizar melhor isto

    sem_destroy(sem_games);
    free(sem_games);
    sem_destroy(sem_slots);
    free(sem_slots);
    
    pthread_mutex_destroy(&p2c->lock);
    destroy_p2c(p2c); //liberta ponteiro "interior"
    free(p2c);
    
    
    //a porra do server fica a correr para sempre
    return 0;
}



/*int main(int argc, char** argv) {
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
    //session_t* game_s = innit_session(argv[3], &numS, 1);
    

    board_t** levels = handle_files(argv[1]);

    //terminal_init();
    int result;
    int indexLevel = 0;
    int tempPoints = 0; //acumulated points between levels
    board_t *game_board = NULL;
    bool end_game = false;
    int* hasBackUp = malloc(sizeof(int));
    *hasBackUp = 0;



    while (!end_game) {

        game_board = levels[indexLevel];
        game_board->hasBackup = hasBackUp;

        if (levels[indexLevel + 1] == NULL) {
            end_game = true; //pode dar victory aqui
            game_board->can_win = 1;
        }
        
        load_level(game_board, hasBackUp, tempPoints); 

        //start_ncurses_thread(game_board);
        start_server_thread(game_board, game_s);
        start_pacman_thread(game_board, game_s);
        //start_server_thread(game_board, game_s);
        start_ghost_threads(game_board);

        pthread_join(game_board->pacTid, NULL); //waits for pacman thread to end

        result = game_board->result; //get result of the game

        pthread_rwlock_wrlock(&game_board->board_lock);
        game_board->active = 0;   //stop other threads
        pthread_rwlock_unlock(&game_board->board_lock);
        
        //pthread_join(game_board->ncursesTid, NULL);
        pthread_join(serverId, NULL);
        stop_ghost_threads(game_board);

        switch (result) {
            case NEXT_LEVEL:
                //screen_refresh(game_board, DRAW_WIN);
                //sleep_ms(game_board->tempo); 
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
                //screen_refresh(game_board, DRAW_GAME_OVER); 
                //sleep_ms(game_board->tempo);
                end_game = true;
                break;
            default:
                break;
        }
        print_board(game_board);
    }
    unload_allLevels(levels);
    free(hasBackUp);
    disconnect_session(game_s);
    //terminal_cleanup();
    close_debug_file();

    return 0;
}
*/