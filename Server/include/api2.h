#ifndef API2_H
#define API2_H

#include <unistd.h>

#define PIPE_NAME_MAX_LENGTH 256
#define MAX_PIPE_PATH_LENGTH 40
#define MAX_SESSIONS 40

typedef struct {
    int id;
    int req_pipe;
    int notif_pipe;
    char req_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
    char notif_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
    int points;
} session_t;

int read_all(int fd, char *buf, size_t len);

int write_all(int fd, char *buf, size_t len);

void innit_session(session_t * session, int sessionId);

char get_pacman_command(session_t* session);

void disconnect_session(session_t* session);


#endif