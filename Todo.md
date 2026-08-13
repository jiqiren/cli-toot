# Todo — cli-toot

All v1 tasks complete. Current version: 1.0.3.

## Completed
- [x] Project scaffolding (meson.build, cjson.wrap, libcurl, cJSON, platform sha256, stub main.c)
- [x] Crypto + base64 helpers (sha256 shim, base64url, random verifier, NIST test vector)
- [x] HTTP layer (libcurl: post_form, get, post_json, urlencode)
- [x] JSON helpers (cJSON wrappers: parse, get_string, get_int)
- [x] Config storage (XDG path, load/save key=value, 0600 mode)
- [x] Browser launch (open / xdg-open)
- [x] Loopback HTTP server (127.0.0.1:0, state validation, callback capture)
- [x] OAuth flow (register_app, PKCE, authorize URL, token exchange, verify_credentials, login orchestrator)
- [x] CLI dispatch (login / toot / whoami / version / help, exit codes)
- [x] Toot posting (POST /api/v1/statuses, print URL)
- [x] Build & verification (clean -Werror c23 build, 25 tests pass, smoke tests)
- [x] Polish (README, .gitignore, Spec/AGENTS review)
- [x] GoToSocial compatibility (newline-joined redirect_uris)
- [x] Version flag (--version / -V / version)
- [x] Linux build fix (_GNU_SOURCE for POSIX functions)
- [x] Meson install support (install : true)
- [x] Homebrew tap (jiqiren/homebrew-tap, formula, bottles)
