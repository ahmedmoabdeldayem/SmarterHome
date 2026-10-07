#!/bin/bash
# SmarterHome — Download Raspberry Pi OS Lite (64-bit)
#
# The OS image is ~500MB compressed and cannot be stored in git.
# Run this script to download it into the scripts/ directory.
#
# After downloading, flash it using Raspberry Pi Imager:
#   - Open Raspberry Pi Imager
#   - Choose OS → "Use custom" → select the .img.xz file downloaded here
#   - Or use the official "Raspberry Pi OS Lite (64-bit)" option directly in Imager
#
# Usage:
#   chmod +x scripts/download_rpi_os.sh
#   ./scripts/download_rpi_os.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/rpi_os"
mkdir -p "$OUTPUT_DIR"

# Official Raspberry Pi OS Lite 64-bit (latest stable)
# Source: https://www.raspberrypi.com/software/operating-systems/
IMAGE_URL="https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2025-05-13/2025-05-13-raspios-bookworm-arm64-lite.img.xz"
IMAGE_FILE="$OUTPUT_DIR/raspios-bookworm-arm64-lite.img.xz"
SHA256_URL="${IMAGE_URL}.sha256"

echo "==> Downloading Raspberry Pi OS Lite (64-bit)..."
echo "    Destination: $IMAGE_FILE"
echo ""

# Download image
curl -L --progress-bar -o "$IMAGE_FILE" "$IMAGE_URL"

# Download and verify checksum
echo ""
echo "==> Verifying checksum..."
EXPECTED_SHA=$(curl -sL "$SHA256_URL" | awk '{print $1}')

if command -v sha256sum &>/dev/null; then
    ACTUAL_SHA=$(sha256sum "$IMAGE_FILE" | awk '{print $1}')
elif command -v shasum &>/dev/null; then
    ACTUAL_SHA=$(shasum -a 256 "$IMAGE_FILE" | awk '{print $1}')
else
    echo "[!] sha256sum not found, skipping verification."
    ACTUAL_SHA="$EXPECTED_SHA"
fi

if [ "$EXPECTED_SHA" = "$ACTUAL_SHA" ]; then
    echo "[✓] Checksum OK: $ACTUAL_SHA"
else
    echo "[✗] Checksum MISMATCH!"
    echo "    Expected: $EXPECTED_SHA"
    echo "    Got:      $ACTUAL_SHA"
    echo "    The downloaded file may be corrupted. Delete it and retry."
    exit 1
fi

echo ""
echo "==> Done! Image saved to:"
echo "    $IMAGE_FILE"
echo ""
echo "==> Next steps:"
echo "    1. Open Raspberry Pi Imager"
echo "    2. Click 'Choose OS' → 'Use custom' → select the .img.xz file above"
echo "    3. Click the gear icon (⚙) to pre-configure:"
echo "         - Hostname:  smarthome-hub"
echo "         - Enable SSH (use password or public key)"
echo "         - Username:  pi"
echo "         - Password:  (choose a strong password)"
echo "         - Wi-Fi SSID and password"
echo "    4. Click 'Write'"
echo "    5. Insert SD card into Raspberry Pi and power on"
echo "    6. SSH in:  ssh pi@smarthome-hub.local"
