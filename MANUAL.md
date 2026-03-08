# SESTRIERE — User Manual

**MeshCore Companion for Haiku OS**
Version 1.8.0 | March 2026

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Getting Started](#2-getting-started)
3. [The Main Interface](#3-the-main-interface)
4. [Connecting to a Device](#4-connecting-to-a-device)
5. [Contacts](#5-contacts)
6. [Messaging](#6-messaging)
7. [Channels](#7-channels)
8. [Muting Contacts and Channels](#8-muting-contacts-and-channels)
9. [Contact Groups](#9-contact-groups)
10. [Network Map](#10-network-map)
11. [Geographic Map and GPX Export](#11-geographic-map-and-gpx-export)
12. [Statistics and Telemetry](#12-statistics-and-telemetry)
13. [Packet Analyzer](#13-packet-analyzer)
14. [Mission Control Dashboard](#14-mission-control-dashboard)
15. [MQTT Integration](#15-mqtt-integration)
16. [Device Administration](#16-device-administration)
17. [Serial Monitor](#17-serial-monitor)
18. [Repeater Monitor](#18-repeater-monitor)
19. [Profile Export and Import](#19-profile-export-and-import)
20. [GIF Sharing](#20-gif-sharing)
21. [Image Sharing](#21-image-sharing)
22. [SAR Markers](#22-sar-markers)
23. [Radio Configuration](#23-radio-configuration)
24. [Notifications and Deskbar](#24-notifications-and-deskbar)
25. [Keyboard Shortcuts](#25-keyboard-shortcuts)
26. [Troubleshooting](#26-troubleshooting)
- [Appendix A: Radio Presets Reference](#appendix-a-radio-presets-reference)
- [Appendix B: MeshCore Protocol Reference](#appendix-b-meshcore-protocol-reference)

---

## 1. Introduction

Sestriere is a native Haiku OS companion application for MeshCore LoRa mesh radios. It provides a Telegram-style interface for managing contacts, sending messages, monitoring network health, and configuring radio devices over a serial connection.

MeshCore is an open-source firmware for LoRa radios that creates multi-hop mesh networks. Nodes can be **chat devices** (companions), **repeaters** (relays), or **rooms** (group servers). Sestriere connects to a companion node via USB serial and acts as the full user interface for the mesh.

### Key Features

- **Real-time messaging** — direct messages and public/private channels
- **GIF sharing** — GIPHY-powered animated GIF picker, cross-compatible with meshcore-open
- **Image sharing** — LoRa chunked image transfer with auto-fetch
- **Emoji rendering** — Unicode emoji displayed as PNG sprites
- **Voice messages** — Push-to-talk voice messages via Codec2, compatible with meshcore-sar
- **SAR markers** — Search and rescue marker display in chat and map
- **Contact management** — groups, mute controls, People files integration
- **Network topology visualization** — live signal quality and hop paths
- **Geographic map** — GPS node positions with OSM tiles and GPX export
- **Device statistics** — battery, signal, packets, uptime
- **Sensor telemetry** — temperature, humidity, pressure charts
- **Packet analyzer** — raw protocol capture with hex dump
- **Mission Control** — unified dashboard for network overview
- **MQTT bridge** — cloud integration and remote monitoring
- **Remote administration** — manage repeaters and rooms
- **Profile export/import** — bulk JSON configuration transfer
- **Native Haiku integration** — Deskbar tray, People files, desktop notifications

### System Requirements

- Haiku OS R1 Beta 5 or later
- MeshCore-compatible LoRa radio with USB serial (Heltec, LILYGO, etc.)
- USB serial driver (CP210x, CH340, or FTDI)

---

## 2. Getting Started

### Installation

Copy the Sestriere application to your preferred location (e.g., `/boot/home/apps/`). The application is self-contained and creates its settings directory automatically.

### First Launch

When Sestriere starts for the first time, it will:

1. Create a settings directory at `~/config/settings/Sestriere/`
2. Initialize the SQLite database for message history, mute settings, contact groups, signal history, and telemetry data
3. Scan for available serial ports
4. Load contacts from Haiku People files (if any exist)

### Quick Start

1. Connect your MeshCore radio to a USB port
2. Launch Sestriere
3. Press **Cmd+O** (or Connection > Connect)
4. Select the serial port for your radio
5. Click **Connect** — the status bar turns green when connected
6. Sestriere automatically syncs contacts and loads message history
7. Select a contact from the sidebar and start messaging

---

## 3. The Main Interface

Sestriere uses a three-panel layout inspired by Telegram Desktop:

```
+------------------+---------------------------+------------------+
|    TOP BAR — status, battery, signal, quick-access icons        |
+------------------+---------------------------+------------------+
|                  |                           |                  |
|  SIDEBAR         |  CHAT AREA                |  INFO PANEL      |
|                  |                           |                  |
|  Search field    |  Contact header           |  Avatar & name   |
|  Public Channel  |  (name, path, signal)     |  Type & status   |
|  Private ch.     |                           |  Last seen       |
|  Contacts...     |  Message list             |  Public key      |
|                  |  (scrollable)             |  Hop count       |
|  [grouped by     |                           |  GPS coords      |
|   contact group] |                           |  SNR chart       |
|                  |  Input area               |  Admin controls  |
|  Device info     |  [text field] [Send]      |  (if logged in)  |
|                  |  Character counter        |                  |
+------------------+---------------------------+------------------+
```

### Top Bar

The top bar provides at-a-glance device status:

| Element | Description |
|---------|-------------|
| Connection dot | Green = connected, Red = disconnected |
| Port name | Serial port in use (e.g., `/dev/ports/usb0`) |
| Battery | Voltage in mV, color-coded: green > 3.9V, yellow > 3.6V, orange > 3.4V, red < 3.4V |
| RSSI | Signal strength in dBm |
| SNR | Signal-to-noise ratio in dB |
| TX / RX | Packet counters |
| Uptime | Device uptime |
| MQTT | Connection status indicator (click to toggle) |

Quick-access icons open tool windows: Network Map, Geographic Map, Stats, Telemetry, Packet Analyzer, Debug Log, Mission Control, MQTT Log.

### Left Panel: Contact Sidebar

The sidebar lists all contacts and channels. The **Public Channel** is always first, followed by private channels and contacts.

Each contact item shows:
- **Avatar** with colored initials (Telegram-style palette)
- **Name** in bold (dimmed if muted)
- **Status dot**: green (online, < 5 min), gold (recent, < 1 hr), gray (offline)
- **Type badge**: `[R]` for repeater, `[S]` for room
- **Last message** preview
- **Timestamp** of last message
- **Unread badge** with count (hidden if muted)

The **search field** at the top filters contacts and group names by text.

### Center Panel: Chat Area

- **Contact header**: selected contact's name, path length, and signal quality
- **Message list**: scrollable conversation history
- **Search bar**: press Cmd+F to search all stored messages
- **Input area**: auto-growing text field with character counter (160 for DM, 200 for channels)
- **Img button**: send an image via LoRa (chunked transfer)
- **GIF button**: open the GIPHY animated GIF picker
- **Send button**: send the message (or press Enter)

### Right Panel: Contact Info

Detailed information about the selected contact:
- Avatar, name, and node type
- Last seen (relative time)
- Public key prefix (12 hex characters)
- Hop count and path quality
- GPS coordinates (if available)
- SNR quality chart (recent signal history)
- Admin controls (when logged into a repeater/room)

Toggle panels with **Cmd+B** (sidebar) and **Cmd+I** (info panel).

---

## 4. Connecting to a Device

### Port Selection

Sestriere scans for serial devices under `/dev/ports/`. Common port names:

| Driver | Typical Port |
|--------|-------------|
| CP210x (Heltec) | `/dev/ports/usb0` |
| CH340 | `/dev/ports/usb_serial` |
| FTDI | `/dev/ports/usb0` |

Use **Connection > Refresh Ports** if your radio is not listed after plugging in.

### Connection Sequence

Upon successful connection, Sestriere automatically:

1. Sends `CMD_APP_START` to initialize the MeshCore companion protocol
2. Queries device info: name, firmware version, board type, public key
3. Downloads the contact list from the device
4. Enumerates private channel configurations
5. Loads message history from the local SQLite database
6. Syncs device time
7. Starts periodic stats refresh

### Disconnecting

Use **Connection > Disconnect** (Cmd+D) or close the application. Data is persisted automatically.

---

## 5. Contacts

### Syncing Contacts

**Device > Sync Contacts** (Cmd+R) downloads the full contact list. Each contact includes:
- 32-byte public key
- Display name (up to 32 characters)
- Node type: Chat (1), Repeater (2), Room (3)
- Flags and path length
- Last seen timestamp
- GPS latitude and longitude

Contacts are saved locally and also exported as Haiku People files under `~/people/MeshCore/`.

### Contact Types

| Type | Badge | Description |
|------|-------|-------------|
| Chat device | — | Standard companion node for messaging |
| Repeater | `[R]` | Relay node that forwards packets between nodes |
| Room | `[S]` | Server node that stores messages for offline retrieval |

### Context Menu Actions

Right-click any contact in the sidebar:

| Action | Description |
|--------|-------------|
| **Ping** | Measure round-trip latency via trace path |
| **Reset Path** | Clear routing history to force route re-discovery |
| **Mute / Unmute** | Silence notifications and badges (see Ch. 8) |
| **Group** | Assign to a contact group (see Ch. 9) |
| **Remove Contact** | Delete from device (with confirmation) |

### Export and Import

- **Contacts > Export Contact**: creates an encrypted data block you can share
- **Contacts > Import Contact**: paste a data block to add a contact to your device

### People Files Integration

Sestriere saves contacts as Haiku People files with standard and MeshCore-specific attributes:

| Attribute | Content |
|-----------|---------|
| `META:name` | Contact name |
| `META:nickname` | `MC-` + hex key prefix |
| `META:group` | "MeshCore" |
| `META:url` | `meshcore://` + hex key |
| `MESHCORE:pubkey` | Full 64-char hex public key |
| `MESHCORE:type` | Node type (1, 2, or 3) |
| `MESHCORE:lastseen` | Last seen timestamp |

---

## 6. Messaging

### Direct Messages

1. Select a contact from the sidebar
2. Type your message in the input field (max 160 characters)
3. Press **Enter** or click **Send**

Messages display with delivery status:

| Status | Meaning |
|--------|---------|
| Pending | Waiting for radio to transmit |
| Sent | Radio has transmitted the frame |
| Confirmed | Recipient ACK received (round-trip time shown) |

### Channel Messages

Select the Public Channel or a private channel. Channel messages are broadcast to all nodes with the matching channel key. Limit: 200 characters.

### Message Search

Press **Cmd+F** to open the search bar. Type a query to search all messages across all contacts in the database. Results appear in the chat area. Press **Escape** to close and return to the current conversation.

### Message Persistence

All messages are stored in the local SQLite database (`sestriere.db`). History is loaded automatically when syncing contacts. Messages survive application restarts.

---

## 7. Channels

### Public Channel

The Public Channel is always the first item in the sidebar. Messages are broadcast unencrypted to all nodes within range. It cannot be removed but can be muted.

### Private Channels

Private channels provide encrypted group communication using a pre-shared key (PSK). Each device supports up to 16 channel slots.

**Creating a channel:**

1. Right-click the Public Channel
2. Select **Add Channel...**
3. Enter a channel name
4. A PSK is automatically derived from the name
5. Share the same channel name with other users

> **Note**: All users must use the exact same channel name to generate matching PSKs.

**Removing a channel:**

1. Right-click the private channel
2. Select **Remove Channel**
3. Confirm the deletion

---

## 8. Muting Contacts and Channels

Muting silences a contact or channel without removing it. Muted items:

- Do **not** trigger desktop notifications
- Do **not** show unread message badges
- Display the name in **dimmed color** in the sidebar
- Continue to **receive and store** messages normally

### How to Mute

Right-click any contact or channel in the sidebar and select **Mute** (or **Mute Channel** for the public channel).

### How to Unmute

Right-click the muted item and select **Unmute** (or **Unmute Channel**).

### Mute Keys

Mute state is stored in the SQLite database per item:

| Item | Key Format |
|------|-----------|
| Contact | 12-char lowercase hex (e.g., `aabbccddeeff`) |
| Public Channel | `ch_public` |
| Private Channel N | `ch_N` (e.g., `ch_1`, `ch_5`) |

Mute settings persist across application restarts.

---

## 9. Contact Groups

Contact groups organize your sidebar when the network grows.

### How Groups Work

- Each contact can belong to **one group** at a time
- Groups appear as **separator headers** in the sidebar
- Contacts not in any group appear under **"Ungrouped"** (only shown when groups exist)
- The search filter matches both contact names and group names

### Creating a Group

1. Right-click a contact
2. Open the **Group** submenu
3. Select **New Group...**
4. A group is created (auto-named "Group 1", "Group 2", etc.)
5. The contact is automatically added to it

### Adding to a Group

1. Right-click the contact
2. Open the **Group** submenu
3. Click the desired group name (a checkmark shows the current group)
4. Selecting a different group **moves** the contact (one group per contact)

### Removing from a Group

1. Right-click the contact
2. Open the **Group** submenu
3. Select **Remove from Group**

### Deleting a Group

1. Right-click any contact in the group
2. Open the **Group** submenu
3. Select **Delete Group**
4. All members become ungrouped (contacts are not deleted)

Groups are stored in SQLite tables (`contact_groups`, `contact_group_members`) and persist across restarts.

---

## 10. Network Map

**View > Network Map** (Cmd+M)

The Network Map shows a force-directed topology visualization of your mesh network.

### Features

- All contacts displayed as **animated nodes** with type-colored circles
- **Links** between nodes colored by signal quality (green = excellent, yellow = fair, red = poor)
- **Line thickness** proportional to link quality (thicker = better SNR)
- **SNR labels** displayed as color-coded pills at connection midpoints
- **Animated flow dots** along connections for online/recent nodes
- **Pulse animation** when a node sends or receives a message
- **Trace route visualization** with multi-hop paths and per-hop SNR
- **Topology edge persistence** — discovered edges saved to SQLite with 30-day expiry
- **Contact name resolution** — nodes show contact names instead of hex prefixes
- **Repeater node merging** — companion's own repeater merged with center "Me" node
- **Resizable to fullscreen** — window can be maximized to full screen width

### Topology Discovery

Click **Map Network** to run a full mesh topology discovery. Sestriere traces all known nodes and builds a complete map of how they connect.

- **Auto-Trace** checkbox enables periodic background trace routes (every 30 seconds) for selected or multi-hop nodes
- Discovered links persist in the database and are reloaded on next session
- Links older than 30 days are automatically pruned

### Interaction

- Right-click a node for: **Open Chat**, **Trace Path**, **Node Info**
- Click a node to see SNR, RSSI, and path info in the info panel
- Watch live link quality updates as messages flow through the network
- Track path changes and signal degradation over time

### Legend

The bottom-left corner shows a link quality legend with colored line samples:
- **Green** — Excellent (SNR > 5 dB)
- **Yellow-green** — Good (0 to 5 dB)
- **Yellow** — Fair (-5 to 0 dB)
- **Orange** — Poor (-10 to -5 dB)
- **Red** — Bad (< -10 dB)

---

## 11. Geographic Map and GPX Export

### Geographic Map

**View > Geographic Map** (Cmd+G)

Displays contacts at their GPS coordinates on a real map with OSM tiles.

- **Zoom levels Z2-Z18** matching Google Maps / OSM standard (each step doubles the scale)
- **Pan** by dragging
- **Click** a node for info popup
- **OSM tile overlay** with checkbox toggle (offline coastline fallback when disabled)
- **Tile cache**: 50 MB disk limit with automatic LRU eviction (oldest tiles deleted first)
- **Cache stats overlay**: bottom-right corner shows current zoom level, tile count, and disk usage
- Scale bar and compass for orientation
- Self position shown when latitude/longitude configured

### GPX Export

**Contacts > Export GPX** saves all contacts with GPS coordinates as a standard GPX file.

The file includes for each waypoint:
- Name
- Node type
- Last seen time
- Public key prefix

GPX files can be opened in Google Earth, QGIS, OsmAnd, or any mapping tool.

---

## 12. Statistics and Telemetry

### Statistics Window

**View > Statistics** (Cmd+S)

| Category | Metrics |
|----------|---------|
| Core | Uptime, battery voltage and percentage |
| Radio | Noise floor, RSSI, SNR, TX/RX air time % |
| Packets | Total RX/TX, flood vs. direct breakdown |

All values are color-coded: green (good), yellow (fair), orange (poor), red (critical).

Use the **Refresh** button to query the device for updated stats.

### Telemetry Window

**View > Sensor Telemetry** (Cmd+Y)

Displays sensor readings collected from remote nodes:

- **Time range selector**: 1 min, 5 min, 15 min, 1 hr, 6 hr, 24 hr, 7 days
- **Graph view** with trend lines per sensor
- **Sensor list** with current value, min, max, average
- **Request All** button polls all contacts for telemetry data

Supported sensor types include temperature, humidity, barometric pressure, and custom sensors via CayenneLPP format.

---

## 13. Packet Analyzer

**View > Packet Analyzer** (Cmd+Shift+P)

Real-time capture and analysis of raw MeshCore protocol frames.

### Capture

Click **Start Capture** to begin recording. The packet list shows:

| Column | Description |
|--------|-------------|
| Index | Sequential packet number |
| Time | Absolute timestamp |
| Delta | Time since last packet |
| Type | Packet type (RSP_CONTACT, PUSH_ADVERT, etc.) |
| Source | Sending node name and key prefix |
| SNR | Signal quality |
| Size | Payload size in bytes |
| Summary | Human-readable decoded summary |

### Analysis

- Click a packet for **detailed hex dump** and decoded fields
- **Filter** by packet type from the filter menu
- **Search** by text across all captured packets
- View **SNR trend** of recent packets
- **Per-contact statistics**: packets per node

### Export

**Export CSV** saves all captured packets for offline analysis in spreadsheet software.

---

## 14. Mission Control Dashboard

**View > Mission Control** (Cmd+Shift+D)

A unified dashboard providing a complete network overview:

| Card | Content |
|------|---------|
| **Device** | Name, firmware version, connection status |
| **Radio** | Frequency, spreading factor, bandwidth, TX power |
| **Health** | Overall health score (percentage) with alert banners |
| **Contacts** | Online / recently seen / offline node counts |
| **SNR Trend** | Signal quality chart over time |
| **Packet Rate** | TX/RX activity chart |
| **Topology** | Mini network map showing nearest nodes |
| **Timeline** | Session event log (connect, messages, errors) |

**Quick actions**: Send Advert, Sync Contacts, Get Stats.

---

## 15. MQTT Integration

Sestriere can bridge your mesh to an MQTT broker for cloud integration.

### Configuration

**Settings > MQTT Settings**:

| Setting | Description |
|---------|-------------|
| Broker | MQTT broker hostname or IP |
| Port | TCP port (default: 1883) |
| Username | Broker authentication |
| Password | Broker authentication |
| IATA Code | Location identifier (e.g., VCE, FCO) |
| Latitude / Longitude | GPS coordinates for publishing |

### Enabling MQTT

Click the **MQTT indicator** in the top bar to toggle connection. MQTT is initialized lazily when first enabled.

### Published Data

When connected, Sestriere publishes:
- Device status: name, battery, uptime, noise floor
- Packet activity: type, source, SNR, RSSI, payload
- Location data (if configured)

### MQTT Log

**View > MQTT Log** (Cmd+Shift+M) shows real-time publish/subscribe activity with filters for message type (Connect, Publish, Error, Reconnect).

---

## 16. Device Administration

Sestriere supports remote administration of repeater and room nodes.

### Logging In

1. Select a repeater `[R]` or room `[S]` contact
2. Go to **Device > Login to Repeater/Room...**
3. Enter the admin password
4. Wait for authentication (timeout: 10 seconds)
5. On success, admin controls appear in the right info panel

### Admin Controls

#### Query Tab
- **Version** — query firmware version and board type
- **Neighbors** — list adjacent nodes in radio range
- **Clock** — synchronize device time
- **Clear Stats** — reset packet counters

#### Config Tab
- **Set Name** — change the device's advertised name
- **Set Password** — change the admin password

#### Actions Tab
- **Reboot** — restart the device (settings preserved)
- **Factory Reset** — erase all settings and contacts (requires confirmation)

### CLI Commands

When logged in, you can type CLI commands directly in the chat input. These are sent as `TXT_TYPE_CLI_DATA` to the device's admin interface. Responses appear in the chat view.

---

## 17. Serial Monitor

**Connection > Serial Monitor**

The Serial Monitor provides a terminal-style interface for direct CLI interaction with repeater or standalone devices that are not running the companion firmware.

### When to Use

When connecting to a device, Sestriere performs a handshake to detect whether the device speaks the MeshCore Companion Protocol. If the handshake times out, the device is likely a repeater with only a serial CLI console. In this case, Sestriere offers to open the Serial Monitor instead.

### Features

- **Terminal output** — Scrollable monospace text view showing device serial output
- **Command input** — Text field at the bottom for sending CLI commands
- **Save log** — Export the entire session log to a text file via save dialog
- **Clear** — Clear the terminal output
- **Auto-pruning** — Output limited to 512 KB to prevent memory issues

### Usage

1. Connect to a repeater or standalone device
2. If the companion handshake fails, accept the Serial Monitor prompt
3. Type CLI commands in the input field and press Enter or click Send
4. View device output in the scrollable terminal area

---

## 18. Repeater Monitor

**View > Repeater Monitor**

The Repeater Monitor provides structured analysis of repeater log output, automatically parsing packet entries into a sortable table.

### Auto-Detection

When connected via Serial Monitor to a device that outputs MeshCore repeater log lines, the Repeater Monitor activates automatically and begins parsing the log.

### Features

- **Packet table** — Sortable columns: Time, Direction (TX/RX), Source, Destination, Route, SNR, RSSI, Summary
- **Per-node statistics** — Separate table showing per-node metrics: packet count, average SNR/RSSI, TX/RX breakdown
- **Auto role detection** — Nodes classified as direct or forwarded based on route field
- **SNR/RSSI graph** — Visual signal quality over time
- **Text search** — Filter packets by node name or content
- **Network Map integration** — Topology data (nodes and links) extracted from log and visualized on the Network Map

### Topology Extraction

The Repeater Monitor tracks directional links between nodes (who talks to whom) and feeds this data to the Network Map for visualization. Each observed packet creates or updates a link with SNR information.

---

## 19. Profile Export and Import

**Contacts > Export/Import Profile**

Bulk configuration transfer using JSON format.

### Exporting

1. Select sections to export: Contacts, Channels, Radio, MQTT
2. Optionally include the MQTT password
3. Click Browse and choose a save location
4. Click Export

### Importing

1. Click Browse and select a JSON profile file
2. Preview shows file contents
3. Select sections to import
4. Click Import — settings are applied to the device

### JSON Schema

```json
{
  "device": { "name": "...", "pubkey": "...", "firmware": "..." },
  "contacts": [
    { "pubkey": "...", "name": "...", "type": 1, "lat": 0, "lon": 0 }
  ],
  "channels": [
    { "index": 0, "name": "...", "secret": "..." }
  ],
  "radio": {
    "frequency": 906875000, "bandwidth": 250000,
    "sf": 11, "cr": 5, "tx_power": 20
  },
  "mqtt": {
    "broker": "...", "port": 1883,
    "username": "...", "password": "...",
    "lat": 45.0, "lon": 7.0, "iata": "TRN"
  }
}
```

---

## 20. GIF Sharing

Sestriere supports animated GIF sharing via GIPHY, fully compatible with meshcore-open.

### How It Works

GIF messages use the compact format `g:{gifId}` — only the short GIPHY identifier is transmitted over LoRa, keeping airtime minimal. The receiving client reconstructs the full GIF URL and downloads it from GIPHY's CDN.

### Sending a GIF

1. Click the **GIF** button next to the message input
2. The GIF Picker window opens with **trending GIFs** (animated thumbnails)
3. Type a search query and press Enter to find specific GIFs
4. Double-click a GIF or select it and click **Send**
5. The message `g:{id}` is sent to the current contact or channel

### Receiving a GIF

When a `g:ID` message is received (from Sestriere or meshcore-open), the GIF is automatically:
1. Downloaded from GIPHY's CDN
2. Decoded into animation frames
3. Displayed as an animated bubble in the chat with a "GIF" badge
4. Cached locally for instant re-display

### GIF Cache

Downloaded GIFs are cached in `~/config/settings/Sestriere/gif_cache/`. Files are small (typically 1-5 MB total) and not automatically pruned.

---

## 21. Image Sharing

Sestriere supports LoRa image transfer using chunked encoding, compatible with meshcore image sharing.

### How It Works

Images are compressed to **color WebP** format at 192px max dimension (quality 50), producing compact files (~1.5-3 KB) that are split into LoRa-sized fragments. WebP provides ~30% smaller files than JPEG at equivalent quality, enabling color images with fewer fragments than the old grayscale format.

Images and GIFs in the chat are automatically scaled to fit within 250x300 pixels, preserving aspect ratio.

### Sending an Image

1. Click the **Img** button next to the message input (or drag-and-drop an image onto the chat)
2. Select an image file via the file dialog
3. The image is compressed to color WebP and sent in chunks over LoRa
4. Progress is shown in the chat bubble

### Receiving an Image

Incoming image messages are automatically detected, fetched chunk by chunk, reassembled, and displayed inline in the chat as a scaled bitmap. Both WebP and legacy JPEG images are supported (auto-detected).

---

## 22. SAR Markers

Sestriere can parse and display Search and Rescue (SAR) markers from the meshcore-sar protocol.

### In Chat

SAR marker messages are displayed as special bubbles showing the marker type, coordinates, and description.

### On Geographic Map

SAR markers with GPS coordinates are plotted on the Geographic Map with distinctive icons, making them visible alongside regular contact nodes.

---

## 23. Radio Configuration

**Settings > Device & Radio**

### Parameters

| Parameter | Description | Range |
|-----------|-------------|-------|
| Frequency | Center frequency | Region-dependent (433/868/915 MHz) |
| Bandwidth | Channel width | 62.5, 125, 250, or 500 kHz |
| Spreading Factor | Chirp rate | SF7–SF12 |
| Coding Rate | Error correction | CR5–CR8 |
| TX Power | Transmit power | 0–20 dBm |

### Trade-offs

| Change | Range | Speed | Battery | Airtime |
|--------|-------|-------|---------|---------|
| Higher SF | Longer | Slower | More | More |
| Higher BW | Shorter | Faster | Same | Less |
| Higher CR | Same | Slower | Same | More |
| Higher TX Power | Longer | Same | More | Same |

> **EU 868 MHz users**: the band has a 1% duty cycle limit enforced by law. Minimize airtime by using lower SF and higher BW when possible.

### Presets

Select from 11 pre-configured presets in the settings dialog. See Appendix A.

---

## 24. Notifications and Deskbar

### Desktop Notifications

When the Sestriere window is not active (in the background), incoming messages trigger **Haiku desktop notifications** showing:
- Sender name
- Message preview
- Whether it's a DM or channel message

Notifications are **suppressed** for muted contacts and channels.

### Deskbar Replicant

Enable the tray icon via **Settings > Show in Deskbar**.

The Deskbar icon shows:
- **Connection status**: green (connected), gray (disconnected)
- **Battery level** indicator bar
- **Unread count** badge

**Click** the icon to bring Sestriere to front. **Right-click** for a menu:
- Connection status (informational)
- Open Sestriere
- Remove from Deskbar

Remove via **Settings > Remove from Deskbar**.

---

## 25. Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Cmd+O | Connect to serial port |
| Cmd+D | Disconnect from device |
| Cmd+Q | Quit application |
| Cmd+R | Sync contacts from device |
| Cmd+A | Send self advertisement |
| Cmd+T | Trace path to selected contact |
| Cmd+F | Toggle message search bar |
| Cmd+B | Toggle contact list sidebar |
| Cmd+I | Toggle info panel |
| Cmd+L | Show debug log window |
| Cmd+S | Show statistics window |
| Cmd+M | Show network map |
| Cmd+G | Show geographic map |
| Cmd+Y | Show sensor telemetry |
| Cmd+Shift+P | Show packet analyzer |
| Cmd+Shift+D | Show Mission Control |
| Cmd+Shift+M | Show MQTT log |
| Enter | Send message |
| Escape | Close search bar |

---

## Appendix A: Radio Presets Reference

| Preset | Frequency (MHz) | Bandwidth (kHz) | SF | CR | Best For |
|--------|----------------|-----------------|----|----|----------|
| MeshCore Default | 906.875 | 250 | 11 | 5 | General US/CA use |
| USA/Canada Narrow | 910.525 | 62.5 | 7 | 5 | Urban, fast |
| USA Fast | 906.875 | 500 | 7 | 5 | High throughput |
| EU/UK 868 Narrow | 869.618 | 62.5 | 8 | 8 | Long range EU |
| EU 868 Wide | 868.000 | 125 | 9 | 8 | General EU use |
| EU 433 MHz | 433.875 | 125 | 9 | 5 | EU sub-GHz band |
| ANZ 915 | 915.000 | 125 | 9 | 8 | Australia/NZ |
| NZ 915 | 915.000 | 250 | 11 | 5 | NZ general |
| Long Range | 868.000 | 62.5 | 12 | 8 | Maximum range |
| Medium Range | 868.000 | 125 | 10 | 5 | Balanced range/speed |
| Fast | 868.000 | 250 | 7 | 5 | Maximum speed |

---

## Appendix B: MeshCore Protocol Reference

### Inbound Commands (App -> Radio)

| Code | Command | Purpose |
|------|---------|---------|
| 1 | CMD_APP_START | Initialize companion protocol |
| 2 | CMD_SEND_TXT_MSG | Send direct message |
| 3 | CMD_SEND_CHANNEL_TXT_MSG | Send channel message |
| 4 | CMD_GET_CONTACTS | Sync contact list |
| 5 | CMD_GET_DEVICE_TIME | Query device clock |
| 6 | CMD_SET_DEVICE_TIME | Set device clock |
| 7 | CMD_SEND_SELF_ADVERT | Broadcast presence |
| 8 | CMD_SET_ADVERT_NAME | Change advertised name |
| 9 | CMD_ADD_UPDATE_CONTACT | Add or update contact |
| 10 | CMD_SYNC_NEXT_MESSAGE | Fetch queued message |
| 11 | CMD_SET_RADIO_PARAMS | Configure frequency/BW/SF/CR |
| 12 | CMD_SET_RADIO_TX_POWER | Set transmit power |
| 13 | CMD_RESET_PATH | Clear routing history |
| 14 | CMD_SET_ADVERT_LATLON | Set GPS position |
| 15 | CMD_REMOVE_CONTACT | Delete contact |
| 16 | CMD_SHARE_CONTACT | Forward contact info |
| 17 | CMD_EXPORT_CONTACT | Prepare encrypted export |
| 18 | CMD_IMPORT_CONTACT | Import encrypted contact |
| 19 | CMD_REBOOT | Restart device |
| 20 | CMD_GET_BATT_AND_STORAGE | Battery + storage status |
| 22 | CMD_DEVICE_QUERY | Get device info |
| 26 | CMD_SEND_LOGIN | Authenticate to admin |
| 31 | CMD_GET_CHANNEL | Read channel config |
| 32 | CMD_SET_CHANNEL | Write channel config |
| 36 | CMD_SEND_TRACE_PATH | Trace route to node |
| 39 | CMD_SEND_TELEMETRY_REQ | Request sensor data |
| 51 | CMD_FACTORY_RESET | Erase all settings |
| 56 | CMD_GET_STATS | Fetch statistics |

### Outbound Responses (Radio -> App)

| Code | Response | Purpose |
|------|----------|---------|
| 0 | RSP_OK | Command acknowledged |
| 1 | RSP_ERR | Command failed |
| 3 | RSP_CONTACT | Contact data (148 bytes) |
| 5 | RSP_SELF_INFO | Device identity and radio params |
| 6 | RSP_SENT | Message transmitted |
| 7 | RSP_CONTACT_MSG_RECV | Incoming direct message (V2) |
| 8 | RSP_CHANNEL_MSG_RECV | Incoming channel message (V2) |
| 12 | RSP_BATT_AND_STORAGE | Battery voltage and storage |
| 16 | RSP_CONTACT_MSG_RECV_V3 | Incoming direct message (V3 with SNR) |
| 17 | RSP_CHANNEL_MSG_RECV_V3 | Incoming channel message (V3 with SNR) |
| 24 | RSP_STATS | Device statistics |

### Push Notifications (Radio -> App, unsolicited)

| Code | Push | Purpose |
|------|------|---------|
| 0x80 | PUSH_ADVERT | New node advertisement |
| 0x82 | PUSH_SEND_CONFIRMED | Delivery ACK received |
| 0x83 | PUSH_MSG_WAITING | Messages queued for sync |
| 0x85 | PUSH_LOGIN_SUCCESS | Admin login accepted |
| 0x86 | PUSH_LOGIN_FAIL | Admin login rejected |
| 0x89 | PUSH_TRACE_DATA | Trace path result |
| 0x8B | PUSH_TELEMETRY_RESPONSE | Sensor data received |

---

## Data Storage

Sestriere stores all persistent data in SQLite:

| Table | Content |
|-------|---------|
| `messages` | Chat message history (DM and channel) |
| `snr_history` | Signal quality data points per contact |
| `telemetry_history` | Sensor readings per node |
| `mute_settings` | Muted contact/channel flags |
| `contact_groups` | Group name definitions |
| `contact_group_members` | Contact-to-group assignments |
| `topology_edges` | Network map link persistence (30-day expiry) |
| `voice_clips` | Codec2 voice message audio data |

Database location: `~/config/settings/Sestriere/sestriere.db`

SQLite journal mode: WAL (preferred) with automatic fallback to DELETE on filesystems that do not support shared-memory locking.

MQTT settings: `~/config/settings/Sestriere/mqtt.settings`

Device settings: `~/config/settings/Sestriere/device.settings`

UI settings: `~/config/settings/Sestriere/ui.settings` (contact filter state)

GIF cache: `~/config/settings/Sestriere/gif_cache/`

Map tile cache: `~/config/settings/Sestriere/tiles/` (50 MB max, LRU eviction)

---

## 26. Troubleshooting

### Database: "locking protocol" error

```
[DatabaseManager] SQL error: locking protocol
[DatabaseManager] Failed to create tables
```

**Cause**: SQLite WAL (Write-Ahead Logging) journal mode requires shared-memory file locking that may not work on all Haiku filesystem configurations. When WAL mode fails, the database cannot initialize.

**Solution**: As of version 1.8.0, Sestriere automatically detects this condition and falls back to DELETE journal mode, which works on all filesystems. If you see this error with version 1.7 or earlier, update to the latest version.

If the error persists on 1.8.0+, try deleting the database file and restarting:

```bash
rm ~/config/settings/Sestriere/sestriere.db
rm -f ~/config/settings/Sestriere/sestriere.db-wal
rm -f ~/config/settings/Sestriere/sestriere.db-shm
```

> **Note**: Deleting the database removes all message history, SNR data, contact groups, and mute settings.

### Serial port: "Device/File/Resource busy"

```
SerialHandler: Failed to open /dev/ports/usb0: Device/File/Resource busy
```

**Cause**: Another process is holding the serial port. This is usually a previous Sestriere instance that did not shut down cleanly, or another serial terminal application.

**Solution**:

1. Close any other serial communication programs (SerialConnect, minicom, etc.)
2. If a previous Sestriere instance is stuck, use ProcessController or `kill` to terminate it:
   ```bash
   kill $(pidof Sestriere)
   ```
3. If the port remains busy, disconnect and reconnect the USB cable
4. Try **Connection > Refresh Ports** to rescan available ports

### Application starts but shows no contacts

**Cause**: The device connection may have failed silently, or the contact sync did not complete.

**Solution**:

1. Check the connection status in the top bar (green dot = connected)
2. Try **Connection > Disconnect** (Cmd+D) then **Connection > Connect** (Cmd+O)
3. After connecting, press **Cmd+R** to force a contact sync
4. Check the Debug Log (Cmd+L) for protocol errors

### UI filters hide all contacts

If the sidebar appears empty but you are connected:

1. Check the filter checkboxes below the search field (Chat, Repeater, Room)
2. Make sure at least one type is checked
3. Clear the search field if it contains text
4. Filter settings persist across restarts — unchecked filters remain unchecked

### Testing without hardware

A radio simulator is included for testing without a physical LoRa device:

```bash
g++ -o fake_radio fake_radio.cpp -Wall -O2
./fake_radio
```

The simulator creates a virtual serial port (PTY) and prints the device path. Connect Sestriere to this path. The simulator handles the full V3 protocol handshake, creates a test contact with GPS coordinates, and sends a simulated message every 10 seconds with varying SNR values. Outgoing messages are acknowledged with delivery confirmation.

---

*Sestriere is open-source software created by Andrea Bernardi, distributed under the MIT license.*
*MeshCore: https://github.com/zjs81/meshcore-open*
