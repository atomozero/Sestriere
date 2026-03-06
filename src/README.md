# Sestriere

> Native MeshCore LoRa mesh client for Haiku OS

## Overview

**Sestriere** is a native Haiku OS MeshCore client for LoRa mesh networking, with a Telegram-style user interface. It implements the MeshCore Companion Radio Protocol V3.

## Screenshots

![Chat with GIF, Emoji & Images](../img/screenshot01.png)

![Network Map](../img/screenshot02.png)

![Geographic Map with OSM Tiles](../img/screenshot03.png)

![Packet Analyzer](../img/screenshot04.png)

![Mission Control Dashboard](../img/screenshot05.png)

## Features

### Messaging
- **Telegram-style UI** — 3-panel layout: contact sidebar, chat area, info panel
- **Contact List** — Avatar initials, last message preview, timestamps, unread badges
- **Contact Search** — Real-time filter bar at top of sidebar (case-insensitive)
- **Contact Groups** — Organize contacts into custom groups with sidebar separators
- **Contact/Channel Muting** — Suppress notifications per contact or channel
- **Auto-growing Input** — Multi-line message input (1-4 lines), Enter to send, Shift+Enter for newline
- **Chat Bubbles** — Color-coded message display with sender colors, SNR indicators, delivery status
- **Direct Messages** — Private messages with delivery confirmation and RTT display
- **Channel Messages** — Public and private encrypted channel support
- **Message Search** — Full-text search across chat history (Cmd+F)
- **Message Persistence** — SQLite database with deduplication and SNR metadata
- **Desktop Notifications** — System notifications (suppressed for muted items)
- **Ping** — Single-hop ping via trace path with RTT measurement
- **GIF Sharing** — GIPHY animated GIF picker with thumbnail grid, cross-compatible with meshcore-open
- **Emoji Rendering** — Unicode emoji displayed as PNG sprites with alpha compositing
- **Image Sharing** — LoRa chunked image transfer with auto-fetch and chat integration
- **SAR Markers** — Search and rescue marker parsing and display

### Network Visualization
- **Network Map** (Cmd+M) — Force-directed topology with SNR-colored links
  - Animated flow dots along connections
  - Trace route visualization with per-hop SNR
  - Full mesh topology discovery ("Map Network" button)
  - Auto-trace for background route monitoring
  - Edge persistence in SQLite (30-day expiry)
  - Contact name resolution on nodes
  - Repeater node merging with center "Me" node
  - Resizable to fullscreen

- **Geographic Map** (Cmd+G) — Coordinate grid with zoom/pan/compass
  - GPS node positions with hop-count colored connections
  - OSM tile overlay with offline cache (TileCache)
  - Coastline rendering (CoastlineData)
  - SAR marker display
  - Scale bar and compass for orientation
  - GPX export for contacts with coordinates

### Radio Analysis
- **Packet Analyzer** (Cmd+Shift+P) — Wireshark-style real-time capture
  - Color-coded packet types (blue=messages, green=adverts, amber=alerts, purple=raw)
  - 27+ decoded packet types with human-readable descriptions
  - Hex dump, decoded fields, and technical detail views
  - SNR trend chart and contact heatmap tabs
  - Delta-t timing between packets (microsecond precision)
  - CSV export

- **Statistics Window** (Cmd+S) — Core/radio/packet stats
  - Battery, uptime, RSSI, SNR, noise floor
  - TX/RX packet counters with auto-refresh
  - Color-coded thresholds

- **Telemetry Dashboard** (Cmd+Y) — Sensor graphs with time ranges
  - Battery, storage, noise floor, RSSI, SNR
  - Time ranges: 1m to 7 days
  - Database-backed history with CSV export

- **Mission Control** (Cmd+Shift+D) — Unified dashboard
  - Health score arc, SNR/RSSI trend, packet rate histogram
  - Mini network topology, session timeline, contact heatmap grid
  - Alert banners, activity feed, quick actions

### Device Control
- **Settings** — Node name, location, TX power, 12 radio presets
- **Battery Chemistry** — LiPo, LiFePO4, NMC voltage curves
- **Repeater Admin** — Remote administration after login (stats, contacts, reboot, factory reset)
- **Profile Export/Import** — JSON-based configuration backup and restore

### Serial & Repeater Monitoring
- **Serial Monitor** — Terminal-style CLI interaction for non-companion devices
- **Repeater Monitor** — Structured log analysis with per-node stats and topology extraction

### MQTT Integration
- **MQTT Bridge** — Relay messages to MQTT broker
- **MQTT Log** (Cmd+Shift+M) — Timestamped connection events and publish reports
- **Auto-reconnect** — Exponential backoff (5s to 60s)

### Haiku Integration
- **Deskbar Replicant** — Tray icon with connection status, battery, and unread count
- **People Files** — Contacts exported as Haiku People files with MeshCore attributes
- **Theme-Aware** — All colors use `ui_color()` / `tint_color()` for light/dark themes

## Building

### Requirements
- Haiku OS R1/beta5 or later (x86_64)
- GCC compiler
- libmosquitto (MQTT support)
- SQLite3 (message/telemetry storage)

### Install Dependencies
```bash
pkgman install mosquitto_devel sqlite_devel curl_devel giflib_devel
```

### Build
```bash
cd src
make -j4
./objects.x86_64-cc13-debug/Sestriere
```

### Release Build
```bash
make OBJ_DIR=release OPTIMIZE=FULL
```

## Usage

1. Connect MeshCore device via USB
2. Launch Sestriere
3. Press Cmd+O to connect (select serial port)
4. Wait for contact sync to complete
5. Select a contact or Public Channel to start messaging

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Cmd+O | Connect |
| Cmd+D | Disconnect |
| Cmd+R | Sync Contacts |
| Cmd+A | Send Advertisement |
| Cmd+T | Trace Path |
| Cmd+F | Search Messages |
| Cmd+B | Toggle Sidebar |
| Cmd+I | Toggle Info Panel |
| Cmd+L | Debug Log |
| Cmd+S | Statistics |
| Cmd+M | Network Map |
| Cmd+G | Geographic Map |
| Cmd+Y | Telemetry |
| Cmd+Shift+P | Packet Analyzer |
| Cmd+Shift+D | Mission Control |
| Cmd+Shift+M | MQTT Log |
| Cmd+Q | Quit |

## File Structure

```
src/
├── Makefile
├── README.md                     # This file
├── Sestriere.cpp                 # BApplication entry point
├── MainWindow.cpp/h              # Main window with 3-panel layout
├── ProtocolHandler.cpp/h         # Protocol parsing (extracted from MainWindow)
├── SerialHandler.cpp/h           # USB serial communication (BLooper)
├── DatabaseManager.cpp/h         # SQLite database (messages, SNR, telemetry, groups)
├── Types.h                       # Protocol structures & radio presets
├── Constants.h                   # Application constants & thresholds
├── Compat.h                      # BObjectList API compatibility across Haiku versions
├── Utils.h                       # Shared utilities (FormatUptime, ParseHex, etc.)
├── ChatView.cpp/h                # Message list display
├── ChatHeaderView.cpp/h          # Chat area header
├── MessageView.cpp/h             # Chat bubble rendering
├── ContactItem.cpp/h             # Contact list item with status dots & groups
├── ContactInfoPanel.cpp/h        # Right-side contact detail panel
├── SNRChartView.cpp/h            # SNR history chart in info panel
├── TopBarView.cpp/h              # Unified top bar (icons + status indicators)
├── GrowingTextView.cpp/h         # Auto-growing multi-line input
├── SettingsWindow.cpp/h          # Settings (Device, Radio, MQTT tabs)
├── StatsWindow.cpp/h             # Statistics window
├── TracePathWindow.cpp/h         # Trace path visualization
├── NetworkMapWindow.cpp/h        # Force-directed network topology
├── MapView.cpp/h                 # Geographic map view
├── TelemetryWindow.cpp/h         # Telemetry dashboard with graphs
├── PacketAnalyzerWindow.cpp/h    # Wireshark-style packet analyzer
├── MissionControlWindow.cpp/h    # Unified mission control dashboard
├── LoginWindow.cpp/h             # Remote login dialog
├── AddChannelWindow.cpp/h        # Channel creation dialog
├── ContactExportWindow.cpp/h     # Contact export dialog
├── ProfileWindow.cpp/h           # Profile export/import (JSON)
├── SerialMonitorWindow.cpp/h     # Terminal-style serial CLI monitor
├── RepeaterMonitorView.cpp/h     # Structured repeater log viewer (BView)
├── RepeaterMonitorWindow.cpp/h   # Repeater monitor window (standalone)
├── DebugLogWindow.cpp/h          # Debug log window
├── MqttClient.cpp/h              # MQTT client (libmosquitto)
├── MqttLogWindow.cpp/h           # MQTT event log
├── NotificationManager.cpp/h     # Desktop notifications
├── DeskbarReplicant.cpp/h        # Deskbar tray integration
├── GiphyClient.cpp/h             # GIPHY API client (search, trending, download)
├── GifPickerWindow.cpp/h         # Animated GIF picker grid window
├── EmojiRenderer.cpp/h           # Unicode emoji PNG sprite rendering
├── ImageCodec.cpp/h              # Image compress/decompress + GIF frame decode
├── ImageSession.cpp/h            # LoRa chunked image transfer session
├── SarMarker.cpp/h               # SAR marker parsing (meshcore-sar protocol)
├── TileCache.cpp/h               # OSM map tile download and cache
└── CoastlineData.h               # Coastline polygon data for geographic map
```

## Author

Created by **Andrea Bernardi**.

## Credits

- MeshCore protocol by the MeshCore team
- MQTT integration compatible with [meshcore-to-maps](https://github.com/xpinguinx/meshcore-to-maps)
- Built for Haiku OS using the Be API

## License

MIT License
