#pragma once

#include <cjson/cJSON.h>

#include <stddef.h>

[[nodiscard]] cJSON *json_parse(const char *body, size_t len);

[[nodiscard]] const char *json_get_string(const cJSON *root, const char *key);

[[nodiscard]] int json_get_int(const cJSON *root, const char *key);

void json_free(cJSON *root);
