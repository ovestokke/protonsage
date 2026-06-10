# ProtonSage

ProtonSage is a local Linux desktop utility for Steam/Proton troubleshooting. It imports ProtonDB snapshot data, scans the local Steam library read-only, ranks recent reports, and presents deterministic launch-option recommendations.

Current stack: **native Qt6/C++**, SQLite, no webview/Electron/Wails.

## Safety status

- Reads Steam library metadata only: `libraryfolders.vdf`, `appmanifest_*.acf`, and `userdata/*/config/localconfig.vdf`.
- Does **not** write Steam config.
- Does **not** call external AI providers.
- Imports ProtonDB data from `bdefore/protondb-data` snapshot archives only; it does not clone the full repository.
- Future Steam config writes must require explicit confirmation, exact preview, backup, and restore path.

## Requirements

Ubuntu/Debian package names:

```bash
sudo apt install build-essential cmake ninja-build zlib1g-dev \
  qt6-base-dev qt6-base-dev-tools libqt6sql6-sqlite \
  libgl1-mesa-dev libxkbcommon-dev
```

Arch/CachyOS package names:

```bash
sudo pacman -S --needed base-devel cmake ninja zlib qt6-base
```

## Build and test

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/import_dedup_test
./build/smoke_test
./build/protonsage
```

If you already have a Makefile build directory:

```bash
cmake -B build
cmake --build build
```

## Data locations

- Database: `$XDG_DATA_HOME/protonsage/protonsage.db`, fallback `~/.local/share/protonsage/protonsage.db`
- Cache: `$XDG_CACHE_HOME/protonsage/`, fallback Qt's standard cache location

## Steam discovery

ProtonSage checks these roots read-only:

- `$PROTONSAGE_STEAM_ROOTS` — optional colon-separated override, highest priority
- `~/.steam/steam`
- `~/.steam/root`
- `$XDG_DATA_HOME/Steam` or `~/.local/share/Steam`
- `~/.var/app/com.valvesoftware.Steam/.local/share/Steam`
- `~/snap/steam/common/.local/share/Steam`

It then parses `steamapps/libraryfolders.vdf` to find secondary libraries.

## Project layout

```text
src/core/          Shared domain models
src/storage/       SQLite schema and query layer
src/protondb/      Snapshot listing and full-snapshot import
src/steam/         Steam path detection, VDF parser, read-only library scan
src/system/        Read-only Linux hardware/OS detection from /proc and /sys
src/advisor/       Deterministic scoring, extraction, recommendations, preview
src/ui/            Qt6 widgets UI and image cache
```

## ProtonDB source/license

Primary source: <https://github.com/bdefore/protondb-data>

The ProtonDB export data is published under ODbL/DbCL terms. ProtonSage stores snapshot filename/date/source URL/import time and attribution metadata; exports must preserve that attribution.
