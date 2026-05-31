# obs-wave-plugin threat model

## Scope

GPL-2.0 OBS Studio plugin loaded into the OBS process on broadcast operators'
machines. Exposes WAVE as a custom output destination so operators can hit
**Start Streaming** with WAVE selected and have encoded video/audio frames
pushed directly to `api.wave.online` via SRT.

## Trust boundaries

```
┌── operator's machine ──────────────────────────────────────────┐
│   ┌── OBS Studio process ──────────────────────────────────┐   │
│   │  libobs (GPL-2)                                        │   │
│   │     ├── scenes / sources / encoders (existing)         │   │
│   │     └── output plugins:                                │   │
│   │         · obs-output-ffmpeg (existing)                 │   │
│   │         · obs-x264 (existing)                          │   │
│   │         · wave-output (THIS PLUGIN, GPL-2 derivative)  │   │
│   │           ├── plugin-main.c   (module entry)           │   │
│   │           ├── wave-output.c   (output API impl)        │   │
│   │           └── wave-properties.c (settings UI)          │   │
│   └────────────────────────────┬───────────────────────────┘   │
│                                │ TLS + SRT (Bearer JWT)        │
└────────────────────────────────┼───────────────────────────────┘
                                 ▼
                   api.wave.online (Layer 1 Edge)
```

| Boundary | Trust direction | Defense |
|---|---|---|
| OBS UI → plugin properties | UI is in-process but operator-driven | Plugin `.parse()` (manual validation) every string field; URL must start `srt://api.wave.online`; reject otherwise |
| Plugin → network | plugin trusted (in OBS process), network untrusted | TLS-pinned SRT (Wave 2); JWT bearer in stream header; gateway issuer-claim check on every response |
| Plugin → OBS profile config | plugin trusted | Stream keys redacted in all `blog()` lines; never written to plugin-local files |
| Operator → stream key paste | operator trusted at paste time | Key never displayed in cleartext after first save; UI shows `••••••••` |

## Threat enumeration (STRIDE)

| Threat | Mitigation |
|---|---|
| **S** Spoofed gateway endpoint via DNS hijack | Hostname allowlist (`api.wave.online` only); TLS pinning (Wave 2 enhancement); JWT issuer-claim verification |
| **T** Tampered plugin binary | Code signing (Apple Developer ID / Authenticode) at release time; OBS verifies signature on load (Wave 2) |
| **R** Operator denies a stream session | All output starts/stops emit a structured log line + (Wave 2) Sentry breadcrumb tagged with stream id |
| **I** Stream key leaked to OBS log | Plugin redacts `Authorization:` and stream-key query params in every `blog()` call |
| **D** Malformed properties cause plugin crash → OBS crash | Every C string from properties is bounds-checked; reject empty / over-length / non-UTF8 |
| **E** Elevation via malformed gateway response | Response parser uses fixed-size buffers; protobuf decode (Wave 2) is `arena`-allocated; no `strcpy` |

## Out-of-scope (today)

- Physical access to the operator's machine — the OBS profile encrypts keys
  on supported platforms; the plugin cannot defend against root-level FS access.
- Side-channel attacks against `libobs` internals — that's the OBS Studio
  project's threat model (https://github.com/obsproject/obs-studio).
- DRM enforcement — WAVE does not DRM operator-owned streams.

## Process

- Threat model is reviewed at every major version bump.
- Plugin logs structured stream lifecycle events for traffic analysis.
- New output API surfaces (frame callbacks, settings fields) MUST update
  this doc in the same PR.
