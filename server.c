#include <stdio.h>
#include <string.h>
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
struct sockaddr_in c_addr;
socklen_t c_addr_size;
int s_sock;// server socket
int c_sock;// clinet socket

char buf[4096];// user agent
char msg[4096];// file content
char head[1024];// http header
char file[128];// which file requested

void init_server();
void read_request();
void send_file();

int main() 
{
  init_server();

  while (1) {
    c_sock = accept(s_sock, NULL, NULL);
    if (c_sock != -1) {
      int nread = recv(c_sock, buf, sizeof(buf), 0);
      read_request();// TODO

      CYAN("%d", nread);
      CYAN("%s", buf);

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
      CYAN("%s", file);
      return;
    }
  }
}

void send_file()
{
  int is_html = 0;
  if (strcmp(file, "/") == 0) {
    sprintf(file, "index.html");
    is_html = 1;
  } else {
    sprintf(file, "%s", file + 1);// skip `/`
  }

  FILE *fp = fopen(file, "r");
  if (1 == 1) {
    // count file length
    int file_len = 0;
    while (fgets(msg, 1000, fp)) {// read by lines
      file_len += strlen(msg);
    }

    // send http header
    sprintf(head, 
        "HTTP/1.1 200 OK\n"
        // "Content-Type: text/html\n"
        "Content-Length: %d\n"
        "\n", file_len
        );

    // send file content
    CYAN("%d", fseek(fp, 0, SEEK_SET));
    memset(msg, 0, sizeof(msg));
    send(c_sock, head, strlen(head), 0);
    while (fgets(msg, 1000, fp)) {// read by lines
      send(c_sock, msg, strlen(msg), 0);
    }
  } else if (is_html == 0) {
    while (fgets(msg, 1000, fp)) {// read by lines
      send(c_sock, msg, strlen(msg), 0);
    }
  }
  fclose(fp);
}
