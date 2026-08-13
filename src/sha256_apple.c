#include "sha256.h"

void sha256_once(const uint8_t *data, size_t len, uint8_t out[32]) {
  (void)data;
  (void)len;
  for (int i = 0; i < 32; i++) out[i] = 0;
}
