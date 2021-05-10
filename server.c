#include <assert.h>
#include <dirent.h>
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
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define RED(format, ...) \
  printf("\033[1;31m" format "\33[0m\n", ## __VA_ARGS__)
#define GREEN(format, ...) \
  printf("\033[1;32m" format "\33[0m\n", ## __VA_ARGS__)
#define MAGENTA(format, ...) \
  printf("\033[1;35m" format "\33[0m\n", ## __VA_ARGS__)
#define CYAN(format, ...) \
  printf("\033[1;36m" format "\33[0m\n", ## __VA_ARGS__)

const int VIDEO_SIZE = 4096;// 1048576

// socket
struct sockaddr_in s_addr;
struct sockaddr_in c_addr;
socklen_t c_addr_size;
int s_sock;// server socket
int c_sock;// clinet socket

// request
int header_len;// 请求头部的长度
char request_header[8192];// user agent
char file_requested[256];// 完整路径的request
// range = end - start
// 请求头部的关于range大小的信息
int range_start;
int range_end;

// response
char response_header[1024];// http response header
char file_content[4096];// file content
char content_type[128];// content type
// content range = end - start
// 根据本地文件判断得到的content大小信息
int content_total;

// 线程
pthread_t send_thread;// 传输文件的线程

void init_server(int argc, char *argv[]);

void read_request();
void main_response();
void *thread_response(void *args);

// 传输不同类型的文件
void send_file();// 普通类型的文件
void send_dir();// 目录
void send_404();// 无效文件
void send_none();// 空文件

// 功能函数
void send_helper(char *, int);

// 其他
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

      int thread_result = pthread_create(&send_thread, NULL, thread_response, NULL);
      if (thread_result) {
        RED("Failed to create thread to send file");
        assert(0);// TODO
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
  header_len = strlen(request_header);
  // memset(file_requested, 0, sizeof(file_requested));
  // memset(content_type, 0, sizeof(content_type));

  // 解析请求的文件
  for (int i = 0, j = 0; i < header_len - 2; i ++) {
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
}

void *thread_response(void *args)
{
  main_response();
}

void main_response()
{
  // 获取文件全名
  char temp[128];
  strcpy(temp, file_requested + 1);// skip 第一个 `/`
  sprintf(file_requested, "%s/%s", rootDir, temp);

  // 判断file_requested的文件类型
  struct stat file_stat;
  if (lstat(file_requested, &file_stat) == -1) {
    extern int errno;// 查看文件打开失败时的错误原因
    RED("[%d] invalid file (%s)", errno, file_requested);
    send_none();
  } else {
    switch (file_stat.st_mode & S_IFMT) {
      case S_IFBLK:
        RED("block device\n");            
        send_404();
        break;
      case S_IFCHR:
        RED("character device\n");        
        send_404();
        break;
      case S_IFDIR:
        send_dir();              
        break;
      case S_IFIFO:
        RED("FIFO/pipe\n");               
        send_404();
        break;
      case S_IFLNK:
        RED("symlink\n");                 
        send_404();
        break;
      case S_IFREG:
        send_file();      
        break;
      case S_IFSOCK:
        RED("socket\n");                  
        send_404();
        break;
      default:
        RED("unknown?\n");                
        send_404();
        break;
    }
  }
}

void send_file() {
  // 传输普通文件
  int i, j;

  // 解析请求文件的格式
  int filename_len = strlen(file_requested);
  for (i = filename_len; file_requested[i] != '.'; i --) {// find `.`
    // do nothing
  }
  for (j = 0; i < filename_len; i ++, j ++) {
    content_type[j] = file_requested[i];
  }
  content_type[j] = '\0';

  CYAN("[%s]", file_requested);
  CYAN("[%s]", content_type);

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

  // 判断文件是否有效
  int fd = open(file_requested, O_RDONLY);
  extern int errno;// 查看文件打开失败时的错误原因
  if (fd < 0) {
    RED("[%d] can't open %s", errno, file_requested);
  }
  content_total = lseek(fd, 0, SEEK_END);// 计算文件总大小
  int range_total;// 传输部分的总大小

  // 2 根据file_requested的后缀判断文件类型, 以使用不同的传输方式
  int partial_content = 0;
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
      range_start = 0;// TODO 从头开始传输
    }
    partial_content = 1;// 断点续传
  } else {
    sprintf(content_type, "application/octet-stream");
  }
  if (partial_content) {// 断点续传协议
    // 计算传输范围
    if (range_end == -1) {
      // 直至文件末尾
      range_end = my_min(content_total - 1, range_start + VIDEO_SIZE);// TODO
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
    if (size <= 0) {
      // TODO 未处理的bug
      break;
    }
    delta += size;
    // send(c_sock, file_content, strlen(file_content), 0);
    send_helper(file_content, size);
    GREEN("%d/%d", delta, range_total);
  }

  close(fd);
  close(c_sock);// 关闭client
}

void send_dir() {
  struct dirent *file_item;
  int file_num = 0;
  char link_item[1024];

  // html头部
  sprintf(file_content, 
  "<html>\r\n"
  "<head>\r\n"
  "  <h1>%s</h1>\r\n"
  "</head>\r\n"
  "<body>\r\n", file_requested);

  // 遍历目录下的文件
  DIR *requested_dir = opendir(file_requested);
  while (file_item = readdir(requested_dir)) {
    // 只显示前100个文件
    if (file_num > 100) break;
    sprintf(link_item, "<a href=\"%s\">%s<a><br>\r\n", file_item->d_name, file_item->d_name);
    strcat(file_content, link_item);
    file_num ++;
  }
  strcat(file_content,
  "</body>\r\n"
  "</html>\r\n");
  
  sprintf(content_type, "text/html");
  // 传输全部文件
  int range_total = strlen(file_content);// 传输部分的总大小
  sprintf(response_header, 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %d\r\n"
    "\r\n"
    , content_type
    , range_total
    );
  send(c_sock, response_header, strlen(response_header), 0);
  MAGENTA("%s", response_header);

  send_helper(file_content, range_total);

  close(c_sock);// 关闭client
}

void send_404()
{
  // 404页面
  sprintf(file_content,
    "<html>\r\n"
    "  <head>\r\n"
    "    <h1>404</h1>\r\n"
    "  </head>\r\n"
    "</html>\r\n");


  // 2 根据file_requested的后缀判断文件类型, 以使用不同的传输方式

  sprintf(content_type, "text/html");
  // 传输全部文件
  int range_total = strlen(file_content);// 传输部分的总大小
  sprintf(response_header, 
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      , content_type
      , range_total
      );
  send(c_sock, response_header, strlen(response_header), 0);
  MAGENTA("%s", response_header);

  send_helper(file_content, range_total);

  close(c_sock);// 关闭client
}

void send_none()
{
  // TODO 空文件
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
