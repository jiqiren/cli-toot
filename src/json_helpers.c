#include "json_helpers.h"

#include <stdlib.h>
#include <string.h>

cJSON *json_parse(const char *body, size_t len) {
  if (len == 0) len = strlen(body);
  return cJSON_ParseWithLength(body, len);
}

const char *json_get_string(const cJSON *root, const char *key) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (item == nullptr || !cJSON_IsString(item)) return nullptr;
  return item->valuestring;
}

int json_get_int(const cJSON *root, const char *key) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (item == nullptr || !cJSON_IsNumber(item)) return 0;
  return item->valueint;
}

void json_free(cJSON *root) { cJSON_Delete(root); }
