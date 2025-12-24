#include "api.h"
#include "protocol.h"
#include "debug.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>


struct Session {
  int id;
  int req_pipe;
  int notif_pipe;
  char req_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
  char notif_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
};

static struct Session session = {.id = -1};



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

int pacman_connect(char const *req_pipe_path, char const *notif_pipe_path, char const *server_pipe_path) {
  // Remove any existing client FIFOs
  if (unlink(req_pipe_path) != 0 && errno != ENOENT) return 1;
  if (unlink(notif_pipe_path) != 0 && errno != ENOENT) return 1;

  // Create client FIFOs
  if (mkfifo(req_pipe_path, 0640) != 0) return 1;    //Não sei o que meter no segundo argumento
  if (mkfifo(notif_pipe_path, 0640) != 0) return 1;

  // Open server FIFO for writing and send CONNECT request
  int servfd = open(server_pipe_path, O_WRONLY);
  if (servfd < 0) return 1;

  // Message format: (char)OP_CODE=1 | (char[40]) req_pipe | (char[40]) notif_pipe
  char msg[1 + 40 + 40];
  memset(msg, '\0', sizeof(msg));
  msg[0] = OP_CODE_CONNECT;
  strncpy(msg + 1, req_pipe_path, 40);
  strncpy(msg + 1 + 40, notif_pipe_path, 40);

  if (write_all(servfd, msg, sizeof(msg)) != 0) {
    close(servfd);
    return 1;
  }
  close(servfd);

  // Open notification FIFO for reading (blocks until server opens it for writing)
  session.notif_pipe = open(notif_pipe_path, O_RDONLY);
  if (session.notif_pipe < 0) return 1;

  // Wait for confirmation: (char)OP_CODE=1 | (char)result
  char resp[2];
  if (read_all(session.notif_pipe, resp, sizeof(resp)) != 0) {
    close(session.notif_pipe);
    session.notif_pipe = -1;
    return 1;
  }
  if (resp[0] != OP_CODE_CONNECT) {
    // Unexpected response
    close(session.notif_pipe);
    session.notif_pipe = -1;
    return 1;
  }
  // Only after confirmation, open request FIFO for writing
  session.req_pipe = open(req_pipe_path, O_WRONLY);
  if (session.req_pipe < 0) {
    debug("Entrou\n");
    close(session.notif_pipe);
    session.notif_pipe = -1;
    return 1;
  }

  // Save paths (optional but useful)
  strncpy(session.req_pipe_path, req_pipe_path, MAX_PIPE_PATH_LENGTH);
  session.req_pipe_path[MAX_PIPE_PATH_LENGTH] = '\0';
  strncpy(session.notif_pipe_path, notif_pipe_path, MAX_PIPE_PATH_LENGTH);
  session.notif_pipe_path[MAX_PIPE_PATH_LENGTH] = '\0';

  
  // Return 0 on success, 1 on error (server decides result)
  return (resp[1] == 0) ? 0 : 1;
}

void pacman_play(char command) {
  char buf[2];
  buf[0] = OP_CODE_PLAY;
  buf[1] = command;
  if(write_all(session.req_pipe, buf, 2) != 0) {
    //error
    //return -1
  }
  //return 0;
  //no enunciado diz para retornar int
  //o enunciado tá todo fodido em comparacao ao codigo base

}

int pacman_disconnect() {
  char buf[1];
  buf[0] = OP_CODE_DISCONNECT;
  write_all(session.req_pipe, buf, 1);

  close(session.req_pipe);
  close(session.notif_pipe);
  debug("Client exiting...\n");
  return 0;
}

Board receive_board_update(void) {
  Board board;
  int width = 0, height = 0;
  int tempo, victory, game_over, accumulated_points;
  
  char buf[1 + (sizeof(int) * 6)];
  memset(buf, '\0', sizeof(buf));

    //verificar se o opcode está certo

  
  if (read_all(session.notif_pipe, buf, 1 + (sizeof(int) * 6)) != 0) {
    //error
  }
    memcpy(&width, buf + 1, sizeof(int));
    memcpy(&height, buf + 1 + sizeof(int),    sizeof(int));
    memcpy(&tempo, buf + 1 + sizeof(int) * 2, sizeof(int));
    memcpy(&victory, buf + 1 + sizeof(int) * 3, sizeof(int));
    memcpy(&game_over, buf + 1 + sizeof(int) * 4, sizeof(int));
    memcpy(&accumulated_points, buf + 1 + sizeof(int) * 5, sizeof(int));
  if (buf[0] != OP_CODE_BOARD) {
    //error
  }

  board.data = malloc((width * height) * sizeof(char));

  if (read_all(session.notif_pipe, board.data, height * width) != 0) {
    //error
  }

  board.width = width;
  board.height = height;
  board.tempo = tempo;
  board.victory = victory;
  board.game_over = game_over;
  board.accumulated_points = accumulated_points;
  //debug("W : %d, H : %d, Tempo : %d, Vic: %d, Over: %d, Acc : %d\n", width, height, tempo, victory, game_over, accumulated_points);
  //debug("%s\n", board.data);
  return board;
}