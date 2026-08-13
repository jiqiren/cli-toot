#pragma once

#include <stddef.h>
#include <stdint.h>

[[nodiscard]] char *base64url_encode(const uint8_t *data, size_t len);

[[nodiscard]] char *random_verifier(size_t bytes);
