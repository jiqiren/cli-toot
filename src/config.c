#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void config_init(config *c) {
  c->instance = nullptr;
  c->client_id = nullptr;
  c->client_secret = nullptr;
  c->access_token = nullptr;
  c->account_id = nullptr;
  c->username = nullptr;
}

void config_free(config *c) {
  free(c->instance);
  free(c->client_id);
  free(c->client_secret);
  free(c->access_token);
  free(c->account_id);
  free(c->username);
  config_init(c);
}

char *config_path(void) {
  const char *xdg = getenv("XDG_CONFIG_HOME");
  const char *home;
  if (xdg != nullptr && xdg[0] != '\0') {
    size_t n = strlen(xdg) + strlen("/sloptoot/config") + 1;
    char *p = malloc(n);
    snprintf(p, n, "%s/sloptoot/config", xdg);
    return p;
  }
  home = getenv("HOME");
  if (home == nullptr || home[0] == '\0') return nullptr;
  size_t n = strlen(home) + strlen("/.config/sloptoot/config") + 1;
  char *p = malloc(n);
  snprintf(p, n, "%s/.config/sloptoot/config", home);
  return p;
}

const char *config_get(const config *c, const char *key) {
  if (strcmp(key, "instance") == 0) return c->instance;
  if (strcmp(key, "client_id") == 0) return c->client_id;
  if (strcmp(key, "client_secret") == 0) return c->client_secret;
  if (strcmp(key, "access_token") == 0) return c->access_token;
  if (strcmp(key, "account_id") == 0) return c->account_id;
  if (strcmp(key, "username") == 0) return c->username;
  return nullptr;
}

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t') s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' ||
                     end[-1] == '\t')) {
    end--;
  }
  *end = '\0';
  return s;
}

static void set_field(config *c, const char *key, char *val) {
  if (strcmp(key, "instance") == 0) {
    free(c->instance);
    c->instance = strdup(val);
  } else if (strcmp(key, "client_id") == 0) {
    free(c->client_id);
    c->client_id = strdup(val);
  } else if (strcmp(key, "client_secret") == 0) {
    free(c->client_secret);
    c->client_secret = strdup(val);
  } else if (strcmp(key, "access_token") == 0) {
    free(c->access_token);
    c->access_token = strdup(val);
  } else if (strcmp(key, "account_id") == 0) {
    free(c->account_id);
    c->account_id = strdup(val);
  } else if (strcmp(key, "username") == 0) {
    free(c->username);
    c->username = strdup(val);
  }
}

bool config_load(config *c) {
  config_init(c);
  char *path = config_path();
  if (path == nullptr) return false;
  FILE *f = fopen(path, "r");
  free(path);
  if (f == nullptr) return false;

  char line[1024];
  while (fgets(line, sizeof(line), f) != nullptr) {
    char *eq = strchr(line, '=');
    if (eq == nullptr) continue;
    *eq = '\0';
    char *key = trim(line);
    char *val = trim(eq + 1);
    set_field(c, key, val);
  }
  fclose(f);
  return true;
}

static bool ensure_dir(const char *path) {
  char *copy = strdup(path);
  char *slash = strrchr(copy, '/');
  if (slash == copy) {
    free(copy);
    return true;
  }
  if (slash != nullptr) {
    *slash = '\0';
    for (char *p = copy + 1; *p != '\0'; p++) {
      if (*p == '/') {
        *p = '\0';
        if (mkdir(copy, 0700) != 0 && errno != EEXIST) {
          free(copy);
          return false;
        }
        *p = '/';
      }
    }
    if (mkdir(copy, 0700) != 0 && errno != EEXIST) {
      free(copy);
      return false;
    }
  }
  free(copy);
  return true;
}

bool config_save(const config *c) {
  char *path = config_path();
  if (path == nullptr) return false;
  if (!ensure_dir(path)) {
    free(path);
    return false;
  }

  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  free(path);
  if (fd < 0) return false;
  if (fchmod(fd, 0600) != 0) {
    close(fd);
    return false;
  }

  FILE *f = fdopen(fd, "w");
  if (f == nullptr) {
    close(fd);
    return false;
  }

  if (c->instance != nullptr) fprintf(f, "instance=%s\n", c->instance);
  if (c->client_id != nullptr) fprintf(f, "client_id=%s\n", c->client_id);
  if (c->client_secret != nullptr)
    fprintf(f, "client_secret=%s\n", c->client_secret);
  if (c->access_token != nullptr)
    fprintf(f, "access_token=%s\n", c->access_token);
  if (c->account_id != nullptr) fprintf(f, "account_id=%s\n", c->account_id);
  if (c->username != nullptr) fprintf(f, "username=%s\n", c->username);

  fclose(f);
  return true;
}
