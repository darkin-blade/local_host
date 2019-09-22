#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define CYAN(format, ...) \
  printf("\033[1;36m" format "\33[0m\n", ## __VA_ARGS__)

struct sockaddr_in s_addr;
struct sockaddr_in c_addr;
socklen_t c_addr_size;
int s_sock;// server socket
int c_sock;// clinet socket

char buf[4096];// user agent
char msg[4096];// file content
char head[1024];// http header
char file[128];// which file requested
char type[128];// file format

void init_server();
void read_request();
void send_file();
void send_helper(char *, int);

int main() 
{
  init_server();

  while (1) {
    c_sock = accept(s_sock, NULL, NULL);
    if (c_sock != -1) {
      int nread = recv(c_sock, buf, sizeof(buf), 0);
      read_request();// TODO

      // CYAN("%d", nread);
      // CYAN("%s", buf);

      send_file();
      close(c_sock);
    }
  }

  close(s_sock);
  return 0;
}

void init_server() 
{
  s_sock = socket(AF_INET, SOCK_STREAM, 0);
  assert(s_sock != -1);
  s_addr.sin_family = AF_INET;
  s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  s_addr.sin_port = htons(8000);

  int res = bind(s_sock, (struct sockaddr*)&s_addr, sizeof(s_addr));
  if (res == -1) { perror("cannot bind"); exit(-1); }

  listen(s_sock, 10);// TODO

  c_addr_size = sizeof(c_addr);
}

void read_request()
{
  int buf_len = strlen(buf);
  int i = 0, j = 0;
  for (i = 0; i < buf_len - 10; i ++) {
    if (buf[i] == 'G' && buf[i + 1] == 'E' && buf[i + 2] == 'T') {// `GET` keyword
      i = i + 4;// skip space
      while (buf[i] != ' ') {
        file[j] = buf[i];
        j ++, i ++;
      }
      file[j] = '\0';
      return;
    }
  }
}

void send_file()
{
  int is_html = 1;
  if (strcmp(file, "/") == 0) {
    sprintf(file, "index.html");
    sprintf(type, ".html");
  } else {
    sprintf(file, "%s", file + 1);// skip `/`
    int i = 0, j = 0;
    for (i = strlen(file); file[i] != '.'; i --) {// find `.`
      ;
    }
    for (j = 0; i < strlen(file); i ++, j ++) {
      type[j] = file[i];
    }
    type[j] = '\0';
    CYAN("%s %s", file, type);
  }

  // count file length
  int fd = open(file, O_RDONLY);
  int file_len = lseek(fd, 0, SEEK_END);

  // send http header
  if (strcmp(type, ".html") == 0) {
    sprintf(type, "text/html");
  } else if (strcmp(type, ".js") == 0) {
    sprintf(type, "application/x-javascript");
  } else if (strcmp(type, ".css") == 0) {
    sprintf(type, "text/css");
  } else if (strcmp(type, ".png") == 0) {
    sprintf(type, "image/png");
  } else if (strcmp(type, ".ico") == 0) {
    sprintf(type, "image/x-ico");
  } else if (strcmp(type, ".json") == 0) {
    sprintf(type, "application/x-javascript");
  } else {
    sprintf(type, "application/octet-stream");
  }
  sprintf(head, 
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      , type
      , file_len
      );
  send(c_sock, head, strlen(head), 0);

  // send file content
  lseek(fd, 0, SEEK_SET);
  memset(msg, 0, sizeof(msg));

  int delta = 0;
  while (delta < file_len) {// read by lines
    memset(msg, 0, 4096);
    int size = read(fd, msg, 1024);
    delta += size;
    // send(c_sock, msg, strlen(msg), 0);
    send_helper(msg, size);
    CYAN("%d %d", delta, file_len);
  }

  close(fd);
}

void send_helper(char *content, int size)
{
  while (size > 0) {
    int delta = send(c_sock, content, size, 0);
    if (delta <= 0) return;
    size -= delta;
    content += delta;
  }
}
