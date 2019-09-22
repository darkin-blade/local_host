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

char content[4096];

int main()
{
  int fd = open("index.html", O_RDONLY);

  int file_len = lseek(fd, 0, SEEK_END);
  int delta = 0;
  lseek(fd, 0, SEEK_SET);
  while (delta < file_len) {
    delta += read(fd, content, 1024);
    CYAN("%d %d", delta, file_len);
  }

  close(fd);

  return 0;
}
