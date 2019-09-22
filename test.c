#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define CYAN(format, ...) \
  printf("\033[1;36m" format "\33[0m\n", ## __VA_ARGS__)

struct sockaddr_in servaddr;
char buf[4096];
char head[1024];
const char *msg =
  "<!DOCTYPE html>\n"
  "<html><body><h1>Hello, World!</h1></body></html>\n";

int main() {
  CYAN("%d\n", strlen(msg));
  sprintf(head, 
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/html\n\n"
      "Content-Length: %d\n\n", 1000
      );
  CYAN("%s\n", strcat(head, msg));

  int res, s_socket = socket(AF_INET, SOCK_STREAM, 0);
  assert(s_socket != -1);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  servaddr.sin_port = htons(8000);

  res = bind(s_socket, (struct sockaddr*)&servaddr, sizeof(servaddr));
  if (res == -1) { perror("cannot bind"); exit(-1); }

  listen(s_socket, 10);
  while (1) {
    int conn = accept(s_socket, NULL, NULL);
    if (conn != -1) {
      int nread = recv(conn, buf, sizeof(buf), 0);
      // send(conn, strcat(head, msg), strlen(head) + strlen(msg), 0);
      send(conn, head, strlen(head), 0);
      send(conn, msg, strlen(msg), 0);
      close(conn);
    }
  }
  close(s_socket);

  return 0;
}
