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

char content[4096];

int main()
{
  FILE *fp = fopen("index.html", "rb");

  fseek(fp, 0, SEEK_SET);
  while (fread(content, 1024, 1, fp)) {
    CYAN("%s", content);
  }
    CYAN("%s", content);

  fclose(fp);

  return 0;
}
