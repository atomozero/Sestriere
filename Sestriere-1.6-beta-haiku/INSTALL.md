# Sestriere Installation

## Requirements

- Haiku OS (x86_64)
- MeshCore-compatible LoRa device (Heltec V3, T-Beam, etc.)
- USB connection to device

## Dependencies

Install required libraries:

```bash
pkgman install mosquitto sqlite
```

## Installation

1. Copy `Sestriere` to your preferred location (e.g., `/boot/home/apps/`)
2. Double-click to run, or execute from Terminal

## First Run

1. Connect your MeshCore device via USB
2. Launch Sestriere
3. File -> Connect (or Cmd+O)
4. Select your serial port (e.g., `/dev/ports/usb0`)
5. Wait for "Connected" in status bar
6. Device -> Sync Contacts to load your mesh contacts

## MQTT Setup (Optional)

To participate in meshcore-to-maps network mapping:

1. Settings -> MQTT tab
2. Check "Enable MQTT"
3. Enter your GPS coordinates
4. Set IATA code (regional airport code, e.g., VCE, FCO, MXP)
5. Click Apply

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Cmd+O | Connect |
| Cmd+D | Disconnect |
| Cmd+R | Sync Contacts |
| Cmd+B | Toggle Sidebar |
| Cmd+I | Toggle Info Panel |
| Cmd+F | Search Messages |
| Cmd+Y | Telemetry |
| Cmd+Shift+D | Mission Control |
| Cmd+Shift+P | Packet Analyzer |
| Cmd+Shift+M | MQTT Log |

## Fix Driver USB Serial (CP210x / Heltec V3)

Il driver `usb_serial` di Haiku R1 beta5 ha un bug che impedisce il
funzionamento dei dispositivi Silicon Labs CP210x (usati da Heltec LoRa32 V3
e simili). Il pacchetto include il driver corretto nella cartella
`usb_serial_fix/`.

### Sintomi del bug
- Il dispositivo viene riconosciuto da `listusb` ma non appare in `/dev/ports/`
- Nel syslog: "failed to queue initial interrupt"

### Installazione driver corretto

```bash
# Crea le directory necessarie
mkdir -p /boot/system/non-packaged/add-ons/kernel/drivers/bin
mkdir -p /boot/system/non-packaged/add-ons/kernel/drivers/dev/ports

# Installa il driver patchato
cp usb_serial_fix/usb_serial.patched \
   /boot/system/non-packaged/add-ons/kernel/drivers/bin/usb_serial

# Crea il symlink
ln -sf ../../bin/usb_serial \
   /boot/system/non-packaged/add-ons/kernel/drivers/dev/ports/usb_serial

# Riavvia
shutdown -r
```

### Verifica

```bash
# Dopo il riavvio, collega il dispositivo e verifica:
listusb | grep -i silicon
ls /dev/ports/
```

Se appare `/dev/ports/usb0` il fix funziona. La patch sorgente e ulteriori
dettagli tecnici sono in `usb_serial_fix/README.md`.

## Troubleshooting

### Device not detected
- Check USB connection
- Try different USB ports
- Verify device is in companion mode
- **If using CP210x (Heltec V3):** install the USB serial fix above

### No contacts showing
- Use Device -> Sync Contacts
- Ensure device has contacts stored
- Check if device is receiving advertisements
