# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Sestriere is a native Haiku OS MeshCore LoRa mesh client. It communicates with LoRa devices (Heltec v3.2, T-Deck, etc.) via USB serial using the MeshCore Companion Radio Protocol V3. Built entirely with Haiku's Be API.

## Build Commands

```bash
# Build main app (from src/)
cd src && make -j4
# Binary: objects.x86_64-cc13-debug/Sestriere

# Release build
make OBJ_DIR=release OPTIMIZE=FULL

# Build companion apps
cd fake_radio && make -j4      # Radio simulator
cd repeater_monitor && make -j4 # Repeater log analyzer
```

**Dependencies** (via `pkgman install`): `mosquitto_devel sqlite_devel curl_devel giflib_devel`
Codec2 must be built from source to `/boot/system/non-packaged`.

## Running Tests

```bash
cd src/tests
./run_tests.sh              # Run all tests
./run_tests.sh phase        # Filter by pattern (runs test_phase*)
./run_tests.sh los          # Run only LoS-related tests
```

Tests are standalone `.cpp` files compiled individually with `g++`. The runner builds each, runs it, and reports PASS/FAIL/SKIP. Tests needing SQLite: `test_error_handling test_phase1_fixes test_phase2_fixes test_phase3_fixes test_security`.

## Architecture

### Core Threading Model

- **Main thread**: BApplication + all UI (BWindow, BView)
- **SerialHandler**: BLooper in its own thread, does POSIX serial I/O with `select()` timeout
- **Communication**: All cross-thread via BMessage posting — never direct method calls

### Key Components

- **MainWindow** (`src/MainWindow.cpp`, ~250KB): Central hub. Owns all child windows, routes all BMessages, parses serial frames via `_ParseFrame()`, handles 100+ message types
- **SerialHandler** (`src/SerialHandler.cpp`): Frame-level serial I/O: `[marker][len_lo][len_hi][payload...]`
- **ProtocolHandler** (`src/ProtocolHandler.cpp`): Builds MeshCore V3 binary commands
- **DatabaseManager** (`src/DatabaseManager.cpp`): SQLite persistence, thread-safe via `BLocker fLock`
- **Types.h / Constants.h**: All protocol structs, command codes, message codes, thresholds

### Window Hierarchy

MainWindow owns all child windows (Settings, NetworkMap, Telemetry, PacketAnalyzer, MissionControl, etc.). Child windows send BMessages back to MainWindow — they never talk to SerialHandler directly.

### UI Layout

3-panel BSplitView: sidebar (contacts) | chat | info panel. TopBarView is a 24px unified bar with 8 clickable icon areas. All colors use `ui_color()`/`tint_color()` for theme awareness.

## Critical Rules

### ODR (One Definition Rule)
Local classes in `.cpp` files are NOT truly local — the linker sees them globally. If two `.cpp` files define a class with the same name but different members, the linker picks ONE vtable → crash. **Always use unique class names** (e.g., `TelemetrySectionHeader` not `SectionHeaderView`).

### Thread Safety
- **Never `snooze()` in MessageReceived** — blocks entire UI. Use BMessageRunner instead
- **Always `LockLooper()` before** `IsHidden()`/`Show()`/`Activate()` on child windows
- Use `localtime_r()`/`gmtime_r()` — the non-reentrant versions are unsafe
- Use `strlcpy()` not `strncpy()`

### Window Lifecycle
- All BWindow subclasses MUST override `QuitRequested()` with `Hide(); return false;` if MainWindow holds a pointer
- Never set child window pointer to NULL without first `Lock()`+`Quit()`

### Memory & Types
- `BBitmap::InitCheck()` before use
- No VLAs — use `new[]`/`delete[]`
- `bigtime_t` is `long int` on Haiku (not `long long`) — cast to `int32` for printf or use `%ld`
- Cannot take `&ui_color()` — store in local variable first (rvalue)
- `size_t` subtraction: always `(a > b) ? (a - b) : 0` to prevent unsigned underflow

### Haiku API Gotchas
- `B_LINK_COLOR` does not exist on Haiku R1 beta5 — use `B_CONTROL_HIGHLIGHT_COLOR`
- `GetExplicitPreferredSize()` doesn't exist — use `ExplicitPreferredSize()` (returns BSize)
- `BPopUpMenu::Go()` with `async=true` auto-deletes the menu — not a leak
- `BPopUpMenu` submenus: must call `SetTargetForItems(this)` on EACH submenu separately
- `BMessageRunner`: delete before recreating, set to NULL on disconnect
- `BObjectList` template: `BObjectList<Type, true>` where `true` = owning

### Protocol V3 Compliance
- APP_START: 8+ bytes `[code][ver=3][reserved×6][appName]`
- Radio params: **frequency and bandwidth in Hz** (uint32 LE), NOT kHz
- RSP_SELF_INFO pubkey at offset **[4-35]**, not [1-32]
- CMD_SEND_LOGIN: **full 32-byte pubkey**, not 6-byte prefix; **password max 15 chars** (firmware `char password[16]` with null-terminating strncpy)
- CMD_REMOVE_CONTACT: **full 32-byte pubkey** (6-byte gives error code 2)
- CMD_SEND_RAW_DATA: `[0x19][path_len][path...][payload]` — path_len byte is mandatory
- V3 messages (0x10, 0x11): SNR at [1], offsets shifted by 3 bytes vs V2
- Contact frame is 148 bytes; outPath hashes are source→dest order
- RSP_ERR `[0x01, N≤3]` during init is APP_START ack, not a real error

## Coding Style

- **Tabs** for indentation
- `f` prefix for member variables (e.g., `fChannels`)
- `_` prefix for private methods (e.g., `_ParseFrame()`)
- No hardcoded colors — use `ui_color()` and `tint_color()`
- Don't use `memset` on structs with constructors — use value-initialization

## Platform Notes

- **ripgrep (`rg`) does not exist on Haiku** — use `grep` via Bash
- Build uses Haiku's `makefile-engine` from `/boot/system/develop/etc/makefile-engine`
- Linked libraries: `be tracker shared columnlistview mosquitto sqlite3 translation curl gif codec2 media`
- Database stored at `~/.config/settings/Sestriere/sestriere.db`
