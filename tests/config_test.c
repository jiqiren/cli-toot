#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
  const char *tmp = getenv("XDG_CONFIG_HOME");
  (void)tmp;

  char dir[] = "/tmp/sloptoot-test-XXXXXX";
  if (mkdtemp(dir) == nullptr) return 1;

  if (setenv("XDG_CONFIG_HOME", dir, 1) != 0) return 1;

  config c;
  config_init(&c);
  c.instance = strdup("example.social");
  c.client_id = strdup("cid");
  c.client_secret = strdup("csecret");
  c.access_token = strdup("atok");
  c.account_id = strdup("12345");
  c.username = strdup("bryan");

  if (!config_save(&c)) {
    fprintf(stderr, "save failed\n");
    return 1;
  }
  config_free(&c);

  if (!config_load(&c)) {
    fprintf(stderr, "load failed\n");
    return 1;
  }

  if (strcmp(c.instance, "example.social") != 0 ||
      strcmp(c.client_id, "cid") != 0 ||
      strcmp(c.client_secret, "csecret") != 0 ||
      strcmp(c.access_token, "atok") != 0 ||
      strcmp(c.account_id, "12345") != 0 ||
      strcmp(c.username, "bryan") != 0) {
    fprintf(stderr, "round-trip mismatch\n");
    return 1;
  }

  char *p = config_path();
  struct stat st;
  if (stat(p, &st) != 0) {
    fprintf(stderr, "stat failed\n");
    return 1;
  }
  if ((st.st_mode & 0777) != 0600) {
    fprintf(stderr, "wrong mode: %o\n", st.st_mode & 0777);
    return 1;
  }

  config_free(&c);
  printf("config tests ok\n");
  return 0;
}
