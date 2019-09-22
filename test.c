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

struct sockaddr_in s_addr;
char buf[4096];
char head[1024];
char msg[4096];

int s_socket;
int c_socket;

void init_server();
void read_request();
void send_file();

int main() {
  init_server();

  while (1) {
    c_socket = accept(s_socket, NULL, NULL);
    if (c_socket != -1) {
      int nread = recv(c_socket, buf, sizeof(buf), 0);
      CYAN("%d\n%s", nread, buf);

      send_file();
      close(c_socket);
    }
  }
  close(s_socket);

  return 0;
}

void init_server()
{
  s_socket = socket(AF_INET, SOCK_STREAM, 0);
  assert(s_socket != -1);
  s_addr.sin_family = AF_INET;
  s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  s_addr.sin_port = htons(8000);

  int res = bind(s_socket, (struct sockaddr*)&s_addr, sizeof(s_addr));
  if (res == -1) { perror("cannot bind"); exit(-1); }

  listen(s_socket, 10);
}

void read_request()
{
  ;
}

void send_file()
{
  sprintf(msg,
      "<!DOCTYPE html>\n"
      "<html><body><h1>Hello, World!</h1></body></html>\n");
  sprintf(head, 
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/html\n"
      "Content-Length: %d\n\n", strlen(msg));
  send(c_socket, head, strlen(head), 0);
  send(c_socket, msg, strlen(msg), 0);
}
