#!/bin/sh
# Build script for patched usb_serial driver on Haiku
# Run this from inside a properly configured Haiku source tree

set -e

HAIKU_SRC="${1:-/boot/system/cache/tmp/haiku-src}"

if [ ! -d "$HAIKU_SRC" ]; then
    echo "Error: Haiku source not found at $HAIKU_SRC"
    echo "Usage: $0 /path/to/haiku-src"
    exit 1
fi

cd "$HAIKU_SRC"

# Check if patch is applied
if grep -q "fControlPipe != 0" src/add-ons/kernel/drivers/ports/usb_serial/SerialDevice.cpp; then
    echo "Patch appears to be applied."
else
    echo "Patch not applied. Apply it first with:"
    echo "  git apply /path/to/0001-usb_serial-fix-CP210x-no-interrupt-endpoint.patch"
    exit 1
fi

# Configure if needed
if [ ! -f "build/config/variables" ]; then
    echo "Configuring build system..."
    ./configure --target-arch x86_64
fi

echo "Building usb_serial driver..."
jam -q usb_serial

echo ""
echo "Build complete! The driver should be at:"
echo "  generated/objects/haiku/x86_64/release/add-ons/kernel/drivers/ports/usb_serial/usb_serial"
echo ""
echo "To install:"
echo "  sudo cp generated/objects/haiku/x86_64/release/add-ons/kernel/drivers/ports/usb_serial/usb_serial \\"
echo "       /boot/system/add-ons/kernel/drivers/bin/"
