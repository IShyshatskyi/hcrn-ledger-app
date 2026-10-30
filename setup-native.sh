#!/usr/bin/env bash
# Install the ARM cross-compiler + Nano S SDK on macOS without Docker.
#
# Run once, then use build.sh --native to build, or:
#   source env.sh && make
#
# After this script finishes, run:
#   source env.sh
#   make
#   make load    (device connected, developer mode on)

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "==> Checking Homebrew…"
if ! command -v brew &>/dev/null; then
  echo "ERROR: Homebrew not found. Install it from https://brew.sh then re-run."
  exit 1
fi

echo "==> Installing ARM cross-compiler (arm-none-eabi-gcc)…"
if ! command -v arm-none-eabi-gcc &>/dev/null; then
  # The GCC Arm Embedded cask is the official ARM Ltd release (same as Docker builder)
  brew install --cask gcc-arm-embedded 2>/dev/null || \
  brew install arm-none-eabi-gcc 2>/dev/null || \
  { echo "ERROR: Could not install ARM toolchain. Try: brew install --cask gcc-arm-embedded"; exit 1; }
else
  echo "   arm-none-eabi-gcc already installed: $(arm-none-eabi-gcc --version | head -1)"
fi

echo "==> Installing Python + ledgerctl…"
if ! command -v ledgerctl &>/dev/null; then
  pip3 install --upgrade ledgerctl
else
  echo "   ledgerctl already installed: $(ledgerctl --version 2>/dev/null || true)"
fi

echo "==> Cloning Nano S SDK…"
SDK_DIR="$SCRIPT_DIR/vendor/nanos-secure-sdk"
if [ ! -d "$SDK_DIR" ]; then
  git clone --depth 1 https://github.com/LedgerHQ/nanos-secure-sdk.git "$SDK_DIR"
  echo "   cloned to $SDK_DIR"
else
  echo "   already present at $SDK_DIR — pulling latest…"
  git -C "$SDK_DIR" pull --ff-only
fi

echo "==> Writing env.sh…"
cat > "$SCRIPT_DIR/env.sh" <<ENVEOF
# Source this before building: source env.sh
export BOLOS_SDK="$SDK_DIR"
export BOLOS_ENV="\$(brew --prefix gcc-arm-embedded 2>/dev/null || brew --prefix arm-none-eabi-gcc 2>/dev/null)"
export PATH="\$BOLOS_ENV/bin:\$PATH"
ENVEOF

echo ""
echo "Done! To build:"
echo "  cd $(basename "$SCRIPT_DIR")"
echo "  source env.sh"
echo "  make"
echo ""
echo "To sideload to your Nano S (enable Settings > Security > Developer Mode first):"
echo "  make load"
