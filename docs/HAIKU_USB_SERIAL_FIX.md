# Haiku USB Serial Driver Fix for Silicon Labs CP210x

## Bug Summary

The Haiku OS `usb_serial` driver fails to properly handle Silicon Labs CP210x USB-to-UART bridge devices (commonly used in Heltec LoRa32 V3.2 boards). The driver attempts to use an interrupt endpoint that doesn't exist on these devices, causing device initialization to fail.

**Affected Devices:**
- Silicon Labs CP210x (VID: 0x10c4)
- Heltec LoRa32 V3.2 and similar ESP32-based boards

**Haiku Version:** R1~beta5+development hrev59344 (and earlier)

**Observed Errors in syslog:**
```
usb_serial: Silicon Labs CP210x USB UART converter (0x10c4/0xea60) added
usb_serial: failed to queue initial interrupt
usb_serial: input thread: failed to clear halt feature
usb error ehci 1: qtd error: 0x82008d42
```

---

## Root Cause Analysis

### The Problem

The bug is in `SiliconDevice::AddDevice()` in `Silicon.cpp`:

```cpp
// Original buggy code (Silicon.cpp:41-43)
SetControlPipe(endpoint->handle);  // Sets fControlPipe to a BULK endpoint
SetWritePipe(endpoint->handle);
```

The driver:
1. Searches for BULK endpoints
2. For BULK OUT endpoints, sets **both** `fControlPipe` and `fWritePipe` to the same handle
3. Marks 2 pipes as "set" for a single endpoint (incorrect counting)

Then in `SerialDevice::Open()` (line 346):

```cpp
status = gUSBModule->queue_interrupt(fControlPipe, ...);
```

The driver calls `queue_interrupt()` on `fControlPipe`, which is actually a **BULK** endpoint. The USB stack correctly rejects this because `queue_interrupt()` only works with interrupt endpoints.

### Why CP210x is Different

Unlike ACM or Prolific devices, Silicon Labs CP210x chips:
- **Do NOT have an interrupt endpoint** for status notifications
- Only have two BULK endpoints (IN and OUT) for data transfer
- All control operations (baudrate, line state, etc.) are performed via USB control transfers (`send_request()`)

### Comparison with Other Drivers

| Driver   | Control Pipe Type | Interrupt Endpoint |
|----------|-------------------|-------------------|
| ACM      | Interrupt         | Yes               |
| Prolific | Interrupt         | Yes               |
| **CP210x** | **None needed** | **No**            |
| FTDI     | Bulk (same bug)   | No                |

Note: FTDI has the same problematic code pattern, but may not manifest the same symptoms depending on device behavior.

---

## Solution Implemented

### Approach

The fix consists of two parts:

1. **Silicon.cpp**: Remove the incorrect `SetControlPipe()` call since CP210x devices don't use interrupt endpoints
2. **SerialDevice.cpp**: Add null checks before using `fControlPipe` to handle devices without interrupt endpoints gracefully

### Changes Made

#### 1. Silicon.cpp - `AddDevice()`

**Before:**
```cpp
if (endpoint->descr->endpoint_address) {
    SetControlPipe(endpoint->handle);  // BUG: bulk != interrupt
    SetWritePipe(endpoint->handle);
    pipesSet += 2;                      // BUG: only 1 endpoint
    if (pipesSet >= 3)
        break;
}
// ...
if (pipesSet >= 3)  // Required 3 but only have 2 real endpoints
```

**After:**
```cpp
if (endpoint->descr->endpoint_address) {
    SetWritePipe(endpoint->handle);
    pipesSet++;
}
// ...
// CP210x devices only have bulk endpoints for data transfer.
// They do not have an interrupt endpoint for status notifications.
// All control operations are performed via USB control transfers.
if (pipesSet >= 2)  // Only need read + write
```

#### 2. SerialDevice.cpp - `Open()`

**Before:**
```cpp
status = gUSBModule->queue_interrupt(fControlPipe, fInterruptBuffer,
    fInterruptBufferSize, _InterruptCallbackFunction, this);
if (status < B_OK)
    TRACE_ALWAYS("failed to queue initial interrupt\n");
```

**After:**
```cpp
// Only queue interrupt transfers if we have a valid interrupt endpoint.
// Some devices (e.g., Silicon Labs CP210x) do not have interrupt endpoints
// and rely solely on USB control transfers for status updates.
if (fControlPipe != 0) {
    status = gUSBModule->queue_interrupt(fControlPipe, fInterruptBuffer,
        fInterruptBufferSize, _InterruptCallbackFunction, this);
    if (status < B_OK)
        TRACE_ALWAYS("failed to queue initial interrupt\n");
}
```

#### 3. SerialDevice.cpp - `Close()`

Added null check before `cancel_queued_transfers()`:
```cpp
if (fControlPipe != 0)
    gUSBModule->cancel_queued_transfers(fControlPipe);
```

#### 4. SerialDevice.cpp - `Removed()`

Same null check added.

#### 5. SerialDevice.cpp - `_InterruptCallbackFunction()`

Added null check to prevent re-queuing on non-existent pipe:
```cpp
if (status == B_OK && !device->fDeviceRemoved && device->fControlPipe != 0) {
```

---

## Files Modified

```
src/add-ons/kernel/drivers/ports/usb_serial/SerialDevice.cpp
src/add-ons/kernel/drivers/ports/usb_serial/Silicon.cpp
```

---

## Patch

The patch is available at:
```
/Magazzino/Sestriere/haiku-patches/0001-usb_serial-fix-CP210x-no-interrupt-endpoint.patch
```

### Full Diff

```diff
diff --git a/src/add-ons/kernel/drivers/ports/usb_serial/SerialDevice.cpp b/src/add-ons/kernel/drivers/ports/usb_serial/SerialDevice.cpp
index 6000e0c9..c37e495d 100644
--- a/src/add-ons/kernel/drivers/ports/usb_serial/SerialDevice.cpp
+++ b/src/add-ons/kernel/drivers/ports/usb_serial/SerialDevice.cpp
@@ -343,10 +343,15 @@ SerialDevice::Open(uint32 flags)
 		| USB_CDC_CONTROL_SIGNAL_STATE_RTS;
 	SetControlLineState(fControlOut);

-	status = gUSBModule->queue_interrupt(fControlPipe, fInterruptBuffer, fInterruptBufferSize,
-		_InterruptCallbackFunction, this);
-	if (status < B_OK)
-		TRACE_ALWAYS("failed to queue initial interrupt\n");
+	// Only queue interrupt transfers if we have a valid interrupt endpoint.
+	// Some devices (e.g., Silicon Labs CP210x) do not have interrupt endpoints
+	// and rely solely on USB control transfers for status updates.
+	if (fControlPipe != 0) {
+		status = gUSBModule->queue_interrupt(fControlPipe, fInterruptBuffer,
+			fInterruptBufferSize, _InterruptCallbackFunction, this);
+		if (status < B_OK)
+			TRACE_ALWAYS("failed to queue initial interrupt\n");
+	}

 	// set our config (will propagate to the slave config as well in SetModes()
 	gTTYModule->tty_control(fSystemTTYCookie, TCSETA, &fTTYConfig,
@@ -463,7 +468,8 @@ SerialDevice::Close()
 	if (!fDeviceRemoved) {
 		gUSBModule->cancel_queued_transfers(fReadPipe);
 		gUSBModule->cancel_queued_transfers(fWritePipe);
-		gUSBModule->cancel_queued_transfers(fControlPipe);
+		if (fControlPipe != 0)
+			gUSBModule->cancel_queued_transfers(fControlPipe);
 	}

 	gTTYModule->tty_close_cookie(fSystemTTYCookie);
@@ -508,7 +514,8 @@ SerialDevice::Removed()
 	fInputStopped = false;
 	gUSBModule->cancel_queued_transfers(fReadPipe);
 	gUSBModule->cancel_queued_transfers(fWritePipe);
-	gUSBModule->cancel_queued_transfers(fControlPipe);
+	if (fControlPipe != 0)
+		gUSBModule->cancel_queued_transfers(fControlPipe);
 }


@@ -727,7 +734,7 @@ SerialDevice::_InterruptCallbackFunction(void *cookie, status_t status,

 	// ToDo: maybe handle those somehow?

-	if (status == B_OK && !device->fDeviceRemoved) {
+	if (status == B_OK && !device->fDeviceRemoved && device->fControlPipe != 0) {
 		status = gUSBModule->queue_interrupt(device->fControlPipe,
 			device->fInterruptBuffer, device->fInterruptBufferSize,
 			device->_InterruptCallbackFunction, device);
diff --git a/src/add-ons/kernel/drivers/ports/usb_serial/Silicon.cpp b/src/add-ons/kernel/drivers/ports/usb_serial/Silicon.cpp
index f902302f..49686a53 100644
--- a/src/add-ons/kernel/drivers/ports/usb_serial/Silicon.cpp
+++ b/src/add-ons/kernel/drivers/ports/usb_serial/Silicon.cpp
@@ -34,21 +34,20 @@ SiliconDevice::AddDevice(const usb_configuration_info *config)
 			if (endpoint->descr->attributes == USB_ENDPOINT_ATTR_BULK) {
 				if (endpoint->descr->endpoint_address & USB_ENDPOINT_ADDR_DIR_IN) {
 					SetReadPipe(endpoint->handle);
-					if (++pipesSet >= 3)
-						break;
+					pipesSet++;
 				} else {
 					if (endpoint->descr->endpoint_address) {
-						SetControlPipe(endpoint->handle);
 						SetWritePipe(endpoint->handle);
-						pipesSet += 2;
-						if (pipesSet >= 3)
-							break;
+						pipesSet++;
 					}
 				}
 			}
 		}

-		if (pipesSet >= 3) {
+		// CP210x devices only have bulk endpoints for data transfer.
+		// They do not have an interrupt endpoint for status notifications.
+		// All control operations are performed via USB control transfers.
+		if (pipesSet >= 2) {
 			status = B_OK;
 		}
 	}
```

---

## How to Apply the Patch

### Option 1: Apply to Haiku Source Tree

```bash
cd /path/to/haiku-src
git apply /Magazzino/Sestriere/haiku-patches/0001-usb_serial-fix-CP210x-no-interrupt-endpoint.patch
```

### Option 2: Manual Application

Copy the modified files from `/tmp/haiku-src/src/add-ons/kernel/drivers/ports/usb_serial/` to your Haiku source tree.

---

## Building the Driver on Haiku

### Prerequisites

1. Install development tools:
   ```bash
   pkgman install haiku_devel makefile_engine
   ```

2. Get the Haiku source (if not already):
   ```bash
   git clone https://github.com/haiku/haiku.git
   ```

### Build Steps

```bash
cd haiku/src/add-ons/kernel/drivers/ports/usb_serial
jam -q usb_serial
```

The compiled driver will be at:
```
objects/haiku/x86_64/release/add-ons/kernel/drivers/ports/usb_serial/usb_serial
```

### Install the Driver

```bash
# Backup original driver
cp /boot/system/add-ons/kernel/drivers/bin/usb_serial ~/usb_serial.backup

# Install new driver (requires root)
cp objects/.../usb_serial /boot/system/add-ons/kernel/drivers/bin/

# Reload the driver
echo "Unplug and replug your USB device, or reboot"
```

---

## Testing the Fix

### Test Procedure

1. Connect the Heltec LoRa32 V3.2 (or any CP210x device)

2. Verify device detection:
   ```bash
   listusb
   # Should show: 10c4:ea60 "Silicon Labs" "CP210x UART Bridge"
   ```

3. Check serial device node:
   ```bash
   ls -la /dev/ports/
   # Should show: usb0 (or similar)
   ```

4. Check syslog for errors:
   ```bash
   grep -i "usb_serial\|cp210" /var/log/syslog
   # Should NOT show "failed to queue initial interrupt"
   ```

5. Test serial communication:
   ```bash
   # Using SerialConnect or similar terminal
   SerialConnect /dev/ports/usb0 115200

   # Or with stty
   stty -F /dev/ports/usb0 115200
   echo "test" > /dev/ports/usb0
   ```

### Expected Results After Fix

- Device recognized without errors
- `/dev/ports/usb0` accessible
- Serial communication works (TX/RX)
- No "failed to queue initial interrupt" in syslog

---

## Additional Notes

### FTDI Driver

The FTDI driver (`FTDI.cpp`) has the same code pattern:
```cpp
SetControlPipe(endpoint->handle);  // Also sets to BULK endpoint
SetWritePipe(endpoint->handle);
```

This should be reviewed for similar issues, though FTDI devices may behave differently.

### Future Improvements

Consider adding a `fHasInterruptEndpoint` flag to `SerialDevice` for cleaner handling, or using endpoint type validation before queue operations.

---

## References

- Haiku USB Serial Driver: `src/add-ons/kernel/drivers/ports/usb_serial/`
- Silicon Labs AN197: Serial Communications Guide for CP210x
- USB CDC-ACM Specification (for comparison with ACM devices)

---

## Author

Analysis and fix generated for Haiku OS R1~beta5+development

**Date:** 2026-02-01
