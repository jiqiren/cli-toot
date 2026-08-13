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
- **JSON:** cJSON via a Meson subproject wrap (`subprojects/cjson.wrap`). Not vendored. At build time, `dependency('libcjson', fallback : ['cjson', 'libcjson_dep'])` tries the system-installed cjson first (via pkg-config), falling back to the subproject only when not found. This lets Homebrew use its brewed `cjson` formula (`--wrap-mode=nofallback`) while local dev builds auto-fetch the wrap.
- **SHA-256 (PKCE):** per-platform shim, no vendored crypto:
  - macOS: CommonCrypto `<CommonCrypto/CommonDigest.h>` (`CC_SHA256`). System framework, no link flag.
  - Linux: `<openssl/sha.h>` (`SHA256()`), linked against `-lcrypto` via pkg-config (`dependency('libcrypto')`).
- **Random bytes:** read 32 bytes from `/dev/urandom` (present on both macOS and Linux).
- **Feature macro (Linux):** `_GNU_SOURCE` is defined via `c_args` on non-darwin to expose POSIX functions (`fchmod`, `fdopen`) that glibc hides by default. macOS needs no such macro (defining `_POSIX_C_SOURCE` would hide BSD symbols like `INADDR_LOOPBACK` and `mkdtemp`).

## App identity
- `client_name` sent to Mastodon during app registration: **`cli ToooT`** (exact casing/spelling).
- `redirect_uris`: registered dynamically to match the bound loopback port (see login flow); OOB URI is always included as a fallback. Multiple URIs are joined with a **newline** (`\n`), not a space — GoToSocial splits `redirect_uris` on newline only, while Mastodon accepts both. Newline works for both implementations.
- `scopes`: `read write`.

## Distribution
- **Homebrew tap:** `jiqiren/homebrew-tap` (https://github.com/jiqiren/homebrew-tap).
- Install with `brew install jiqiren/tap/cli-toot`.
- The formula builds from a tagged GitHub release archive (`https://github.com/jiqiren/cli-toot/archive/refs/tags/v<version>.tar.gz`), verified with SHA-256.
- Formula dependencies: `meson`, `ninja`, `pkgconf` (build); `curl`, `cjson` (runtime); `openssl@3` (runtime, Linux only).
- The formula passes `meson setup` with `--wrap-mode=nofallback` so the system `cjson` is used instead of building the subproject.

## Commands

### `cli-toot login <instance>`
Performs the full Mastodon OAuth login dance and persists credentials.

Flow (default PKCE + loopback, OOB fallback):
1. Validate `instance` arg (required). Normalize to `https://<instance>`.
2. **Bind loopback HTTP server** on `127.0.0.1:0` (ephemeral port) first. `redirect_uri = http://127.0.0.1:<port>/callback`.
   - If bind fails → fall back to OOB: `redirect_uri = urn:ietf:wg:oauth:2.0:oob`.
3. **Register app** — `POST /api/v1/apps` with `client_name="cli ToooT"`, `redirect_uris` set to the exact bound `redirect_uri` plus `urn:ietf:wg:oauth:2.0:oob` (joined with `\n`; OOB only, on fallback), `scopes="read write"`. Parse `client_id`, `client_secret`. (Binding first lets us register the exact URI Mastodon will match at authorize time.)
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

### `cli-toot version` / `--version` / `-V`
Print `cli-toot <version>`. The version is configured by Meson from `meson.build` into a generated `src/version.h` (via `src/version.h.in`).

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
│   ├── version.h.in        (template; Meson generates version.h)
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
├── tests/
│   ├── crypto_test.c
│   ├── config_test.c
│   └── loopback_test.c
├── Spec.md
├── AGENTS.md
├── README.md
└── LICENSE
```

## Build dependencies
- macOS: `brew install meson pkg-config curl` (cJSON auto-fetched via wrap, or use `brew install cjson` to use the system one).
- Linux: install `meson`, `pkg-config`, `libcurl4-openssl-dev` (or distro equivalent), `libssl-dev` (provides `libcrypto`).

## Meson build essentials
- `project('cli-toot','c',version:'<version>',default_options:['c_std=c23','warning_level=2','werror=true'])`.
- `dependency('libcurl')` via pkg-config.
- `dependency('libcjson', fallback : ['cjson', 'libcjson_dep'])` — uses system cjson if available, else builds the subproject.
- `configure_file(input : 'src/version.h.in', output : 'version.h', configuration : ...)` injects the project version.
- `_GNU_SOURCE` added to `c_args` on non-darwin for POSIX function visibility.
- Linux only: `dependency('libcrypto')`.
- Per-platform source selection: `sha256_apple.c` on `'darwin'`, else `sha256_posix.c`.
- `executable('cli-toot', ..., install : true)` — `meson install` places the binary in the prefix `bin/`.
- Tests: `crypto_test`, `config_test`, `loopback_test` (run via `meson test`).

## Verification
- `meson setup build && meson compile -C build` clean under `-Werror c_std=c23`.
- `meson test -C build` — all tests pass (crypto NIST vector + verifier length, config round-trip + 0600 mode, loopback end-to-end).
- `./build/cli-toot version` prints `cli-toot <version>`.
- `./build/cli-toot login <instance>` → browser opens, token stored, `whoami` works. Tested with Mastodon and GoToSocial.
- `./build/cli-toot toot "hello"` → post visible.
- Force OOB fallback (loopback bind failure); confirm paste flow works.
- Smoke test on Linux: `xdg-open`, `/dev/urandom`, libcrypto link, `_GNU_SOURCE` visibility.
- Homebrew formula: `brew install jiqiren/tap/cli-toot` builds and installs; `brew test` and `brew audit --new --formula` pass.
