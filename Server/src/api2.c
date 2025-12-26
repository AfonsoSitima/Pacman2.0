#include "api2.h"
#include "board.h"
#include "protocol2.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>


int write_all(int fd, char *buf, size_t len) {
  //talvez tenhamos que por uma lock aqui se vários threads escreverem no mesmo pipe
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

void innit_session(session_t * session, int sessionId) {  //talvez returnar int para erro
  char msg[2];
  debug("%s\n", session->req_pipe_path);
  debug("%s\n", session->notif_pipe_path);

  session->notif_pipe = open(session->notif_pipe_path, O_WRONLY);
  if (session->notif_pipe < 0) {
      free(session);
      return;
  }
  msg[0] = OP_CODE_CONNECT;
  msg[1] = 0;
  if (write_all(session->notif_pipe, msg, sizeof(msg)) != 0) {
      close(session->notif_pipe);
      free(session);
      return;
  }

  session->req_pipe = open(session->req_pipe_path, O_RDONLY);
  if (session->notif_pipe < 0){
    free(session);
    return;
  }

  session->id = sessionId;
  return;    //fechamos os pipes no final
}

char get_pacman_command(session_t* session) {
  int reqfd = session->req_pipe;
  if (reqfd < 0) return '\0';
  char command[2];
  if (read_all(reqfd, command, sizeof(command)) != 0) {
      return '\0';
  }
  debug("Comando recebido : %c\n", command[1]);
  return command[1];
}

void disconnect_session(session_t* session) {
  char op_code[1];
  read_all(session->req_pipe, op_code, 1);
  if (op_code[0] != OP_CODE_DISCONNECT) {
      //error
      return;
  }
  close(session->req_pipe);
  close(session->notif_pipe);
  free(session);
}