#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define RED(format, ...) \
  printf("\033[1;31m" format "\33[0m\n", ## __VA_ARGS__)
#define GREEN(format, ...) \
  printf("\033[1;32m" format "\33[0m\n", ## __VA_ARGS__)
#define MAGENTA(format, ...) \
  printf("\033[1;35m" format "\33[0m\n", ## __VA_ARGS__)
#define CYAN(format, ...) \
  printf("\033[1;36m" format "\33[0m\n", ## __VA_ARGS__)

struct sockaddr_in s_addr;
struct sockaddr_in c_addr;
socklen_t c_addr_size;
int s_sock;// server socket
int c_sock;// clinet socket

char request_header[4096];// user agent
char file_requested[128];// which file requested
// range = end - start
int range_start;
int range_end;

char response_header[1024];// http response header
char file_content[4096];// file content
char content_type[128];// content type
// content range = end - start
int content_start;
int content_end;
int content_total;

// 线程
pthread_t send_thread;// 传输文件的线程

void init_server(int argc, char *argv[]);
void read_request();
void send_file();
void send_helper(char *, int);

void sigpipe_handler();
int my_min(int a, int b) {
  if (a < b) return a;
  return b;
}

char ip_addr[32] = "127.0.0.1";// 127.0.0.1
int port_num = 8000;
char rootDir[128] = "/home/lynx/darkin_blade/html_player";

int main(int argc, char *argv[]) 
{
  init_server(argc, argv);

  while (1) {
    int new_sock = accept(s_sock, NULL, NULL);// 接收新的请求
    pthread_join(send_thread, NULL);

    c_sock = new_sock;
    if (c_sock != -1) {
      int nread = recv(c_sock, request_header, sizeof(request_header), 0);
      CYAN("%s", request_header);
      // CYAN("%d", nread);

      read_request();

      int thread_result = pthread_create(&send_thread, NULL, send_file, NULL);
      if (thread_result) {
        RED("Failed to create thread to send file");
        send_file();// TODO
      }

      // close(c_sock);// 关闭client
    }
  }

  close(s_sock);// 关闭server
  return 0;
}

void init_server(int argc, char *argv[]) 
{
  if (argc > 1) {// give a specific path
    sprintf(ip_addr, "%s", argv[1]);
    if (argc > 2) {
      port_num = atoi(argv[2]);
      if (argc > 3) {
        sprintf(rootDir, "%s", argv[3]);
      }
    }
  }
  GREEN("%s:%d", ip_addr, port_num);

  s_sock = socket(AF_INET, SOCK_STREAM, 0);
  assert(s_sock != -1);
  s_addr.sin_family = AF_INET;
  s_addr.sin_addr.s_addr = inet_addr(ip_addr);
  s_addr.sin_port = htons(port_num);

  int reuse = 1;// 避免time wait
  if (setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) {
    perror("setsockopt");
    exit(-1);
  }
  int res = bind(s_sock, (struct sockaddr*)&s_addr, sizeof(s_addr));
  if (res == -1) { perror("cannot bind"); exit(-1); }

  listen(s_sock, 10);// TODO

  c_addr_size = sizeof(c_addr);

  // TODO 信号处理
  signal(SIGPIPE, sigpipe_handler);
}

void read_request()
{
  int i, j;
  int header_len = strlen(request_header);
  // memset(file_requested, 0, sizeof(file_requested));
  // memset(content_type, 0, sizeof(content_type));

  // 解析请求的文件
  for (i = 0, j = 0; i < header_len - 2; i ++) {
    if (request_header[i] == 'G' &&
        request_header[i + 1] == 'E' &&
        request_header[i + 2] == 'T') {// `GET` keyword
      i = i + 4;// skip `GET `
      while (request_header[i] != ' ') {
        file_requested[j] = request_header[i];
        j ++, i ++;
      }
      file_requested[j] = '\0';
    }
  }

  // 解析请求文件的格式
  if (strcmp(file_requested, "/") == 0) {
    sprintf(file_requested, "%s/index.html", rootDir);
    sprintf(content_type, ".html");
  } else {
    char temp[128];
    strcpy(temp, file_requested + 1);// skip `/`
    sprintf(file_requested, "%s/%s", rootDir, temp);
    int filename_len = strlen(file_requested);
    for (i = filename_len; file_requested[i] != '.'; i --) {// find `.`
      // do nothing
    }
    for (j = 0; i < filename_len; i ++, j ++) {
      content_type[j] = file_requested[i];
    }
    content_type[j] = '\0';
  }

  GREEN("[%s]", file_requested);
  GREEN("[%s]", content_type);

  // 解析请求文件的范围
  range_start = -1;
  range_end = -1;
  for (i = 0; i < header_len - 4; i ++) {
    if (request_header[i] == 'R' &&
        request_header[i + 1] == 'a' &&
        request_header[i + 2] == 'n' &&
        request_header[i + 3] == 'g' &&
        request_header[i + 4] == 'e') {
      i += 13;// skip `Range: bytes=`
      range_start = 0;
      while (request_header[i] >= '0' && request_header[i] <= '9') {
        range_start = range_start * 10 + request_header[i] - '0';
        i ++;
      }
      assert(request_header[i] == '-');
      i ++;
      if (request_header[i] >= '0' && request_header[i] <= '9') {
        range_end = 0;
        while (request_header[i] >= '0' && request_header[i] <= '9') {
          range_end = range_end * 10 + request_header[i] - '0';
          i ++;
        }
      } else {
        // 直至文件末尾
        assert(range_end == -1);
      }
    }
  }
}

void send_file()
{
  // CYAN("%s %d: %s %s", __func__, __LINE__, file_requested, content_type);

  int partial_content = 0;
  int fd = open(file_requested, O_RDONLY);
  extern int errno;// 查看文件打开失败时的错误原因
  if (fd < 0) {
    RED("[%d] %s", errno, file_requested);
  }
  content_total = lseek(fd, 0, SEEK_END);// 计算文件总大小
  int range_total;// 传输部分的总大小

  // send http header
  if (strcmp(content_type, ".html") == 0) {
    sprintf(content_type, "text/html");
  } else if (strcmp(content_type, ".js") == 0) {
    sprintf(content_type, "application/x-javascript");
  } else if (strcmp(content_type, ".css") == 0) {
    sprintf(content_type, "text/css");
  } else if (strcmp(content_type, ".png") == 0) {
    sprintf(content_type, "image/png");
  } else if (strcmp(content_type, ".ico") == 0) {
    sprintf(content_type, "image/x-ico");
  } else if (strcmp(content_type, ".json") == 0) {
    sprintf(content_type, "application/x-javascript");
  } else if (strcmp(content_type, ".mp4") == 0) {
    sprintf(content_type, "video/mp4");
    if (range_start == -1) {
      range_start = 0;// TODO
    }
    partial_content = 1;// 断点续传
  } else {
    sprintf(content_type, "application/octet-stream");
  }
  if (partial_content) {// 断点续传协议
    // 计算传输范围
    if (range_end == -1) {
      // 直至文件末尾
      range_end = my_min(content_total - 1, range_start + 1048576);// TODO
    }
    range_total = range_end - range_start;
    sprintf(response_header, 
        "HTTP/1.1 206 Partial Content\r\n"
        "Content-Type: %s\r\n"
        "Content-Range: bytes %d-%d/%d\r\n"
        "Content-Length: %d\r\n"
        "Accept-Ranges: bytes\r\n"
        "\r\n"
        , content_type
        , range_start, range_end, content_total
        , range_total
        );
    // 发送头部
    send(c_sock, response_header, strlen(response_header), 0);
    // 设置文件偏移
    lseek(fd, range_start, SEEK_SET);
    memset(file_content, 0, sizeof(file_content));
  } else {
    // 传输全部文件
    range_total = content_total;
    sprintf(response_header, 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        , content_type
        , range_total
        );
    send(c_sock, response_header, strlen(response_header), 0);

    lseek(fd, 0, SEEK_SET);
    memset(file_content, 0, sizeof(file_content));
  }
  MAGENTA("%s", response_header);

  int delta = 0;
  while (delta < range_total) {
    memset(file_content, 0, 4096);
    int size = read(fd, file_content, 1024);
    delta += size;
    // send(c_sock, file_content, strlen(file_content), 0);
    send_helper(file_content, size);
    GREEN("%d/%d", delta, range_total);
  }

  close(fd);
  close(c_sock);// 关闭client
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

void sigpipe_handler() {
  pthread_cancel(send_thread);
  // exit(1);
}
