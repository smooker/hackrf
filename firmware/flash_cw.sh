#!/bin/bash
# flash_cw.sh — Flash CW firmware to HackRF One
# Key press required at every step
#
# Usage: ./flash_cw.sh [dfu|spi|both]
#   dfu  — DFU only (RAM boot)
#   spi  — SPI flash only (persistent)
#   both — DFU then SPI (default)

set -e

FW_DIR="$(dirname "$0")/hackrf_usb/build"
DFU_FILE="$FW_DIR/hackrf_usb.dfu"
SPI_FILE="$FW_DIR/hackrf_usb.bin"

MODE="${1:-both}"

echo "=== HackRF CW Firmware Flash ==="
echo "DFU: $DFU_FILE"
echo "SPI: $SPI_FILE"
echo "Mode: $MODE"
echo ""

pause() {
    echo ""
    echo ">>> $1"
    echo "Press ENTER to continue, Ctrl+C to abort..."
    read -r
}

# Step 1: Check files exist
echo "--- Checking firmware files ---"
if [ "$MODE" = "dfu" ] || [ "$MODE" = "both" ]; then
    ls -la "$DFU_FILE" || { echo "ERROR: DFU file not found!"; exit 1; }
fi
if [ "$MODE" = "spi" ] || [ "$MODE" = "both" ]; then
    ls -la "$SPI_FILE" || { echo "ERROR: SPI file not found!"; exit 1; }
fi

# Step 2: Check USB bus
pause "Step 1: Check USB — is HackRF connected?"
echo "--- lsusb ---"
lsusb | grep -E "1fc9:000c|1d50:6089" || echo "WARNING: HackRF not found on USB!"
echo ""

# Step 3: DFU flash
if [ "$MODE" = "dfu" ] || [ "$MODE" = "both" ]; then
    pause "Step 2: DFU flash (HackRF must be in DFU mode — 1fc9:000c)"
    echo "--- dfu-util ---"
    dfu-util --device 1fc9:000c --alt 0 --download "$DFU_FILE"
    echo "DFU flash done."
    sleep 2
fi

# Step 4: SPI flash
if [ "$MODE" = "spi" ] || [ "$MODE" = "both" ]; then
    pause "Step 3: SPI flash (HackRF must be running — 1d50:6089)"
    echo "--- hackrf_spiflash ---"
    hackrf_spiflash -Rw "$SPI_FILE"
    echo "SPI flash done. Waiting for reboot..."
    sleep 5
fi

# Step 5: Verify
pause "Step 4: Verify — hackrf_info"
echo "--- hackrf_info ---"
hackrf_info

echo ""
echo "=== Done! ==="
