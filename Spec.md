# cli-toot — Specification

A minimal command-line Mastodon client for sending quick toots, written in pure C23.

## Goals
- Fast, single-binary CLI for posting toots and authenticating with Mastodon.
- Cross-platform: macOS and Linux.
- Homebrew-friendly build system.
- Minimal external dependencies.

## Non-goals (v1)
- Streaming, timelines, notifications, media uploads.
- Multiple account profiles (single active account only).
- Windows support.

## Stack
- **Language:** C23 (`-std=c23`), Clang/GCC.
- **Build system:** Meson (accepted by the brew project). `meson.build` at repo root.
- **HTTP/TLS:** libcurl — hard system dependency, linked via pkg-config.
- **JSON:** cJSON via a Meson subproject wrap (`subprojects/cjson.wrap`). Not vendored.
- **SHA-256 (PKCE):** per-platform shim, no vendored crypto:
  - macOS: CommonCrypto `<CommonCrypto/CommonDigest.h>` (`CC_SHA256`). System framework, no link flag.
  - Linux: `<openssl/sha.h>` (`SHA256()`), linked against `-lcrypto` via pkg-config (`dependency('libcrypto')`).
- **Random bytes:** read 32 bytes from `/dev/urandom` (present on both macOS and Linux).

## App identity
- `client_name` sent to Mastodon during app registration: **`cli ToooT`** (exact casing/spelling).
- `redirect_uris`: registered dynamically to match the bound loopback port (see login flow); OOB URI is always included as a fallback.
- `scopes`: `read write`.

## Commands

### `cli-toot login <instance>`
Performs the full Mastodon OAuth login dance and persists credentials.

Flow (default PKCE + loopback, OOB fallback):
1. Validate `instance` arg (required). Normalize to `https://<instance>`.
2. **Bind loopback HTTP server** on `127.0.0.1:0` (ephemeral port) first. `redirect_uri = http://127.0.0.1:<port>/callback`.
   - If bind fails → fall back to OOB: `redirect_uri = urn:ietf:wg:oauth:2.0:oob`.
3. **Register app** — `POST /api/v1/apps` with `client_name="cli ToooT"`, `redirect_uris` set to the exact bound `redirect_uri` plus `urn:ietf:wg:oauth:2.0:oob` (OOB only, on fallback), `scopes="read write"`. Parse `client_id`, `client_secret`. (Binding first lets us register the exact URI Mastodon will match at authorize time.)
4. **Generate PKCE:**
   - `code_verifier` = base64url(32 random bytes from `/dev/urandom`) → 43 chars.
   - `code_challenge` = base64url(SHA-256(verifier)), `code_challenge_method = S256`.
5. **Open browser** to `GET /oauth/authorize?response_type=code&client_id=...&redirect_uri=<enc>&scope=read+write&code_challenge=...&code_challenge_method=S256&state=<rand>`.
   - macOS: `open <url>`. Linux: `xdg-open <url>`.
6. **Capture code:** loopback server accepts exactly one request, validates `state`, extracts `code`, responds with a small HTML "you can close this" page, then shuts down. (OOB fallback: read code from stdin.)
7. **Exchange token** — `POST /oauth/token` form: `grant_type=authorization_code`, `code`, `client_id`, `client_secret`, `redirect_uri`, `code_verifier`. Parse `access_token`.
8. **Verify** — `GET /api/v1/accounts/verify_credentials` with `Authorization: Bearer <token>`. Print `@username@instance`.
9. **Save** `instance`, `client_id`, `client_secret`, `access_token`, `account_id`, `username` to config (mode 0600). Print success.

### `cli-toot toot "<text>"`
1. `config_load()` — require `instance` + `access_token` (exit 2 if not logged in).
2. URL-encode status text. `POST /api/v1/statuses` form `status=<enc>&visibility=public` with Bearer header.
3. Parse `Status` entity; print posted `url` field. On non-2xx, print error + response body.

### `cli-toot whoami`
`GET /api/v1/accounts/verify_credentials`; print `@username@instance`. Cheap token-validation helper.

### `cli-toot help` / bare invocation
Print usage.

## Config storage
- Path: `$XDG_CONFIG_HOME/cli-toot/config` or `$HOME/.config/cli-toot/config`.
- Format: flat `key=value`, one per line.
- Fields: `instance`, `client_id`, `client_secret`, `access_token`, `account_id`, `username`.
- Created with mode 0600 (`open(O_CREAT, 0600)` + `fchmod`).

## Exit codes
- `0` success
- `1` usage error
- `2` not logged in
- `3` network / HTTP error

## Project layout
```
cli-toot/
├── meson.build
├── subprojects/cjson.wrap
├── src/
│   ├── main.c
│   ├── config.h / config.c
│   ├── http.h / http.c
│   ├── json_helpers.h / json_helpers.c
│   ├── oauth.h / oauth.c
│   ├── loopback.h / loopback.c
│   ├── browser.h / browser.c
│   ├── sha256.h
│   ├── sha256_apple.c
│   ├── sha256_posix.c
│   ├── base64.h / base64.c
│   └── toot.h / toot.c
├── Spec.md
├── AGENTS.md
├── Todo.md
└── README.md
```

## Build dependencies
- macOS: `brew install meson pkg-config curl` (cJSON auto-fetched via wrap).
- Linux: install `meson`, `pkg-config`, `libcurl4-openssl-dev` (or distro equivalent), `libssl-dev` (provides `libcrypto`).

## Meson build essentials
- `project('cli-toot','c',version:'0.1.0',default_options:['c_std=c23','warning_level=2','werror=true'])`
- `dependency('libcurl')` via pkg-config.
- `cjson = subproject('cjson').get_variable('cjson_dep')`.
- Linux only: `dependency('libcrypto')`.
- Per-platform source selection: `sha256_apple.c` on `'darwin'`, else `sha256_posix.c`.
- `executable('cli-toot', ...)`.

## Verification
- `meson setup build && meson compile -C build` clean under `-Werror c_std=c23`.
- `./build/cli-toot login fosstodon.org` → browser opens, token stored, `whoami` works.
- `./build/cli-toot toot "hello"` → post visible.
- Force OOB fallback (loopback bind failure); confirm paste flow works.
- Smoke test on Linux: `xdg-open`, `/dev/urandom`, libcrypto link.
