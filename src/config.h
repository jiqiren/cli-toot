#pragma once

#include <stdbool.h>

typedef struct {
  char *instance;
  char *client_id;
  char *client_secret;
  char *access_token;
  char *account_id;
  char *username;
} config;

[[nodiscard]] char *config_path(void);

void config_init(config *c);
void config_free(config *c);

[[nodiscard]] bool config_load(config *c);
[[nodiscard]] bool config_save(const config *c);

const char *config_get(const config *c, const char *key);
