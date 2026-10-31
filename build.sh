#!/usr/bin/env bash
# Build the Shisha Ledger app using Ledger's official Docker builder.
#
# Usage:
#   ./build.sh [nano_s | nano_x | nano_sp | stax]   (default: nano_s)
#
# Prerequisites:
#   - Docker installed and running
#
# The built app binary lands at:
#   bin/app.elf    (ELF — useful for Speculos emulator)
#   bin/app.hex    (Intel HEX — what you sideload to a real device)
#
# To sideload to a real device (developer mode must be enabled):
#   pip install ledgerctl
#   ./build.sh nano_x && ledgerctl install <app_load_params> bin/app.hex
#
# To run in the Speculos emulator (no hardware needed):
#   pip install speculos
#   ./build.sh nano_s
#   speculos bin/app.elf --model nanos

set -euo pipefail

TARGET="${1:-nano_s}"

case "$TARGET" in
  nano_s)  SDK_VAR="NANOS_SDK"  ;;
  nano_x)  SDK_VAR="NANOX_SDK"  ;;
  nano_sp) SDK_VAR="NANOSP_SDK" ;;
  stax)    SDK_VAR="STAX_SDK"   ;;
  *)
    echo "Unknown target '$TARGET'. Use: nano_s | nano_x | nano_sp | stax"
    exit 1
    ;;
esac

IMAGE="ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder:latest"

echo "==> Pulling Ledger builder image (first run may take a few minutes)…"
docker pull "$IMAGE"

echo "==> Building for $TARGET (SDK var: \$$SDK_VAR)…"
docker run --rm \
  --platform linux/amd64 \
  -v "$(pwd)":/app \
  -w /app \
  "$IMAGE" \
  bash -lc "set -euo pipefail; export BOLOS_SDK=\${$SDK_VAR}; make clean; make"

echo ""
echo "Build complete!"
echo "  ELF: $(pwd)/bin/app.elf"
echo "  HEX: $(pwd)/bin/app.hex"
echo ""
echo "To run in Speculos emulator (no hardware needed):"
echo "  pip install speculos"
echo "  speculos bin/app.elf --model ${TARGET//_/-}"
