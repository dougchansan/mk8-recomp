# Bootstrap Record

Session 1. All values below were measured on this machine, not assumed.

## Workspace

| | |
|---|---|
| Repo root | wherever the clone lives; scripts derive it from their own location, or `MK8R_ROOT` |
| Pinned upstream | `<root>/third_party/suyu` (gitignored) |
| Build output | `<root>/build/suyu` |
| Public remote | https://github.com/dougchansan/mk8-recomp |

The workspace needs room for the Suyu build plus generated C - budget a few
hundred GiB on whichever volume the clone sits on.


## Input title (READ ONLY — never modified)

| | |
|---|---|
| Path | `$MK8R_ROM` (not recorded here) |
| Size | 15,971,909,632 bytes (14.87 GiB) |
| SHA-256 | `REDACTED` |
| LastWriteTime | 2024-09-07 18:16:56 |
| Filesystem | NTFS (F:, 9.09 TiB total, 3.19 TiB free) |

Sibling files present and untouched:
- the cartridge's sibling `(Certificate).bin` and `(Initial Data).bin`

Neither is used unless Suyu's loader demands it.

## Host machine

| | |
|---|---|
| OS | Windows 11 Pro 10.0.26200 |
| CPU | AMD Ryzen 9 9950X3D — 16C / 32T |
| RAM | 125.6 GiB |
| GPU | AMD Radeon RX 9070 XT (driver 32.0.31041.1004) |
| | AMD Radeon(TM) Graphics (iGPU, 32.0.21045.5002) |
| | Parsec Virtual Display Adapter |

## Toolchain present

| Tool | Version |
|---|---|
| git | 2.54.0.windows.1 |
| git-lfs | 3.7.1 |
| gh | 2.88.1 (authed as dougchansan) |
| cmake | 4.3.2 |
| ninja | 1.13.0 |
| python | 3.12.10 |
| MSVC | 14.50.35717 (VS 2026 Community; located at run time via `vswhere`, or `MK8R_VCVARS`) |
| MSVC (alt) | 14.44.35207 (VS 2022 Build Tools) |
| Windows SDK | 10.0.26100.0 |

Not present: `clang` / `clang-cl` on PATH, Vulkan SDK, standalone Qt.
Neither is a blocker — Suyu vendors `externals/Vulkan-Headers` and
`YUZU_USE_BUNDLED_QT` defaults ON under MSVC (downloads a Qt binary drop).

## Keys / firmware

No `%APPDATA%\suyu`, `\yuzu`, or `\eden` user directory exists — Suyu has never been
run on this machine, so it has no configured key store.

One user-owned key file was located on local storage:

```
<your prod.keys>
12,584 bytes, 2022-10-17
```

This is a firmware-15.0.1-era key set. Nothing was downloaded. Whether it carries a
high enough master key generation for this XCI's update partition is unverified —
see `docs/progress.md`.

## Upstream pin

| | |
|---|---|
| Repo | `suyu-emu/suyu-v0.0.4` (public archive, GPL-3.0) |
| Commit | `d1d09321d7ab84252291e05b3efbc8a8dfa57481` |
| Date | 2026-09-04 22:40:58 +0100 |
| Subject | `feat: build the libretro core for Windows as well as Linux` |
| Default branch | `main` |

Submodules are NOT checked in; `git submodule update --init --recursive` is required.

## Build result (session 1)

| | |
|---|---|
| Compiler | MSVC 14.50.35717 (VS2026 Community), runtime 14.51.36247 |
| Generator | Ninja 1.13.0 |
| Build type | Release |
| Commit | `d1d09321d7ab84252291e05b3efbc8a8dfa57481` |
| Qt | 6.9.3 `msvc2022_64`, official, via `aqtinstall` (not the bundled drop) |
| glslang | 16.5.0 standalone (no Vulkan SDK installed) |
| Targets | 1259 |
| Duration | 4.8 min (full rebuild) |
| Output | `build/suyu/bin/suyu.exe` 40,048,128 B; `suyu-cmd.exe` 30,234,624 B |

CMake options: see `scripts/build-suyu.ps1`.

`--help` prints nothing on either binary: upstream links them
`/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup` in Release
(`src/suyu_cmd/CMakeLists.txt:142`), so `std::cout` is discarded. "Runs" is
evidenced by the GUI's log and its MCP server, not by console output.

## Keys — insufficient

The one user-owned key file on this machine covers `master_key_00`-`master_key_0e`
and `titlekek_00`-`titlekek_0e` (firmware ~15.x, 2022). The XCI is a 2024
cartridge dump requiring generation `0f` or higher.

Result: `KeyManager` hands out a zero-filled titlekek, `content_archive.cpp:65`'s
`HasKey` guard passes anyway, and suyu dies with `0xC0000094`. See issues #2 and
#18.

Nothing was downloaded, derived, or searched for. A newer `prod.keys` must be
supplied by the user at `%APPDATA%\suyu\keys\prod.keys`.
