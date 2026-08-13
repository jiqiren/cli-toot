# cli-toot

A minimal command-line Mastodon client for sending quick toots, written in pure C23. Targets macOS and Linux.

## What it does

- `cli-toot login <instance>` — run the Mastodon OAuth login dance and store a Bearer token.
- `cli-toot toot "<text>"` — post a new public status.
- `cli-toot whoami` — show the logged-in account handle.
- `cli-toot help` — print usage.

The app registers itself with Mastodon as `cli ToooT` (exact spelling intentional).

## Login flow

`login` uses PKCE (`S256`) with a loopback HTTP server by default:

1. Binds a local server on `127.0.0.1:<ephemeral port>/callback`.
2. Registers an app with the instance, listing the exact bound callback URI (plus `urn:ietf:wg:oauth:2.0:oob` as a fallback).
3. Generates a PKCE `code_verifier` (32 bytes from `/dev/urandom`, base64url-encoded) and `code_challenge = base64url(SHA-256(verifier))`.
4. Opens your browser to the instance's `/oauth/authorize` endpoint with the challenge and a random `state`.
5. Waits for the instance to redirect back with `?code=...&state=...`, validates `state`, exchanges the code for an access token, and verifies via `/api/v1/accounts/verify_credentials`.
6. Saves credentials to `~/.config/cli-toot/config` (mode 0600).

If the loopback server can't bind, it falls back to the OOB flow: the instance displays a code on a page and you paste it into the CLI.

## Build

Requires Meson, pkg-config, libcurl, and (on Linux) libcrypto/openssl dev headers. cJSON is fetched automatically via a Meson subproject wrap.

### macOS (Homebrew)

```sh
brew install meson pkg-config curl
meson setup build && meson compile -C build
```

### Linux (Debian/Ubuntu)

```sh
sudo apt install meson pkg-config libcurl4-openssl-dev libssl-dev
meson setup build && meson compile -C build
```

The build uses `c_std=c23`, `warning_level=2`, and `werror=true` — it must compile cleanly under `-Wall -Wextra -Werror`.

## Use

```sh
./build/cli-toot login fosstodon.org
# browser opens → authorize → "Logged in as @you@fosstodon.org"

./build/cli-toot whoami
# @you@fosstodon.org

./build/cli-toot toot "hello from cli-toot"
# https://fosstodon.org/@you/...
```

### Config

Credentials live at `$XDG_CONFIG_HOME/cli-toot/config` (or `~/.config/cli-toot/config` if `XDG_CONFIG_HOME` is unset), written with mode `0600`. The file is a flat `key=value` list of `instance`, `client_id`, `client_secret`, `access_token`, `account_id`, `username`.

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
│   ├── main.c            CLI dispatch
│   ├── oauth.{c,h}       app registration, PKCE, token exchange, login
│   ├── toot.{c,h}        post a status
│   ├── http.{c,h}        libcurl helpers (form/json/get, urlencode)
│   ├── loopback.{c,h}    127.0.0.1 callback server
│   ├── config.{c,h}      XDG config load/save (0600)
│   ├── browser.{c,h}    open / xdg-open
│   ├── base64.{c,h}     base64url + PKCE verifier
│   ├── json_helpers.{c,h}  cJSON wrappers
│   └── sha256.{h,apple.c,posix.c}  per-platform SHA-256 shim
├── tests/
│   ├── crypto_test.c
│   ├── config_test.c
│   └── loopback_test.c
├── Spec.md
├── AGENTS.md
└── Todo.md
```

## Running tests

```sh
meson test -C build
```

## Dependencies

- [libcurl](https://curl.se/) — HTTP/TLS, via pkg-config.
- [cJSON](https://github.com/DaveGamble/cJSON) — JSON, via Meson wrap (auto-fetched).
- SHA-256: CommonCrypto on macOS, libcrypto (`SHA256()`) on Linux. No vendored crypto.
- Random bytes: `/dev/urandom`.
