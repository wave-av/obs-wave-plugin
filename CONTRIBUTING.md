# Contributing to obs-wave-plugin

Thanks for your interest. Read this in full before opening a PR.

## License

Source is **GPL-2.0-or-later** (see `LICENSE`). This is **not optional** — the
OBS Studio library this plugin links against (`libobs`) is GPL-2.0; under the
GPL's strong-copyleft terms our derivative work must inherit the same
license. By contributing you agree your contributions are also GPL-2.0.

## License boundary — IMPORTANT

This repository ships **no vendor-licensed binaries** and **no upstream
headers**. The following are deny-listed in `.gitignore` and **must never be
committed**:

- `libobs.*`, `libobs.dylib`, `libobs.so`, `libobs.dll` — pulled by CMake at
  configure time via `find_package(libobs CONFIG REQUIRED)` or via the OBS
  plugin-template's helpers; never bundled
- OBS Studio headers (`obs.h`, `obs-module.h`, `obs-source.h`, etc.) —
  consumed from the installed `libobs` package; never copied into `src/`
- `libsrt.*` and related transport libraries — system-installed
- `Processing.NDI.Lib*` / `libndi*` — never used by this plugin, but
  deny-listed defensively (operators sometimes confuse NDI with SRT)

These libraries are resolved on each contributor's machine via their OS
package manager (homebrew / vcpkg / apt) or via the OBS-recommended CMake
flow. If you accidentally `git add` one, the PR-side foundation gate will
refuse to merge. Run `git reset HEAD <path>` and add the path to
`.gitignore`.

## Dev setup

You need:

- CMake ≥ 3.28
- A C/C++ toolchain (clang on macOS, MSVC on Windows, gcc/clang on Linux)
- `libobs` installed (build OBS Studio from source per
  <https://github.com/obsproject/obs-studio/wiki/Install-Instructions>, or
  install the `libobs-dev` package on Debian/Ubuntu)
- Ninja (recommended generator)

```sh
cmake --preset macos
cmake --build --preset macos
```

Output: `build_macos/wave-output.plugin/` (macOS bundle) or
`build/wave-output.{dll,so,dylib}` depending on platform.

To install for local dev (macOS):

```sh
cp -R build_macos/wave-output.plugin \
  ~/Library/Application\ Support/obs-studio/plugins/
```

Restart OBS. Open **Settings → Stream → Service** and pick **WAVE**.

## Architecture rules

1. **Plugin runs in the OBS process.** Treat the OBS process as the trust
   boundary — never trust user-pasted stream keys without validation, never
   log them, never write them to disk outside OBS's own config store.
2. **One responsibility per file.** Target 200–500 lines per C/C++ file;
   split before 800. The gate enforces a hard 1000-line ceiling.
3. **No `printf`.** Use `blog(LOG_INFO|LOG_WARNING|LOG_ERROR, ...)` so OBS's
   logger captures and rotates plugin output.
4. **No exceptions across the FFI boundary.** OBS is C. Return error codes
   from C++ callsites and translate at the API boundary.
5. **Memory ownership documented at every public symbol.** OBS uses manual
   refcounting (`obs_source_addref` / `obs_source_release`). Mismatched
   refcounts cause crashes in long sessions.

## PR shape

- Branch off `main`. Name: `feat/<short-thing>` or `fix/<short-thing>`.
- One concern per PR. Don't bundle a refactor and a feature.
- Update `CHANGELOG.md` under the unreleased heading.
- Self-test: `cmake --build --preset <platform>` must succeed against an
  installed `libobs`.
- CodeRabbit + the foundation gate review automatically. Resolve every
  comment before requesting human review.

## Reporting security issues

Open a private GitHub Security Advisory at
<https://github.com/wave-av/obs-wave-plugin/security/advisories/new>. Do NOT
open a public issue.
