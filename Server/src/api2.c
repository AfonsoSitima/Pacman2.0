#include "api2.h"
#include "protocol2.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>


int write_all(int fd, char *buf, size_t len) {
  const char *p = (const char *)buf;
  size_t written = 0;
  while (written < len) {
    ssize_t ret = write(fd, p + written, len - written);
    if (ret < 0) {
      //if (errno == EINTR) continue;
      return -1;
    }
    written += (size_t)ret;
  }
  return 0;
}

int read_all(int fd, char *buf, size_t len) {
  char *p = (char *)buf;
  size_t got = 0;
  while (got < len) {
    ssize_t ret = read(fd, p + got, len - got);
    if (ret == 0) return -1;              // EOF before full message
    if (ret < 0) {
      //if (errno == EINTR) continue;
      return -1;
    }
    got += (size_t)ret;
  }
  return 0;
}

session_t* innit_session(char const *server_pipe_path, int* nSessions, int max_games) {  //talvez returnar int para erro
    session_t* session;
    int servfd;
    char msg[2];
    if (unlink(server_pipe_path) != 0 && errno != ENOENT) return NULL;
    if (mkfifo(server_pipe_path, 0640) != 0) return NULL; //Não sei o que meter no segundo argumento

    servfd = open(server_pipe_path, O_RDONLY);  //abre o pipe do servidor
    if (servfd < 0) return NULL;
    char buffer[1 + 40 + 40]; //1 para o id, 40 para o req pipe path, 40 para o notif pipe path 
    read_all(servfd, buffer, sizeof(buffer)); //lê o que o cliente enviou
    close(servfd);
    if(buffer[0] != OP_CODE_CONNECT) return NULL;

    session = (session_t*) malloc (sizeof(session_t));
    if (session == NULL) return NULL;

    strncpy(session->req_pipe_path, buffer + 1, 40);
    strncpy(session->notif_pipe_path, buffer + 1 + 40, 40);

    session->notif_pipe = open(session->notif_pipe, O_WRONLY);
    if (session->notif_pipe < 0) {
        free(session);
        return NULL;
    }
    msg[0] = OP_CODE_CONNECT;
    msg[1] = (*nSessions < max_games) ? 0 : 1; //0 se conseguiu criar sessão, 1 se não
    if (write_all(session->notif_pipe, msg, sizeof(msg)) != 0) {
        close(session->notif_pipe);
        free(session);
        return NULL;
    }
    session->id = *nSessions;
    *nSessions += 1;
    return session;    //fechamos os pipes no final
}

void free_session(session_t* session) {
    if (session == NULL) return;
    close(session->req_pipe);
    close(session->notif_pipe);
    free(session);
}