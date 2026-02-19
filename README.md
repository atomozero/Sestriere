# Sestriere

> Native MeshCore LoRa mesh client for Haiku OS

## Overview

**Sestriere** is a 100% native Haiku OS application that serves as a MeshCore client, communicating with LoRa devices (Heltec v3.2, T-Deck, etc.) via USB serial.

The name recalls the Venetian *sestieri* – interconnected districts like nodes in a mesh network.

## Features

### Core Features
- **Native Haiku UI** — Built entirely with Haiku's native Be API, theme-aware colors via ui_color()
- **USB Serial Communication** — POSIX-based serial with DTR/RTS support for ESP32 devices
- **Contact Management** — View, sync, and manage mesh network contacts
- **Messaging** — Send and receive direct messages and channel (broadcast) messages
- **Message Persistence** — Chat history saved to disk per contact

### Device Control
- **Settings Window** — Configure node name, lat/lon location, TX power, and radio parameters with 12 MeshCore radio presets (frequency, bandwidth, SF, CR)
- **Battery Monitoring** — Real-time battery voltage and storage status
- **Device Statistics** — Core, radio, and packet statistics with auto-refresh
- **Login to Repeater/Room** — Authenticate to password-protected repeaters and rooms

### Visualization
- **Geographic Map** — Lat/lon-based map with zoom, pan, grid, compass, scale bar, and connection lines colored by hop count
- **Mesh Graph** — Force-directed network topology with animated nodes, signal strength indicators, and node type coloring
- **Telemetry Dashboard** — Sensor data graphs with time range selection, min/max/avg stats, and CSV export to Desktop

### Advanced Features
- **Trace Path** — Visualize message routing through the mesh
- **Contact Export/Import** — Export contacts as hex data, import via clipboard paste
- **Desktop Notifications** — System notifications for new messages
- **MQTT Bridge** — Relay messages to MQTT broker (meshcoreitalia.it)

## Requirements

- Haiku OS R1/beta5 or later
- MeshCore-compatible LoRa device with USB Serial Companion firmware
- USB cable

### USB Serial Driver Note

For Silicon Labs CP210x devices (like Heltec LoRa32 v3.2), you may need the patched USB serial driver. See `HAIKU_USB_SERIAL_FIX.md` for details.

## Building

### Using Makefile (recommended)

```bash
cd src
make
./objects.x86_64-cc13-debug/Sestriere
```

## Usage

1. Connect your MeshCore device via USB
2. Launch Sestriere
3. Select the serial port (typically `/dev/ports/usb0`)
4. Wait for contact sync to complete
5. Select a contact or Public channel to start messaging

### Keyboard Shortcuts

- `Cmd+B` — Toggle sidebar
- `Cmd+I` — Toggle info panel
- `Cmd+R` — Refresh contacts
- `Cmd+M` — Show network map
- `Cmd+G` — Show geographic map
- `Cmd+L` — Show debug log
- `Cmd+S` — Show statistics

## Supported Hardware

- Heltec LoRa32 v3/v3.2
- LilyGO T-Deck
- LilyGO T-Beam
- RAK WisBlock
- Other MeshCore-compatible devices with USB Serial Companion firmware

## Protocol

Sestriere implements the [MeshCore Companion Radio Protocol](https://github.com/ripplebiz/MeshCore/wiki/Companion-Radio-Protocol) (V3):

- Frame format: `[marker][len_lo][len_hi][payload...]`
- Inbound marker (App → Radio): `<` (0x3C)
- Outbound marker (Radio → App): `>` (0x3E)
- All multi-byte values are Little Endian
- Default baud rate: 115200 8N1

### Protocol Version

Sestriere requests **protocol V3** via `CMD_APP_START`. V3 adds SNR fields to incoming messages and uses different byte layouts for `RSP_CONTACT_MSG_RECV_V3` (0x10) and `RSP_CHANNEL_MSG_RECV_V3` (0x11). V2 responses (0x07 and 0x08) are also supported for backwards compatibility.

### MeshCore Node Types (ADV_TYPE)

| Type | Value | Description |
|------|-------|-------------|
| NONE | 0 | Unknown / not advertised |
| CHAT | 1 | Chat device (phone, PC, T-Deck) |
| REPEATER | 2 | Relay/router node, extends mesh coverage |
| ROOM | 3 | Room server, group chat host |

### Implemented Commands (App → Radio)

| Code | Command | Description |
|------|---------|-------------|
| 0x01 | CMD_APP_START | Initialize connection (V3, 8+ bytes with app name) |
| 0x02 | CMD_SEND_TXT_MSG | Send direct message |
| 0x03 | CMD_SEND_CHANNEL_TXT_MSG | Send channel message (with channel_idx) |
| 0x04 | CMD_GET_CONTACTS | Sync contact list |
| 0x07 | CMD_SEND_SELF_ADVERT | Broadcast self advertisement |
| 0x08 | CMD_SET_ADVERT_NAME | Change node name |
| 0x0A | CMD_SYNC_NEXT_MESSAGE | Fetch next waiting message |
| 0x0B | CMD_SET_RADIO_PARAMS | Configure radio (freq/bw in Hz, SF, CR) |
| 0x0C | CMD_SET_RADIO_TX_POWER | Set transmit power (dBm) |
| 0x0E | CMD_SET_ADVERT_LATLON | Set GPS location (int32 x 1E6) |
| 0x11 | CMD_EXPORT_CONTACT | Export contact data |
| 0x12 | CMD_IMPORT_CONTACT | Import contact data |
| 0x14 | CMD_GET_BATT_AND_STORAGE | Battery voltage + storage KB |
| 0x16 | CMD_DEVICE_QUERY | Get device info |
| 0x1A | CMD_SEND_LOGIN | Authenticate with full 32-byte pubkey |
| 0x24 | CMD_SEND_TRACE_PATH | Trace route to contact |
| 0x27 | CMD_SEND_TELEMETRY_REQ | Request telemetry data |
| 0x38 | CMD_GET_STATS | Get statistics (3 subtypes) |

### Responses (Radio → App)

| Code | Response | Key fields |
|------|----------|------------|
| 0x00 | RSP_OK | Command acknowledged |
| 0x01 | RSP_ERR | Error or APP_START version reply |
| 0x03 | RSP_CONTACT | 148 bytes: pubkey[32], type, flags, outPathLen, name[32], lastSeen, lat/lon |
| 0x05 | RSP_SELF_INFO | type, txPower, pubkey[32], lat/lon, radio params, name |
| 0x07 | RSP_CONTACT_MSG_RECV | V2: pubkey[6], pathLen, txtType, timestamp, text |
| 0x08 | RSP_CHANNEL_MSG_RECV | V2: channelIdx, pathLen, txtType, timestamp, text |
| 0x0C | RSP_BATT_AND_STORAGE | battMv (uint16), usedKb (uint32), totalKb (uint32) |
| 0x0D | RSP_DEVICE_INFO | fwVer, maxContacts/2, maxChannels, buildDate, board, version |
| 0x10 | RSP_CONTACT_MSG_RECV_V3 | V3: snr, pubkey[6], pathLen, txtType, timestamp, text |
| 0x11 | RSP_CHANNEL_MSG_RECV_V3 | V3: snr, channelIdx, pathLen, txtType, timestamp, text |
| 0x18 | RSP_STATS | 3 subtypes: core (uptime uint32), radio (noiseFloor/rssi/snr), packets (rx/tx) |

### Push Notifications (Radio → App, unsolicited)

| Code | Notification | Description |
|------|--------------|-------------|
| 0x80 | PUSH_ADVERT | Contact advertisement received |
| 0x81 | PUSH_PATH_UPDATED | Routing path changed |
| 0x82 | PUSH_SEND_CONFIRMED | Message delivery confirmed |
| 0x83 | PUSH_MSG_WAITING | New message available for download |
| 0x85 | PUSH_LOGIN_SUCCESS | Login accepted |
| 0x86 | PUSH_LOGIN_FAIL | Login rejected |
| 0x89 | PUSH_TRACE_DATA | Trace path: pathLen, hashes[], snrs[] |
| 0x8B | PUSH_TELEMETRY_RESPONSE | Sensor telemetry data |

## Project Structure

```
Sestriere/
├── README.md                       # This file
├── HAIKU_USB_SERIAL_FIX.md         # USB driver patch docs
├── src/                            # Main project source
│   ├── Makefile                    # Build system
│   ├── Sestriere.cpp/h             # BApplication entry point
│   ├── MainWindow.cpp/h            # Main window + protocol handler
│   ├── SerialHandler.cpp/h         # POSIX serial I/O (BLooper)
│   ├── ChatView.cpp/h              # Telegram-style message display
│   ├── ChatHeaderView.cpp/h        # Chat header with contact info
│   ├── MessageView.cpp/h           # Chat bubble rendering
│   ├── ContactItem.cpp/h           # Telegram-style contact list item with status dots
│   ├── TopBarView.cpp/h            # Unified top bar (hamburger menu + status indicators)
│   ├── ContactInfoPanel.cpp/h     # Right-side contact detail panel
│   ├── SettingsWindow.cpp/h        # Device & Radio settings (12 presets)
│   ├── StatsWindow.cpp/h           # Device statistics display
│   ├── MapView.cpp/h               # Geographic map with zoom/pan/grid
│   ├── NetworkMapWindow.cpp/h      # Force-directed network topology
│   ├── TelemetryWindow.cpp/h       # Sensor dashboard with graphs + CSV
│   ├── TracePathWindow.cpp/h       # Route tracing visualization
│   ├── LoginWindow.cpp/h           # Repeater/Room authentication
│   ├── ContactExportWindow.cpp/h   # Contact import/export via clipboard
│   ├── DebugLogWindow.cpp/h        # Raw protocol debug log
│   ├── NotificationManager.cpp/h   # Desktop notifications
│   ├── MqttClient.cpp/h            # MQTT bridge integration
│   ├── MqttSettingsWindow.cpp/h    # MQTT connection settings
│   ├── Types.h                     # Protocol structures & radio presets
│   └── Constants.h                 # Application constants
└── haiku-patches/                  # USB driver patches
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Sestriere (BApplication)          │
└─────────────────────────────────────────────────────┘
                          │
    ┌─────────────────────┴─────────────────────┐
    │                                           │
    ▼                                           ▼
┌──────────────────┐                  ┌─────────────────────┐
│  MainWindow      │◄─── BMessage ───►│  SerialHandler      │
│  (BWindow)       │                  │  (BLooper + Thread) │
└──────────────────┘                  └─────────────────────┘
        │                                       │
        ├─ TopBarView (menu + status)            ▼
        ├─ ContactList (sidebar)      ┌─────────────────────┐
        ├─ ChatView (message bubbles) │  POSIX Serial       │
        ├─ ContactInfoPanel (right)   │  (DTR/RTS enabled)  │
        │                             └─────────────────────┘
        ├─ SettingsWindow                       │
        ├─ TelemetryWindow                      ▼
        ├─ MapWindow (Geographic)     ┌─────────────────────┐
        ├─ NetworkMapWindow (Graph)   │ MeshCore Device     │
        ├─ LoginWindow                │ (Heltec, T-Deck)    │
        ├─ ContactExportWindow        └─────────────────────┘
        ├─ StatsWindow
        ├─ TracePathWindow
        └─ MqttClient (bridge)
```

### Child Window Lifecycle

All child windows (SettingsWindow, TelemetryWindow, LoginWindow, MapWindow, ContactExportWindow, etc.) follow the Haiku pattern:

- **QuitRequested()** returns `false` and calls `Hide()` — the window is never destroyed while MainWindow holds a pointer to it
- **MainWindow** creates windows on first use and reuses them via `Show()`/`Activate()`
- **MainWindow::QuitRequested()** calls `Lock()` + `Quit()` on all child windows before exiting

This prevents use-after-free crashes from dangling window pointers.

## License

MIT License — See LICENSE file for details.

## Acknowledgments

- [MeshCore](https://github.com/meshcore-dev/MeshCore) — The mesh networking firmware
- [Haiku OS](https://www.haiku-os.org/) — The operating system
- [Haiku API](https://api.haiku-os.org/) — Native API documentation

## References

- [MeshCore Companion Radio Protocol](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol)
- [Haiku Coding Guidelines](https://www.haiku-os.org/development/coding-guidelines/)
