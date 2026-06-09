# Changelog

All notable changes documented here. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.2.0] — real SRT push (#176)

### Added
- `src/wave-srt.h` + `src/wave-srt.c` — libsrt C-API wrapper. One active
  socket per output instance, non-blocking sends (SRTO_SNDSYN=false +
  SRTO_LATENCY=120ms matching wave-desktop's encoder default), pthread
  mutex guards the socket against the encoder thread ↔ stop() race.
  Public surface: `wave_srt_open()` / `wave_srt_push()` /
  `wave_srt_close()` / `wave_srt_parse_url()` / `wave_srt_strerror()`.
- `src/wave-srt-parse.c` — URL parser + strerror split from wave-srt.c
  so they are stdlib-only and unit-testable without libsrt / libobs.
- `src/wave-srt-test.c` + `src/wave-srt-test-stub.c` — 10 vitest-style
  cases pinning the parser (wave:// + srt:// schemes, no-port default,
  streamid query param, bad-scheme rejection, port-range bounds,
  scratch-overrun safety, NULL-input handling). Wired as a CTest target.
- `wave-output.c` now:
    - Opens libsrt on `start()` (reads `gateway_url` + `stream_key`
      from settings; surfaces clear error via `obs_output_set_last_error`
      on bad URL / missing key).
    - Implements `encoded_packet` callback — pushes to libsrt, drops on
      WAVE_SRT_E_WOULDBLOCK with a log warning (encoder thread MUST
      keep moving; backpressure into libobs would stall capture),
      signals OBS_OUTPUT_DISCONNECTED on hard error so OBS shows the
      failure to the operator.
    - Closes the socket on `stop()` and on destroy-while-active.
- `CMakeLists.txt` resolves libsrt via `find_package(srt CONFIG)` →
  `PkgConfig::SRT` fallback; clear `FATAL_ERROR` with install
  instructions per platform when neither is present. Adds CTest target
  `wave-srt-test` (parser-only, no libsrt link required for the test).
- `data/locale/en-US.ini` — `WAVE.Error.MissingStreamKey` string.
- `capabilities.json` bumped 0.1.0 → 0.2.0; the WAVE API gateway endpoint
  reference already pointed at `/ingest/srt/{stream-key}` and is now
  accurate.

### Why ffmpeg subprocess is NOT used here (deliberate divergence from #173)
The wave-desktop encoder (task #173) spawns ffmpeg-as-child-process to
keep operator install simple (`brew install ffmpeg`). For an OBS plugin
that's the wrong call: OBS already owns the encoder, and every encoded
packet would need to pipe across a process boundary at frame rate.
Direct libsrt link adds one dependency (which OBS itself already links
on most distros) and removes the per-packet IPC cost. Different threat
+ perf model, different choice.

### Added (Wave 1 scaffold — pre-existing)
- Initial scaffold: OBS Studio plugin skeleton (GPL-2.0)
- Module entry points (`plugin-main.c`) — `obs_module_load` / `obs_module_unload`
- Custom output registration (`wave-output.c` / `wave-output.h`) —
  `obs_output_info` registered via `obs_register_output()` (stubbed
  create/destroy/start/stop, raw_video/raw_audio NULL until Wave 2)
- Settings UI (`wave-properties.c`) — gateway URL, stream key, codec
  selector (H.264 / HEVC / AV1, AV2 reserved)
- Cross-platform CMake build (macOS / Windows / Linux) + `CMakePresets.json`
- `buildspec.json` for OBS CI flow
- `cmake/finders/Findlibobs.cmake` fallback finder for dev machines
- Foundation chassis: CODEOWNERS, SECRETS.md, threat-model.md,
  CONTRIBUTING.md, foundation-gate workflow, `.foundation-version` pin

### Pending (next waves)
- Gateway JWT bearer exchange (today the stream-key goes directly into
  streamid; gateway accepts it as a long-lived bearer for the MVP)
- Protobuf metadata frames (scene name, operator id) for the gateway
- AV2 codec wiring once ffmpeg 8.2 `--enable-libavm` lands (foundation
  codec-watch sticky tracks this via #334)
- IPv6 SRT targets (port wave-desktop's bracketed-host pattern from
  #173 into wave-srt-parse.c)
- Sentry + structured logging
