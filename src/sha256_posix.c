#include "sha256.h"

#include <openssl/sha.h>

void sha256_once(const uint8_t *data, size_t len, uint8_t out[32]) {
  SHA256(data, len, out);
}
