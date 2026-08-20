# AGENTS.md — guidance for opencode agents working on sloptoot

## Project
sloptoot: a minimal C23 CLI Mastodon client. See `Spec.md` for the full specification before doing anything.

## Toolchain
- Build: **Meson**. Do not switch to CMake/Make.
- Language: **C23** (`c_std=c23`). Use C23 features freely (`constexpr`, `nullptr`, `bool`, `auto` where readable, `[[nodiscard]]`/`[[noreturn]]`).
- Compiler: Clang (macOS default) and GCC (Linux). Code must compile clean under `-Wall -Wextra -Werror` (set via `warning_level=2, werror=true` in `meson.build`).

## Dependencies
- **libcurl**: hard system dependency, linked via pkg-config. Always use it for HTTP — do not hand-roll sockets for API calls.
- **cJSON**: via Meson subproject wrap (`subprojects/cjson.wrap`). **Do not vendor** cJSON source. Access via `dependency('libcjson', fallback : ['cjson', 'libcjson_dep'])` — this prefers a system-installed cjson (pkg-config) and falls back to the subproject only when not found. Pass `--wrap-mode=nofallback` to force system cjson (used by the Homebrew formula).
- **SQLite**: used for the status cache (`src/cache.c`). Link `dependency('sqlite3')` via pkg-config — the Homebrew `sqlite` keg ships `sqlite3.pc`; Linux distros ship `libsqlite3-dev`. Never store `access_token`/`client_secret` in the DB.
- **SHA-256**: per-platform shim only.
  - macOS: CommonCrypto (`CC_SHA256`).
  - Linux: libcrypto (`SHA256()`, `dependency('libcrypto')`).
  - Do **not** vendor any SHA-256 implementation and do **not** use wolfSSL/LibreSSL/OpenSSL as a second TLS stack.
- **Random bytes**: `/dev/urandom`. Do not use `rand()`/`random()` for PKCE.
- **Feature macro (Linux)**: `_GNU_SOURCE` is added to `c_args` on non-darwin to expose POSIX functions (`fchmod`, `fdopen`) that glibc hides by default. Do not define `_POSIX_C_SOURCE` — on macOS it hides BSD symbols like `INADDR_LOOPBACK` and `mkdtemp`.

## File layout convention
- One concern per `.c`/`.h` pair (see `Spec.md` layout).
- Headers guard with `#pragma once`.
- Comments explaining how code works are welcome. Do not leave commented-out code or noise.

## Meson rules
- Source files per platform are selected via `host_machine.system()`: `'darwin'` → `sha256_apple.c`, else `sha256_posix.c`.
- Only add `dependency('libcrypto')` on non-darwin.
- `configure_file` generates `src/version.h` from `src/version.h.in` with the project version. Include `version.h` (not `version.h.in`) in source.
- `executable(..., install : true)` so `meson install` places the binary in the prefix `bin/`.
- Keep `default_options` as in Spec; do not lower warning level or remove `werror=true`.

## Config / credentials
- Config path resolution must honor `XDG_CONFIG_HOME`, falling back to `$HOME/.config`.
- Always write config with mode 0600. Never print secrets (`access_token`, `client_secret`) in normal output.

## OAuth / network
- `client_name` sent to Mastodon is **`slop TooT`** (exact casing/spelling — do not change it).
- Default flow: PKCE (`S256`) + loopback server on `127.0.0.1:0`. OOB is a fallback only.
- All API calls go over HTTPS via libcurl; always send `Authorization: Bearer <token>` for non-public endpoints.
- `redirect_uris` must join multiple URIs with a **newline** (`\n`), not a space. GoToSocial splits on newline only; Mastodon accepts both. Newline works for both implementations.
- The loopback server must be bound **before** `register_app` so the registered `redirect_uri` exactly matches the bound port. Mastodon and GoToSocial both require exact-match at authorize time.
- The loopback server prints the raw callback request to stderr when `SLOPTOOT_DEBUG=1` is set — useful for diagnosing OAuth errors.

## Build & verify commands (run before declaring done)
```sh
meson setup build && meson compile -C build
```
Must compile cleanly. If it doesn't, fix before stopping.

To run a built binary: `./build/sloptoot <args>`.
To run tests: `meson test -C build`.

## Git
- Do not commit unless the user explicitly asks.
- Do not update git config, force-push, or amend.
- Commit messages: concise, conventional-style (e.g. `feat: add login flow`, `fix: handle OOB fallback`).

## Versioning
- The project version lives in `meson.build` (`version : '...'`).
- Every bugfix commit must increment the patch level (e.g. `1.0.0` → `1.0.1`). New features increment minor (`1.0.1` → `1.1.0`); breaking changes increment major.
- Tag releases as `v<version>` (e.g. `v1.0.3`) and create a GitHub release. The Homebrew formula at `jiqiren/homebrew-tap` points at the release archive.

## Distribution (Homebrew)
- The tap lives at `jiqiren/homebrew-tap` (https://github.com/jiqiren/homebrew-tap).
- Users install with `brew install jiqiren/tap/sloptoot`.
- The formula (`Formula/sloptoot.rb`) builds from the tagged release tarball, depends on `cjson` (system), and passes `--wrap-mode=nofallback` to `meson setup`.
- When releasing a new version: bump `meson.build` version, commit, tag `v<version>`, push the tag, create a GitHub release, compute the tarball SHA-256, and update the formula's `url`/`sha256`/`version` in the tap.

## Things to NOT do
- Do not add streaming/notification features or media uploads — out of scope.
- Do not introduce a second TLS library.
- Do not vendor JSON or crypto libs.
- Do not leave commented-out code or noise in source files.
