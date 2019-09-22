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
#define SERVER

struct sockaddr_in s_addr, c_addr;
// const char *msg = 
  // "HTTP/1.1 200 OK\n"
  // "Content-Type: text/html\n"
  // "Content-Length: 48\n"
  // "\n"
  // "<html><body><h1>Hello, World!</h1></body></html>\n";
char buf[4096];// user agent
char line[1024];// 一行行读取
char msg[4096] = {0};// html内容
char head[128];// html头

int main() {

#ifdef SERVER
  int s_sock = socket(AF_INET, SOCK_STREAM, 0);// fd为server的socket
  assert(s_sock != -1);
  s_addr.sin_family = AF_INET;
  // s_addr.sin_addr.s_addr = htonl(INADDR_ANY);// 监听任何地址
  s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  s_addr.sin_port = htons(8000);

  // 绑定服务端的地址和端口
  int res = bind(s_sock, (struct sockaddr*)&s_addr, sizeof(s_addr));
  if (res == -1) { perror("cannot bind"); exit(-1); }

  listen(s_sock, 10);

  socklen_t c_addr_size = sizeof(c_addr);
  while (1) {
    int c_sock = accept(s_sock, (struct sockaddr *)&c_addr, &c_addr_size);// 客户端socket
    if (c_sock != -1) {
      int nread = recv(c_sock, buf, sizeof(buf), 0);
      CYAN("%d", nread);
      CYAN("%s", buf);
      // send(c_sock, msg, strlen(msg), 0);
#endif

      FILE *fp = fopen("test.html", "r");
      while (fgets(line, 1000, fp)) {// 一行行
        sprintf(msg + strlen(msg), "%s", line);
      }
      sprintf(head, 
          "HTTP/1.1 200 OK\n"
          "Content-Type: text/html\n"
          "Content-Length: %d\n\n", (int)strlen(msg));

#ifdef SERVER
      send(c_sock, head, strlen(head), 0);
      send(c_sock, msg, strlen(msg), 0);
      close(c_sock);
    }
  }
  close(s_sock);
#endif

  return 0;
}
