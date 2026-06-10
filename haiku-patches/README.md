# Haiku USB Serial Driver Patch

## Fix per dispositivi CP210x (Heltec LoRa32, etc.)

### Il Bug
Il driver `usb_serial` chiama `queue_interrupt()` su un bulk endpoint per dispositivi CP210x, causando errori:
- "failed to queue initial interrupt"
- Device non funzionante

### La Patch
File: `usb_serial_cp210x_fix.patch`

Modifiche:
- **Silicon.cpp**: Rimuove `SetControlPipe()` (CP210x non ha interrupt endpoint)
- **SerialDevice.cpp**: Aggiunge null check prima di `queue_interrupt()`

---

## Installazione Rapida (Driver Precompilato)

Il driver patchato precompilato è disponibile: `usb_serial.patched`

```bash
# Crea le directory necessarie
mkdir -p /boot/system/non-packaged/add-ons/kernel/drivers/bin
mkdir -p /boot/system/non-packaged/add-ons/kernel/drivers/dev/ports

# Installa il driver
cp usb_serial.patched /boot/system/non-packaged/add-ons/kernel/drivers/bin/usb_serial

# Crea il symlink
ln -sf ../../bin/usb_serial /boot/system/non-packaged/add-ons/kernel/drivers/dev/ports/usb_serial

# Riavvia per caricare il nuovo driver
shutdown -r
```

---

## Compilazione da Sorgente

### 1. Configura ambiente di build Haiku

```bash
# Clona Haiku
git clone https://github.com/haiku/haiku.git
cd haiku

# Configura (serve solo una volta)
./configure --target-arch x86_64

# Applica la patch
git apply /path/to/usb_serial_cp210x_fix.patch
```

### 2. Compila il driver

```bash
jam -q usb_serial
```

Output: `generated/objects/haiku/x86_64/release/add-ons/kernel/drivers/ports/usb_serial/usb_serial`

### 3. Installa

```bash
# Crea directory non-packaged
mkdir -p /boot/system/non-packaged/add-ons/kernel/drivers/bin
mkdir -p /boot/system/non-packaged/add-ons/kernel/drivers/dev/ports

# Copia il driver
cp generated/objects/haiku/x86_64/release/add-ons/kernel/drivers/ports/usb_serial/usb_serial \
   /boot/system/non-packaged/add-ons/kernel/drivers/bin/

# Crea symlink
ln -sf ../../bin/usb_serial /boot/system/non-packaged/add-ons/kernel/drivers/dev/ports/usb_serial

# Riavvia
shutdown -r
```

### 4. Test

```bash
# Connetti dispositivo CP210x
listusb | grep -i silicon
ls /dev/ports/
cat /var/log/syslog | grep usb_serial
```

---

## Contribuire Upstream

Per contribuire la fix a Haiku:
1. Crea account su https://review.haiku-os.org/
2. Fai push della patch via Gerrit
3. Cita questo bug come riferimento

## Files

- `usb_serial_cp210x_fix.patch` - Patch per Haiku
- `usb_serial.patched` - Driver precompilato (x86_64)
- `0001-usb_serial-fix-CP210x-no-interrupt-endpoint.patch` - Formato git commit
