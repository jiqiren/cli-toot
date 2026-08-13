#include "sha256.h"
#include "base64.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *hex(const uint8_t *b, size_t n) {
  char *out = malloc(n * 2 + 1);
  for (size_t i = 0; i < n; i++)
    sprintf(out + i * 2, "%02x", b[i]);
  out[n * 2] = '\0';
  return out;
}

int main(void) {
  uint8_t out[32];
  sha256_once((const uint8_t *)"abc", 3, out);
  char *h = hex(out, 32);
  if (strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) {
    fprintf(stderr, "sha256(abc) mismatch: %s\n", h);
    free(h);
    return 1;
  }
  free(h);

  char *v = random_verifier(32);
  if (v == nullptr || strlen(v) != 43) {
    fprintf(stderr, "verifier length wrong\n");
    free(v);
    return 1;
  }
  free(v);

  printf("crypto tests ok\n");
  return 0;
}
