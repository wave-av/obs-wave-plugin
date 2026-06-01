# obs-wave-plugin

**WAVE as a first-class OBS Studio output destination.** Hit **Start
Streaming** with WAVE selected and your encoded video/audio frames push
directly to `api.wave.online` via SRT — no relay, no re-stream, no second
RTMP server.

Layer 0 (Operator) of the [WAVE Protocol Plane][plane], plugin surface.

## What it does

Adds a new entry under OBS **Settings → Stream → Service**:

| Field | Meaning |
|---|---|
| **Service** | `WAVE` |
| **Gateway URL** | `srt://api.wave.online` (default; editable for self-hosted gateways) |
| **Stream key** | Per-event key from <https://wave.online/console> |
| **Codec** | H.264 / HEVC / AV1 / AV2 (AV2 = Wave 2) |

When the operator hits **Start Streaming**, this plugin:

1. Exchanges the stream key for a short-lived gateway JWT.
2. Opens a single SRT connection to the gateway with the JWT in the stream
   id header.
3. Pushes every encoded video + audio packet OBS gives it, in real time, to
   that connection.

No re-encoding, no muxing through a second server. Lowest possible latency
from the operator's encoder to the WAVE edge.

## Install

| OS | Path |
|---|---|
| macOS | `~/Library/Application Support/obs-studio/plugins/wave-output.plugin/` |
| Windows | `%APPDATA%\obs-studio\plugins\wave-output\bin\64bit\wave-output.dll` |
| Linux | `~/.config/obs-studio/plugins/wave-output/bin/64bit/wave-output.so` |

Drop the built artifact into the path above and restart OBS. The **WAVE**
service appears under **Settings → Stream → Service**.

## Build

You need an installed `libobs` — see the [OBS plugin template wiki][tpl]
for the recommended toolchain per platform.

```sh
# macOS
cmake --preset macos
cmake --build --preset macos
# → build_macos/wave-output.plugin/

# Linux
cmake --preset linux-x86_64
cmake --build --preset linux-x86_64

# Windows (from a Developer PowerShell)
cmake --preset windows-x64
cmake --build --preset windows-x64 --config Release
```

If CMake can't find `libobs`, set `-DLIBOBS_PATH=/path/to/libobs/install`.

## License

**GPL-2.0-or-later.** This plugin links `libobs`, which is GPL-2; under the
GPL's strong-copyleft terms a derivative work must inherit the same
license. See [`LICENSE`](./LICENSE).

## License boundary

This repo ships **no vendor-licensed binaries** and **no upstream headers**.
`libobs` and `libsrt` are resolved at build time on each contributor's
machine via CMake / package managers. See [`CONTRIBUTING.md`](./CONTRIBUTING.md)
and [`SECRETS.md`](./SECRETS.md).

## Roadmap

| Wave | Surface | Status |
|---|---|---|
| **W1** | Skeleton + module entry + properties UI + foundation chassis | this PR |
| W2 | Real SRT push + gateway JWT bearer + protobuf metadata | task [#163][t163] |
| W3 | AV2 codec support + auto-bitrate from gateway hints | tracked under [`wave-on-prem-layer`][on-prem] |

## Related

- [`wave-desktop`](https://github.com/wave-av/wave-desktop) — Operator
  Console (sibling Layer-0 surface; built-in encoders, receivers, multiview)
- [`wave-foundation`](https://github.com/wave-av/wave-foundation) (private)
  — chassis the foundation gate mirrors
- [`vmix-wave-bridge`](https://github.com/wave-av/vmix-wave-bridge) — sibling
  plugin for vMix (task [#164][t164])

## Security

Open a private GitHub Security Advisory at
<https://github.com/wave-av/obs-wave-plugin/security/advisories/new>. Do
NOT open a public issue.

[plane]: https://github.com/wave-av/wave-foundation (private)
[tpl]: https://github.com/obsproject/obs-plugintemplate/wiki
[t163]: https://github.com/wave-av/wave-foundation/issues/163
[t164]: https://github.com/wave-av/wave-foundation/issues/164
[on-prem]: https://github.com/wave-av/wave-foundation (private — plans/wave-on-prem-layer)
