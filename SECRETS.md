# obs-wave-plugin secrets

> **No secret of any kind belongs in this repo. None.** This file documents
> where each secret lives so contributors don't have to guess.

## Runtime (per operator install)

The plugin **never bundles** operator-facing secrets. They are entered by the
operator in the OBS **Settings → Stream → Service: WAVE** panel and persisted
inside OBS's own profile config (which OBS encrypts on supported platforms).

| Secret | Where it lives | Set how |
|---|---|---|
| Stream key (per-operator, per-event) | OBS profile config (`basic.ini` per profile) | Operator pastes from https://wave.online/console |
| Gateway JWT (short-lived, ~1h) | Plugin runtime memory only | Exchanged from stream key at `Start Streaming` |
| OAuth refresh token (Wave 2) | OBS profile config | Issued at sign-in, refreshed on expiry |

Never log, echo, or send these to any external service other than the WAVE
gateway (`https://api.wave.online`). The plugin must redact stream keys
and bearer tokens in every `blog()` log line.

## Build / release (CI only — never in source)

| Secret | Where | Purpose |
|---|---|---|
| `SENTRY_AUTH_TOKEN` | GitHub Actions repo secret | Symbol upload (Wave 2) |
| `APPLE_SIGNING_IDENTITY` / `APPLE_NOTARIZATION_*` | GitHub Actions org secret | macOS dylib signing (Wave 2) |
| `WINDOWS_SIGNING_CERT` / `WINDOWS_SIGNING_PASSWORD` | GitHub Actions org secret | Windows DLL signing (Wave 2) |

## Public-facing config (OK to ship)

| Value | Why public is fine |
|---|---|
| OAuth client ID | Public by OAuth design (must be embedded in the client) |
| Gateway base URL | Production endpoint is `https://api.wave.online` |
| Sentry DSN (Wave 2) | DSNs are designed to be public; abuse is rate-limited by Sentry |

## Licensed binaries (NEVER vendored)

`libobs` headers and `libsrt` shared libraries are **not vendored** — they
are resolved at build time:

- `libobs` via the OBS plugin-template CMake helpers (`find_package(libobs CONFIG REQUIRED)`)
- `libsrt` via the system package manager (homebrew / vcpkg / apt)

If you accidentally `git add` any of these, the foundation gate's
vendor-binary deny-list (`libobs*`, `libsrt*`, NDI binaries) will refuse to
merge. See `CONTRIBUTING.md` § "License boundary" and `.gitignore`.
