# Todo — cli-toot implementation

Track v1 deliverables. Mark items off as they are completed and verified.

## 1. Project scaffolding
- [ ] 1.1 Write `meson.build` (project, c_std=c23, warning_level=2, werror=true).
- [ ] 1.2 Add `subprojects/cjson.wrap` pointing at WrapDB cJSON.
- [ ] 1.3 Wire libcurl via `dependency('libcurl')`.
- [ ] 1.4 Wire cJSON via `subproject('cjson')`.
- [ ] 1.5 Platform source selection for `sha256_apple.c` (darwin) / `sha256_posix.c` (else).
- [ ] 1.6 Add `dependency('libcrypto')` only on non-darwin.
- [ ] 1.7 Empty-stub `src/main.c` so `meson setup build && meson compile -C build` succeeds end-to-end.

## 2. Crypto + base64 helpers
- [ ] 2.1 `src/sha256.h` declaring `void sha256_once(const uint8_t *data, size_t len, uint8_t out[32])`.
- [ ] 2.2 `src/sha256_apple.c` implementing via `CC_SHA256`.
- [ ] 2.3 `src/sha256_posix.c` implementing via `SHA256()` (openssl/sha.h).
- [ ] 2.4 `src/base64.h/.c` — `base64url_encode(const uint8_t*, size_t)` (no padding).
- [ ] 2.5 `src/base64.c` — `random_verifier()`: read 32 bytes from `/dev/urandom`, base64url → 43-char verifier.
- [ ] 2.6 Smoke test the digest against a NIST vector (`"abc" → ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`).

## 3. HTTP layer (libcurl)
- [ ] 3.1 `src/http.h/.c` — `http_post_form(url, form_fields[], bearer, &resp)` returning long http_code; fills response body buffer.
- [ ] 3.2 `http_get(url, bearer, &resp)` variant.
- [ ] 3.3 `http_post_json(url, json_body, bearer, &resp)` variant (used later for statuses if needed).
- [ ] 3.4 Error path: non-2xx returns 3, body preserved for caller to print.
- [ ] 3.5 curl global init / cleanup managed in `main.c`.

## 4. JSON helpers (cJSON)
- [ ] 4.1 `src/json_helpers.h/.c` — `json_get_string(root, key)` returning `const char*` or nullptr.
- [ ] 4.2 `json_get_int(root, key)`.
- [ ] 4.3 Parse helper: `json_parse(const char *body, size_t len)` returning owned `cJSON*` (must free).
- [ ] 4.4 Wrap cJSON free in a small RAII-ish helper or document caller responsibility.

## 5. Config storage
- [ ] 5.1 `src/config.h/.c` — `config_path()` honoring `XDG_CONFIG_HOME` then `$HOME/.config`.
- [ ] 5.2 `config_load(struct config*)` parsing flat `key=value`.
- [ ] 5.3 `config_save(const struct config*)` writing with mode 0600 (`open` + `fchmod`).
- [ ] 5.4 `config_get(key)` accessor + getters for `instance`, `client_id`, `client_secret`, `access_token`, `account_id`, `username`.
- [ ] 5.5 Never print `client_secret` / `access_token` in normal output.

## 6. Browser launch
- [ ] 6.1 `src/browser.h/.c` — `open_browser(const char *url)`.
- [ ] 6.2 macOS: `open`. Linux: `xdg-open`. Fail gracefully (return nonzero) if missing.

## 7. Loopback HTTP server
- [ ] 7.1 `src/loopback.h/.c` — bind `127.0.0.1:0`, return chosen port + `redirect_uri`.
- [ ] 7.2 Serve one request, parse `?code=` and `&state=`, validate state, respond with HTML "you can close this", shutdown.
- [ ] 7.3 Timeout: if no request within ~5 minutes, abort with error.
- [ ] 7.4 Return code via out-param; thread-safe enough for single-shot use.

## 8. OAuth flow (oauth.c)
- [ ] 8.1 `src/oauth.h/.c` — `register_app(instance)` → fills `client_id`, `client_secret`.
- [ ] 8.2 `build_authorize_url(...)` constructing `/oauth/authorize` query with PKCE + state.
- [ ] 8.3 `exchange_token(...)` → `POST /oauth/token` returning `access_token`.
- [ ] 8.4 `verify_credentials(token, instance)` → fills username + account_id.
- [ ] 8.5 `login(instance)` orchestrator: register → PKCE gen → loopback (or OOB fallback) → open browser → capture → exchange → verify → save config.

## 9. CLI dispatch (main.c)
- [ ] 9.1 `main.c` arg parsing: `login <instance>`, `toot <text>`, `whoami`, `help`.
- [ ] 9.2 `login` command — call `oauth.c::login`.
- [ ] 9.3 `toot` command — call `toot.c::post_status`.
- [ ] 9.4 `whoami` command — call `oauth.c::verify_credentials` and print handle.
- [ ] 9.5 `help` / bare invocation — usage text, exit 1.
- [ ] 9.6 Exit codes: 0 ok, 1 usage, 2 not-logged-in, 3 network/HTTP error.

## 10. Toot posting
- [ ] 10.1 `src/toot.h/.c` — `post_status(instance, token, text)` → `POST /api/v1/statuses`.
- [ ] 10.2 URL-encode status text.
- [ ] 10.3 On success, parse `Status.url` and print it.
- [ ] 10.4 On non-2xx, print error + body, exit 3.

## 11. Build & verification
- [ ] 11.1 `meson setup build && meson compile -C build` clean under `-Werror c_std=c23`.
- [ ] 11.2 `./build/cli-toot help` prints usage.
- [ ] 11.3 `./build/cli-toot login fosstodon.org` end-to-end: browser opens, token stored, `whoami` prints handle.
- [ ] 11.4 `./build/cli-toot toot "hello from cli-toot"` — post visible on instance.
- [ ] 11.5 Force OOB fallback (loopback bind failure) — paste-code flow works.
- [ ] 11.6 Linux smoke: `xdg-open`, `/dev/urandom`, libcrypto link all functional.

## 12. Polish
- [ ] 12.1 `README.md` with install + usage (only if requested).
- [ ] 12.2 `.gitignore` for `build/` and `subprojects/cjson*/`.
- [ ] 12.3 Final review pass against `Spec.md` + `AGENTS.md`.
