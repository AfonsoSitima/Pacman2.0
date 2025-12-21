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



static int write_all(int fd, const void *buf, size_t len) {
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

static int read_all(int fd, void *buf, size_t len) {
  char *p = (char *)buf;
  size_t got = 0;
  while (got < len) {
    ssize_t ret = read(fd, p + got, len - got);
    if (ret == 0) return -1;              // EOF before full message
    if (ret < 0) {
      if (errno == EINTR) continue;
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
  if (mkfifo(req_pipe_path, 0640) != 0) return 1;
  if (mkfifo(notif_pipe_path, 0640) != 0) return 1;

  // Open server FIFO for writing and send CONNECT request
  int servfd = open(server_pipe_path, O_WRONLY);
  if (servfd < 0) return 1;

  // Message format: (char)OP_CODE=1 | (char[40]) req_pipe | (char[40]) notif_pipe
  char msg[1 + 40 + 40];
  memset(msg, 0, sizeof(msg));
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

  // TODO - implement me

}

int pacman_disconnect() {
  write_all(session.req_pipe, OP_CODE_DISCONNECT, 1);
  close(session.req_pipe);
  close(session.notif_pipe);
  return 0;
}

Board receive_board_update(void) {
    // TODO - implement me
}