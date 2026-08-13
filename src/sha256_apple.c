#include "sha256.h"

#include <CommonCrypto/CommonDigest.h>

void sha256_once(const uint8_t *data, size_t len, uint8_t out[32]) {
  CC_SHA256_CTX ctx;
  CC_SHA256_Init(&ctx);
  CC_SHA256_Update(&ctx, data, (CC_LONG)len);
  CC_SHA256_Final(out, &ctx);
}
