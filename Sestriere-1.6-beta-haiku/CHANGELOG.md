# Sestriere Changelog

## Version 1.6 beta (2026-03-01)

### New Features

#### Geographic Map — Real World Map
- **Offline Coastlines** — Simplified world coastline polygons always visible on the dark blue sea background, providing recognizable geography without any network connection
- **Web Mercator Projection** — Proper Mercator projection replacing the old equirectangular approximation, ensuring correct alignment with standard map tiles
- **OSM Online Tiles** — Optional OpenStreetMap raster tile overlay via "Online Map" checkbox in toolbar
- **Tile Cache** — Async tile fetching with disk persistence (~/.config/settings/Sestriere/tiles/) and in-memory LRU cache (max 100 tiles), cached tiles load instantly on reopen
- **Adaptive Drawing** — Grid dimmed when tiles active, coastlines switch to outline-only mode over tiles, land polygons filled when offline

### Improvements
- **Pan/Zoom** — Dragging and zoom-to-fit updated for Mercator math
- **Scale Bar** — Correct distance calculation with Mercator projection

---

## Version 1.5 beta (2026-02-28)

### New Features

#### Serial Monitor & Repeater Monitor
- **Serial Monitor** — Direct serial CLI interaction for repeater/standalone devices
- **Repeater Monitor** — Auto-detection of repeater log output with real-time topology extraction
- **Per-Node TX/RX Tracking** — Individual packet counters and auto role detection in Repeater Monitor

#### Network Map Topology Discovery
- **Map Network** — One-click full mesh topology discovery (traces all known nodes)
- **Auto-Trace** — Periodic background trace route for selected or multi-hop nodes
- **Topology Edge Persistence** — Discovered edges saved to SQLite with 30-day expiry
- **Packet Flow Animation** — Animated data flow dots along connections
- **Contact Name Resolution** — Nodes show contact names instead of hex prefixes
- **Repeater Node Merging** — Companion's own repeater merged with center "Me" node
- **Relay Node Clustering** — Nodes connected through repeaters detach from center and cluster near relay
- **Resizable Window** — Network Map can be resized to full screen width

#### Contact Management
- **Contact Groups** — Organize contacts into custom groups
- **Contact/Channel Muting** — Mute notifications per contact or channel
- **Profile Export/Import** — JSON-based profile backup and restore

#### Documentation
- **User Manual** — Comprehensive Markdown user manual
- **Development Roadmap** — Planned features and milestones

### Bug Fixes
- **Trace Data Parsing** — Fix 4-byte hop prefix format (was incorrectly parsed as 1-byte hashes)
- **Trace Frame Format** — Fix CMD_SEND_TRACE_PATH to use correct 7-byte format (radio was ignoring 16-byte format)
- **Discovery Focus Stealing** — Network Map no longer steals focus during background refresh
- **Handshake Timeout** — Detect non-companion devices and avoid hanging on connect

---

## Version 1.4 (2026-02-22)

### Code Quality & Stability

#### Architecture Refactoring
- **ProtocolHandler Extraction** — Protocol parsing separated from MainWindow into dedicated ProtocolHandler class for cleaner architecture
- **Utils.h Consolidation** — Common utilities extracted: FormatUptime, FormatTimeAgo, ParseHexPrefix, ParseHexPubKey, FormatContactKey
- **Named Constants** — All magic numbers replaced with named constants for protocol offsets, thresholds, colors, and buffer sizes
- **Dead Code Removal** — Removed unused MqttSettingsWindow, StatusBarView, and MSG_SHOW_SETTINGS

#### Security Fixes
- **Hardcoded Credentials Removed** — No more default credentials in source code
- **Input Validation** — All user inputs validated before protocol operations
- **Prepared SQL Statements** — All database queries use parameterized queries to prevent injection
- **Bounds Checks** — Protocol handler methods validate frame lengths before parsing

#### Thread Safety
- **DatabaseManager** — Thread-safe singleton with mutex-protected operations
- **Child Window Access** — New `_LockIfVisible()` helper ensures proper BLooper locking
- **MQTT Connection** — Race condition fix between connect/disconnect threads
- **BMessageRunner Lifecycle** — Always NULL after delete, prevents use-after-free

#### Bug Fixes
- **CMD_REMOVE_CONTACT** — Now sends full 32-byte pubkey (was 6-byte, causing error code 2)
- **CMD_RESET_PATH** — Now sends full 32-byte pubkey
- **CMD_SET_RADIO_PARAMS** — Frequency sent in kHz as protocol requires (was Hz)
- **Packet Analyzer** — V3 message text offsets corrected to match protocol spec
- **localtime() -> localtime_r()** — Fix non-reentrant call in multi-threaded context
- **strtok() -> strtok_r()** — Thread-safe string tokenization
- **LoginWindow** — Remove blocking snooze() from UI thread
- **MissionControlWindow** — Fix BMessageRunner memory leak
- **PacketAnalyzerWindow** — Add missing cleanup in QuitRequested

#### UX Polish
- **Centered Windows** — All windows use CenterOnScreen() or CenterIn(parent) instead of hardcoded positions
- **Text Truncation** — Mission Control cards truncate long labels/values gracefully in narrow windows
- **MQTT Password** — Show/hide toggle for password field in settings
- **Theme-Aware StatsWindow** — Uses system colors instead of hardcoded values
- **Responsive Layouts** — Reduced minimum sizes for Telemetry and Packet Analyzer windows
- **Unified Battery Calculation** — Consistent battery percentage formula across all views

---

## Version 1.3 (2026-02-19)

### New Features

#### Mission Control Dashboard (Cmd+Shift+D)
- **Unified Dashboard** — Single window consolidating device status, radio health, network overview, and activity feed
- **Device Status Card** — Battery percentage with storage gauge bar, uptime, firmware info, pulsing connection dot
- **Radio Health Card** — RSSI, SNR, noise floor, TX power, frequency, bandwidth with SF/CR display
- **Health Score Arc** — Composite 0-100 score from connection, battery, signal quality, and contacts
- **SNR/RSSI Dual Chart** — 200-point rolling line chart with color-coded quality zones and dual Y-axis
- **Packet Rate Histogram** — 60-bar TX/RX stacked bar chart with auto-scaling
- **Contact Heatmap Grid** — Per-contact SNR-based color coding with online status rings
- **Mini Network Topology** — Radial node layout with self at center, contacts in circle, SNR sparklines
- **Session Timeline** — Horizontal event timeline with color-coded markers since connection
- **Alert Banner** — Priority-based flashing alerts for disconnect, low battery, poor signal
- **Quick Actions** — Send Advert, Sync Contacts, Refresh Stats buttons
- **Activity Feed** — Timestamped event log with color-coded categories (MSG/ADV/SYS/ERR)
- **Last Update Footer** — Stale data detection (turns red after 30 seconds)
- **TopBarView Icon** — Dashboard icon in toolbar for quick access

#### Protocol Commands
- **CMD_ADD_UPDATE_CONTACT** — Add or update contact name/type
- **CMD_SHARE_CONTACT** — Share a contact with another node
- **CMD_GET_DEVICE_TIME / CMD_SET_DEVICE_TIME** — Read/write device RTC
- **CMD_GET_TUNING_PARAMS / CMD_SET_TUNING_PARAMS** — Read/write radio tuning
- **CMD_SEND_RAW_DATA / PUSH_RAW_DATA** — Raw binary data exchange
- **CMD_SEND_TRACE_PATH** — Explicit trace route request
- **CMD_SET_DEVICE_PIN** — Set device security PIN
- **CMD_GET_CUSTOM_VARS / CMD_SET_CUSTOM_VAR** — Custom variable management
- **CMD_GET_ADVERT_PATH / RSP_ADVERT_PATH** — Query advert routing path
- **CMD_SEND_BINARY_REQ** — Binary request to remote node
- **CMD_SEND_CONTROL_DATA** — Control data transmission

#### Admin Panel Improvements
- **Inline Admin in Info Panel** — Repeater/room administration via BTabView (Overview/CLI/Actions)
- **CLI Console** — Remote CLI commands: `ver`, `neighbors`, `clock`, `clear stats`, `set name`, `password`
- **Remote Telemetry** — Request and display sensor data from remote nodes

### Bug Fixes
- **Statistics Window** — Fix all V3 protocol fields: battery now displayed, noise floor parsed, SNR no longer divided by 4, packet stats correctly mapped to flood/direct categories
- **Admin Panel Layout** — Fix tabs overlapping with SNR chart

---

## Version 1.2 (2026-02-15)

### New Features

#### Repeater Admin in Info Panel
- **Inline Administration** — Repeater/Room device stats and controls displayed directly in the contact info panel (no separate window)
- **Remote Device Status** — Battery, uptime, radio stats fetched from the remote repeater via CMD_SEND_STATUS_REQ / PUSH_STATUS_RESPONSE
- **Action Buttons** — Reboot and Factory Reset buttons (with double confirmation) visible only on repeater/room contacts
- **Auto-Refresh** — Remote status polled every 15 seconds while logged in
- **Separated Data Sources** — Top bar shows local device stats, info panel shows remote repeater stats

#### Packet Analyzer Enhancements
- **Live SNR Trend Chart** — Rolling line chart with color-coded signal quality zones
- **Delta-t Timing Column** — Microsecond-precision inter-packet timing, color-coded for bursts and gaps
- **Contact Heatmap** — "Contact Stats" tab with per-contact packet count, average/min/max SNR, last seen
- **Color-Coded Packet Types** — Blue (messages), Green (adverts), Amber (alerts), Purple (raw/radio)
- **Decoded Packet Detail** — 27+ packet types with human-readable descriptions and hex dump
- **CSV Export** — Export captured packets via save dialog

#### Network Topology
- **Dynamic Link Quality** — Connection lines colored by SNR (green excellent, red bad)
- **Line Thickness** — Proportional to link quality
- **SNR Labels** — Color-coded pills at connection midpoints
- **Animated Flow Dots** — Visual data flow along connections
- **Trace Route Visualization** — Multi-hop paths with per-hop SNR
- **Auto-Trace** — Periodic trace route requests for selected nodes

#### CLI Console Mode
- **Repeater/Room Login** — CLI-style console for repeater administration
- **Auto-Open Login** — Login dialog opens automatically when selecting a repeater/room contact
- **Console Message Styling** — Distinct styling for CLI messages

#### Private Channels
- **Channel Management** — Create, join, and remove private encrypted channels
- **Offline Message Sync** — Automatic drain of queued messages on connect

#### UI Improvements
- **HVIF Toolbar Icons** — Native Haiku vector icons in the top bar
- **TX/RX Activity LEDs** — Visual mesh activity indicators
- **Input Character Counter** — Live count with color feedback near message limit
- **Delivery Checkmarks** — Real delivery confirmation status on sent messages
- **Contact Context Menu** — Right-click for Reset Path, Remove Contact
- **Sidebar Device Info** — Device name and firmware shown in sidebar footer
- **Message Copy** — Context menu to copy message text

### Bug Fixes
- Fix GPS visibility and radio frequency parsing from RSP_SELF_INFO
- Fix MQTT log window deadlock
- Fix packet rate always showing zero on startup
- Fix message bubble sizing and resize handling
- Remove verbose frame debug logging

---

## Version 1.1 (2026-02-04)

### New Features

#### MQTT Integration (meshcore-to-maps)
- **Broker Connection** — Connect to meshcoreitalia.it central broker for mesh network mapping
- **Status Publishing** — Automatic status updates every 60 seconds
- **Packet Reporting** — Report received packets (DM, Channel, Adverts) for network analysis
- **GPS Configuration** — Set latitude/longitude in MQTT Settings
- **IATA Code** — Configure regional airport code for topic routing
- **Status Indicator** — Real-time MQTT connection status in status bar (ON/OFF)

#### Network Map Window (View -> Network Map)
- **Interactive Visualization** — Circular layout of mesh network
- **Node Types** — Distinct shapes for different node types
- **Signal Indicators** — Color-coded signal strength per node
- **Online Status** — 5-minute timeout for online/offline detection

---

## Version 1.0 (2026-02-02)

### Initial Release
- Telegram-style UI with contact list and chat bubbles
- Direct messages and channel messaging
- Statistics window with real-time device stats
- Trace path visualization
- Debug log window
- Desktop notifications
- Message persistence
