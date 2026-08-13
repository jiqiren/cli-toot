#include <stdio.h>

#include "http.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  http_global_init();
  printf("cli-toot stub\n");
  http_global_cleanup();
  return 0;
}
