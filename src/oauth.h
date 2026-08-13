#pragma once

#include <stdbool.h>

#define OOB_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"
#define CLI_CLIENT_NAME "cli ToooT"
#define CLI_SCOPES "read write"

typedef struct {
  char *client_id;
  char *client_secret;
} app_credentials;

void app_credentials_free(app_credentials *c);

[[nodiscard]] bool register_app(const char *instance, app_credentials *out);

[[nodiscard]] char *build_authorize_url(const char *instance,
                                         const char *client_id,
                                         const char *redirect_uri,
                                         const char *scope,
                                         const char *code_challenge,
                                         const char *state);

[[nodiscard]] bool exchange_token(const char *instance, const char *code,
                                   const char *client_id,
                                   const char *client_secret,
                                   const char *redirect_uri,
                                   const char *code_verifier,
                                   char **access_token_out);

[[nodiscard]] bool verify_credentials(const char *instance,
                                       const char *access_token,
                                       char **username_out,
                                       char **account_id_out);

int login(const char *instance);
