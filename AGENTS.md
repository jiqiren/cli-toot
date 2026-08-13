# AGENTS.md — guidance for opencode agents working on cli-toot

## Project
cli-toot: a minimal C23 CLI Mastodon client. See `Spec.md` for the full specification before doing anything.

## Toolchain
- Build: **Meson**. Do not switch to CMake/Make.
- Language: **C23** (`c_std=c23`). Use C23 features freely (`constexpr`, `nullptr`, `bool`, `auto` where readable, `[[nodiscard]]`/`[[noreturn]]`).
- Compiler: Clang (macOS default) and GCC (Linux). Code must compile clean under `-Wall -Wextra -Werror` (set via `warning_level=2, werror=true` in `meson.build`).

## Dependencies
- **libcurl**: hard system dependency, linked via pkg-config. Always use it for HTTP — do not hand-roll sockets for API calls.
- **cJSON**: via Meson subproject wrap (`subprojects/cjson.wrap`). **Do not vendor** cJSON source. Access through `subproject('cjson')`.
- **SHA-256**: per-platform shim only.
  - macOS: CommonCrypto (`CC_SHA256`).
  - Linux: libcrypto (`SHA256()`, `dependency('libcrypto')`).
  - Do **not** vendor any SHA-256 implementation and do **not** use wolfSSL/LibreSSL/OpenSSL as a second TLS stack.
- **Random bytes**: `/dev/urandom`. Do not use `rand()`/`random()` for PKCE.

## File layout convention
- One concern per `.c`/`.h` pair (see `Spec.md` layout).
- Headers guard with `#pragma once`.
- No comments in code unless explicitly requested.

## Meson rules
- Source files per platform are selected via `host_machine.system()`: `'darwin'` → `sha256_apple.c`, else `sha256_posix.c`.
- Only add `dependency('libcrypto')` on non-darwin.
- Keep `default_options` as in Spec; do not lower warning level or remove `werror=true`.

## Config / credentials
- Config path resolution must honor `XDG_CONFIG_HOME`, falling back to `$HOME/.config`.
- Always write config with mode 0600. Never print secrets (`access_token`, `client_secret`) in normal output.

## OAuth / network
- `client_name` sent to Mastodon is **`cli ToooT`** (exact spelling — do not "fix" the typo).
- Default flow: PKCE (`S256`) + loopback server on `127.0.0.1:0`. OOB is a fallback only.
- All API calls go over HTTPS via libcurl; always send `Authorization: Bearer <token>` for non-public endpoints.

## Build & verify commands (run before declaring done)
```sh
meson setup build && meson compile -C build
```
Must compile cleanly. If it doesn't, fix before stopping.

To run a built binary: `./build/cli-toot <args>`.

## Git
- Do not commit unless the user explicitly asks.
- Do not update git config, force-push, or amend.
- Commit messages: concise, conventional-style (e.g. `feat: add login flow`, `fix: handle OOB fallback`).

## Things to NOT do
- Do not add streaming/timeline/notification features — out of scope for v1.
- Do not introduce a second TLS library.
- Do not vendor JSON or crypto libs.
- Do not add comments to source files.
- Do not create a README unless asked.
