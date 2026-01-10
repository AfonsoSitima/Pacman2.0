#include "api.h"
#include "protocol.h"
#include "display.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>


Board board;
bool stop_execution = false;
int tempo;
pthread_rwlock_t temp_lock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t ready_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t execution = PTHREAD_RWLOCK_INITIALIZER;

volatile sig_atomic_t SIGINT_received = 0; 
int svOn = 1;

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t ncurses = PTHREAD_MUTEX_INITIALIZER;
bool board_ready = false;

static void *receiver_thread(void *arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);   
    pthread_sigmask(SIG_BLOCK, &set, NULL); 

    (void)arg;

    while (true) {
        if(board.data){
            free(board.data);
        }
        board = receive_board_update();
        
        if (!board.data || board.game_over == 1 || board.victory == 1 || SIGINT_received == 1) {
            debug("Game over received, stopping receiver thread...\n");
            pthread_rwlock_wrlock(&execution);
            stop_execution = true;
            pthread_rwlock_unlock(&execution);
            break;
        }


        pthread_rwlock_wrlock(&temp_lock);
        tempo = board.tempo;
        if (!board_ready && tempo > 0) {

            pthread_mutex_lock(&ready_lock);
            board_ready = true;
            pthread_mutex_unlock(&ready_lock);
            pthread_cond_broadcast(&cond);  // acorda o main
        }
        pthread_rwlock_unlock(&temp_lock);
        


        pthread_mutex_lock(&ncurses);
        draw_board_client(board);
        refresh_screen();
        pthread_mutex_unlock(&ncurses);
    }
    debug("Returning receiver thread...\n");
    pthread_mutex_lock(&ncurses);
    draw_board_client(board);
    refresh_screen();
    pthread_mutex_unlock(&ncurses);
    free(board.data);
    return NULL;
}

void handle_SIGINT(){

    SIGINT_received = 1;
}

int main(int argc, char *argv[]) {
    struct sigaction sa2;
    sa2.sa_handler = handle_SIGINT;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = 0; 
    sigaction(SIGINT, &sa2, NULL); 
    signal(SIGPIPE, SIG_IGN);

    if (argc != 3 && argc != 4) {
        fprintf(stderr,
            "Usage: %s <client_id> <register_pipe> [commands_file]\n",
            argv[0]);
        return 1;
    }

    const char *client_id = argv[1];
    const char *commands_file = (argc == 4) ? argv[3] : NULL;

    FILE *cmd_fp = NULL;
    if (commands_file) {
        cmd_fp = fopen(commands_file, "r");
        if (!cmd_fp) {
            perror("Failed to open commands file");
            return 1;
        }
    }
    
    char sv_pipe[MAX_PIPE_PATH_LENGTH];
    char req_pipe_path[MAX_PIPE_PATH_LENGTH];
    char notif_pipe_path[MAX_PIPE_PATH_LENGTH];

    snprintf(req_pipe_path, MAX_PIPE_PATH_LENGTH,
             "/tmp/%s_request", client_id);

    snprintf(notif_pipe_path, MAX_PIPE_PATH_LENGTH,
             "/tmp/%s_notification", client_id);

    snprintf(sv_pipe, MAX_PIPE_PATH_LENGTH, 
                "/tmp/%s", argv[2]);

    open_debug_file("client-debug.log");
    debug("%s\n", req_pipe_path);
    debug("%s\n", notif_pipe_path);
    debug("%s\n", sv_pipe);
    if (pacman_connect(req_pipe_path, notif_pipe_path, sv_pipe) != 0) {
        perror("Failed to connect to server");
        return 1;
    }

    terminal_init();
    //set_timeout(500);

    pthread_t receiver_thread_id;
    pthread_create(&receiver_thread_id, NULL, receiver_thread, NULL);

    //pthread_mutex_lock(&ncurses);
    //draw_board_client(board);
    //refresh_screen();
    //pthread_mutex_unlock(&ncurses);

    pthread_mutex_lock(&ready_lock);
    while (!board_ready) pthread_cond_wait(&cond, &ready_lock);
    pthread_mutex_unlock(&ready_lock);

    char command;
    int ch;

    while (1) {

        pthread_rwlock_rdlock(&execution);
        if (stop_execution){
            pthread_rwlock_unlock(&execution);
            break;
        }
        pthread_rwlock_unlock(&execution);

        if (cmd_fp) {
            // Input from file
            ch = fgetc(cmd_fp);

            if (ch == EOF) {
                // Restart at the start of the file
                rewind(cmd_fp);
                continue;
            }

            command = (char)ch;

            if (command == '\n' || command == '\r' || command == '\0')
                continue;

            command = toupper(command);
            
            // Wait for tempo, to not overflow pipe with requests
            
        } else {
            // Interactive input
            pthread_mutex_lock(&ncurses);
            command = get_input();
            pthread_mutex_unlock(&ncurses);
            if(SIGINT_received){
                break;
            }
            command = toupper(command);
        }
        pthread_rwlock_rdlock(&temp_lock);
        int wait_for = tempo;
        debug("wait_for : %d\n", tempo);
        pthread_rwlock_unlock(&temp_lock);

        sleep_ms(wait_for);
        debug("wait_for2 : %d\n", wait_for);

        debug("Command: %c\n", command);
        if(SIGINT_received){
            pacman_play('Q');
        }
        else{
            if(pacman_play(command) < 0){
                svOn = 0;
                break;
            }
        }
    }

    debug("Waiting for receiver thread to finish...\n");

    pthread_join(receiver_thread_id, NULL);

    debug("Disconnecting from server...\n");

    if(svOn){
        pacman_disconnect();
    }
    
    debug("DESCONECTOU\n");
    sleep_ms(1000);
    debug("Exiting main...\n");
    close_debug_file();
    if (cmd_fp)
        fclose(cmd_fp);

    pthread_rwlock_destroy(&temp_lock);
    pthread_mutex_destroy(&ready_lock);
    pthread_rwlock_destroy(&execution);
    pthread_cond_destroy(&cond);

    terminal_cleanup();

    return 0;
}