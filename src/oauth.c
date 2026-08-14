#include "oauth.h"

#include "base64.h"
#include "browser.h"
#include "config.h"
#include "http.h"
#include "json_helpers.h"
#include "loopback.h"
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *join_url(const char *instance, const char *path) {
  size_t n = strlen(instance) + strlen(path) + 1;
  char *out = malloc(n);
  if (out == nullptr) return nullptr;
  snprintf(out, n, "%s%s", instance, path);
  return out;
}

void app_credentials_free(app_credentials *c) {
  free(c->client_id);
  free(c->client_secret);
  c->client_id = nullptr;
  c->client_secret = nullptr;
}

bool register_app(const char *instance, const char *redirect_uris,
                   app_credentials *out) {
  out->client_id = nullptr;
  out->client_secret = nullptr;

  char *url = join_url(instance, "/api/v1/apps");
  if (url == nullptr) return false;

  http_field fields[] = {
      {"client_name", CLI_CLIENT_NAME},
      {"redirect_uris", redirect_uris},
      {"scopes", CLI_SCOPES},
  };

  http_response resp = {0};
  bool ok = http_post_form(url, fields, 3, nullptr, &resp);
  free(url);
  if (!ok || resp.code < 200 || resp.code >= 300) {
    http_response_free(&resp);
    return false;
  }

  cJSON *root = json_parse(resp.body, resp.len);
  http_response_free(&resp);
  if (root == nullptr) return false;

  const char *cid = json_get_string(root, "client_id");
  const char *csec = json_get_string(root, "client_secret");
  if (cid == nullptr || csec == nullptr) {
    json_free(root);
    return false;
  }
  out->client_id = strdup(cid);
  out->client_secret = strdup(csec);
  json_free(root);
  return out->client_id != nullptr && out->client_secret != nullptr;
}

char *build_authorize_url(const char *instance, const char *client_id,
                          const char *redirect_uri, const char *scope,
                          const char *code_challenge, const char *state) {
  char *e_client = urlencode(client_id);
  char *e_redir = urlencode(redirect_uri);
  char *e_scope = urlencode(scope);
  char *e_cc = urlencode(code_challenge);
  char *e_state = urlencode(state);
  if (!e_client || !e_redir || !e_scope || !e_cc || !e_state) {
    free(e_client);
    free(e_redir);
    free(e_scope);
    free(e_cc);
    free(e_state);
    return nullptr;
  }

  size_t n = strlen(instance) + 256 + strlen(e_client) + strlen(e_redir) +
             strlen(e_scope) + strlen(e_cc) + strlen(e_state);
  char *out = malloc(n);
  if (out == nullptr) {
    free(e_client);
    free(e_redir);
    free(e_scope);
    free(e_cc);
    free(e_state);
    return nullptr;
  }

  snprintf(out, n,
           "%s/oauth/authorize?response_type=code&client_id=%s"
           "&redirect_uri=%s&scope=%s&code_challenge=%s"
           "&code_challenge_method=S256&state=%s",
           instance, e_client, e_redir, e_scope, e_cc, e_state);

  free(e_client);
  free(e_redir);
  free(e_scope);
  free(e_cc);
  free(e_state);
  return out;
}

bool exchange_token(const char *instance, const char *code,
                    const char *client_id, const char *client_secret,
                    const char *redirect_uri, const char *code_verifier,
                    char **access_token_out) {
  *access_token_out = nullptr;

  char *url = join_url(instance, "/oauth/token");
  if (url == nullptr) return false;

  http_field fields[] = {
      {"grant_type", "authorization_code"},
      {"code", code},
      {"client_id", client_id},
      {"client_secret", client_secret},
      {"redirect_uri", redirect_uri},
      {"code_verifier", code_verifier},
  };

  http_response resp = {0};
  bool ok = http_post_form(url, fields, 6, nullptr, &resp);
  free(url);
  if (!ok || resp.code < 200 || resp.code >= 300) {
    http_response_free(&resp);
    return false;
  }

  cJSON *root = json_parse(resp.body, resp.len);
  http_response_free(&resp);
  if (root == nullptr) return false;

  const char *tok = json_get_string(root, "access_token");
  if (tok == nullptr) {
    json_free(root);
    return false;
  }
  *access_token_out = strdup(tok);
  json_free(root);
  return *access_token_out != nullptr;
}

bool verify_credentials(const char *instance, const char *access_token,
                        char **username_out, char **account_id_out) {
  *username_out = nullptr;
  *account_id_out = nullptr;

  char *url = join_url(instance, "/api/v1/accounts/verify_credentials");
  if (url == nullptr) return false;

  http_response resp = {0};
  bool ok = http_get(url, access_token, &resp);
  free(url);
  if (!ok || resp.code < 200 || resp.code >= 300) {
    http_response_free(&resp);
    return false;
  }

  cJSON *root = json_parse(resp.body, resp.len);
  http_response_free(&resp);
  if (root == nullptr) return false;

  const char *u = json_get_string(root, "username");
  const char *id = json_get_string(root, "id");
  if (u == nullptr) {
    json_free(root);
    return false;
  }
  *username_out = strdup(u);
  if (id != nullptr) *account_id_out = strdup(id);
  json_free(root);
  return *username_out != nullptr;
}

static char *build_challenge(const char *verifier, char *method_out) {
  (void)method_out;
  uint8_t digest[32];
  sha256_once((const uint8_t *)verifier, strlen(verifier), digest);
  return base64url_encode(digest, 32);
}

static char *gen_state(void) {
  char *raw = random_verifier(16);
  return raw;
}

int login(const char *instance_arg, bool force_oob) {
  char instance[512];
  if (strncmp(instance_arg, "http://", 7) == 0 ||
      strncmp(instance_arg, "https://", 8) == 0) {
    snprintf(instance, sizeof(instance), "%s", instance_arg);
  } else {
    snprintf(instance, sizeof(instance), "https://%s", instance_arg);
  }

  char *verifier = random_verifier(32);
  char *challenge = build_challenge(verifier, nullptr);
  char *state = gen_state();
  if (verifier == nullptr || challenge == nullptr || state == nullptr) {
    free(verifier);
    free(challenge);
    free(state);
    return 3;
  }

  uint16_t port = 0;
  int lb_fd = -1;
  char *loopback_uri = nullptr;
  const char *redirect_uri;
  bool use_loopback = !force_oob && loopback_listen(&port, &lb_fd);
  if (use_loopback) {
    loopback_uri = loopback_redirect_uri(port);
    redirect_uri = loopback_uri;
  } else {
    redirect_uri = OOB_REDIRECT_URI;
  }

  char redirect_uris_buf[512];
  if (use_loopback) {
    snprintf(redirect_uris_buf, sizeof(redirect_uris_buf), "%s\n%s",
             loopback_uri, OOB_REDIRECT_URI);
  } else {
    snprintf(redirect_uris_buf, sizeof(redirect_uris_buf), "%s",
             OOB_REDIRECT_URI);
  }

  app_credentials app = {0};
  if (!register_app(instance, redirect_uris_buf, &app)) {
    fprintf(stderr, "error: failed to register app with %s\n", instance);
    if (use_loopback) close(lb_fd);
    free(loopback_uri);
    free(verifier);
    free(challenge);
    free(state);
    return 3;
  }

  char *auth_url = build_authorize_url(instance, app.client_id, redirect_uri,
                                       CLI_SCOPES, challenge, state);
  if (auth_url == nullptr) {
    if (use_loopback) close(lb_fd);
    free(loopback_uri);
    free(verifier);
    free(challenge);
    free(state);
    app_credentials_free(&app);
    return 3;
  }

  printf("Opening browser for authorization:\n%s\n", auth_url);
  bool opened = open_browser(auth_url);
  if (!opened) {
    printf("Could not open browser automatically. Open this URL manually:\n%s\n",
           auth_url);
  }
  free(auth_url);

  char *code = nullptr;
  if (use_loopback) {
    printf("Waiting for callback on http://127.0.0.1:%u/ ...\n", port);
    loopback_result r = {0};
    if (!loopback_accept_once(lb_fd, state, 300, &r)) {
      fprintf(stderr, "error: did not receive authorization callback\n");
      close(lb_fd);
      free(loopback_uri);
      free(verifier);
      free(challenge);
      free(state);
      app_credentials_free(&app);
      return 3;
    }
    close(lb_fd);
    code = r.code;
    if (r.error != nullptr) {
      fprintf(stderr, "error: authorization server returned: %s\n", r.error);
    }
    free(r.error);
  } else {
    printf("After authorizing, paste the displayed code here:\n> ");
    fflush(stdout);
    char buf[1024] = {0};
    if (fgets(buf, sizeof(buf), stdin) == nullptr) {
      free(loopback_uri);
      free(verifier);
      free(challenge);
      free(state);
      app_credentials_free(&app);
      return 1;
    }
    char *nl = strpbrk(buf, "\r\n");
    if (nl != nullptr) *nl = '\0';
    code = strdup(buf);
  }

  if (code == nullptr || code[0] == '\0') {
    fprintf(stderr, "error: no authorization code\n");
    free(code);
    free(loopback_uri);
    free(verifier);
    free(challenge);
    free(state);
    app_credentials_free(&app);
    return 1;
  }

  char *access_token = nullptr;
  if (!exchange_token(instance, code, app.client_id, app.client_secret,
                      redirect_uri, verifier, &access_token)) {
    fprintf(stderr, "error: token exchange failed\n");
    free(code);
    free(loopback_uri);
    free(verifier);
    free(challenge);
    free(state);
    app_credentials_free(&app);
    return 3;
  }
  free(code);
  free(loopback_uri);
  free(verifier);
  free(challenge);
  free(state);

  char *username = nullptr;
  char *account_id = nullptr;
  if (!verify_credentials(instance, access_token, &username, &account_id)) {
    fprintf(stderr, "error: verify_credentials failed (token may still work)\n");
    free(access_token);
    free(username);
    free(account_id);
    return 3;
  }

  config c;
  config_init(&c);
  c.instance = strdup(instance);
  c.client_id = strdup(app.client_id);
  c.client_secret = strdup(app.client_secret);
  c.access_token = access_token;
  c.account_id = account_id != nullptr ? account_id : strdup("");
  c.username = username;
  app_credentials_free(&app);
  if (!config_save(&c)) {
    fprintf(stderr, "error: failed to save config\n");
    config_free(&c);
    return 3;
  }
  config_free(&c);

  printf("Logged in as @%s@%s\n", username, instance_arg);
  return 0;
}
