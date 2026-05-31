# Changelog

All notable changes documented here. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
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

### Pending (Wave 2 — task #163)
- Real SRT push to gateway via `libsrt`
- Gateway JWT bearer exchange + stream-id header
- Protobuf metadata frames (scene name, operator id) for the gateway
- AV2 codec wiring once ffmpeg 8.2 `--enable-libavm` lands
- Sentry + structured logging
