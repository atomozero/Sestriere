# Sestriere

> Native MeshCore LoRa mesh client for Haiku OS

## Overview

**Sestriere** is a 100% native Haiku OS application that serves as a MeshCore client, communicating with LoRa devices (Heltec v3.2, T-Deck, etc.) via USB serial.

The name recalls the Venetian *sestieri* – interconnected districts like nodes in a mesh network.

## Features

- **Native Haiku UI** — Built entirely with Haiku's native Be API (BApplication, BWindow, BView, etc.)
- **USB Serial Communication** — Connects to MeshCore companion firmware via BSerialPort
- **Contact Management** — View and manage mesh network contacts
- **Messaging** — Send and receive direct messages and channel messages
- **Device Status** — Monitor battery, radio parameters, and connection status

## Requirements

- Haiku OS R1/beta5 or later
- MeshCore-compatible LoRa device with USB Serial Companion firmware
- USB cable

## Building

### Using Makefile (recommended for standalone builds)

```bash
cd Sestriere
make
```

### Using Jamfile (for Haiku source tree integration)

```bash
jam -q Sestriere
```

## Supported Hardware

- Heltec LoRa32 v3/v3.2
- LilyGO T-Deck
- LilyGO T-Beam
- RAK WisBlock
- Other MeshCore-compatible devices

## Protocol

Sestriere implements the MeshCore USB Serial Companion protocol:

- Frame format: `[marker][len_lo][len_hi][payload...]`
- Inbound marker (App → Radio): `<` (0x3C)
- Outbound marker (Radio → App): `>` (0x3E)
- All multi-byte values are Little Endian

## Project Structure

```
Sestriere/
├── Jamfile                 # Haiku build system
├── Makefile                # Standard makefile
├── src/
│   ├── Sestriere.cpp/h     # BApplication main
│   ├── MainWindow.cpp/h    # Main window with layout
│   ├── SerialHandler.cpp/h # BLooper for serial I/O
│   ├── Protocol.cpp/h      # MeshCore protocol encoder/decoder
│   ├── ContactListView.cpp/h
│   ├── ContactItem.cpp/h
│   ├── ChatView.cpp/h
│   ├── MessageView.cpp/h
│   ├── StatusBarView.cpp/h
│   ├── SettingsWindow.cpp/h
│   ├── PortSelectionWindow.cpp/h
│   ├── Types.h             # Protocol structures
│   └── Constants.h         # Application constants
├── resources/
│   └── Sestriere.rdef      # Application resources
└── locales/                # Translations (future)
```

## License

MIT License — See LICENSE file for details.

## References

- [MeshCore Companion Radio Protocol](https://github.com/meshcore-dev/MeshCore/wiki/Companion-Radio-Protocol)
- [Haiku API Documentation](https://api.haiku-os.org/)
- [Haiku Coding Guidelines](https://www.haiku-os.org/development/coding-guidelines/)
