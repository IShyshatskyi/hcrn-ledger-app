# Shisha Network — Ledger BOLOS application
#
# Build targets:
#   make           — build for the device set by BOLOS_SDK
#   make load      — build and sideload via ledgerctl (device must be connected)
#   make clean
#
# BOLOS_SDK must point to the SDK for your target device. Inside the Docker
# container this is already set (see Dockerfile / build.sh).
#
# Example (Nano S inside Docker):
#   docker run --rm -v $(pwd):/app -e BOLOS_SDK=$NANOS_SDK \
#       ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder make

ifeq ($(BOLOS_SDK),)
$(error BOLOS_SDK is not set. Use the Docker build container or set it manually.)
endif

include $(BOLOS_SDK)/Makefile.defines

# ── App metadata ──────────────────────────────────────────────────────────────

APPNAME     = HCRN
APPVERSION_M = 2
APPVERSION_N = 0
APPVERSION_P = 0
APPVERSION   = $(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)

DEFINES += APPNAME=\"$(APPNAME)\"
DEFINES += APPVERSION=\"$(APPVERSION)\"
DEFINES += APPVERSION_MAJOR=$(APPVERSION_M)
DEFINES += APPVERSION_MINOR=$(APPVERSION_N)
DEFINES += APPVERSION_PATCH=$(APPVERSION_P)

# USB HID (legacy Makefile.defines apps must set these explicitly on current SDKs)
DEFINES += HAVE_IO_USB HAVE_L4_USB_HID HAVE_USB_APDU
DEFINES += IO_USB_MAX_ENDPOINTS=6
DEFINES += IO_HID_EP_LENGTH=64
DEFINES += USB_SEGMENT_SIZE=64
DEFINES += HAVE_WEBUSB WEBUSB_URL_SIZE_B=0 WEBUSB_URL=""


# ── Allowed BIP32 paths ───────────────────────────────────────────────────────
# The OS enforces this allowlist — the app cannot sign with arbitrary paths.
# m/44'/20000' covers all accounts and indices under the Shisha coin type.

APP_LOAD_PARAMS  = --appFlags 0x200
APP_LOAD_PARAMS += --path "44'/20000'"
APP_LOAD_PARAMS += --curve secp256k1

# ── Sources ───────────────────────────────────────────────────────────────────

APP_SOURCE_PATH = src

# ── SDK libraries ────────────────────────────────────────────────────────────
# lib_stusb / lib_stusb_impl  → USB HID transport
# lib_ux                      → UX flow engine (button navigation)

SDK_SOURCE_PATH += lib_stusb lib_stusb_impl lib_ux

# ── Icons ─────────────────────────────────────────────────────────────────────
# glyphs/app_shisha_16px.bmp  — 16×16 app icon (shown in the Ledger device menu)
# glyphs/app_shisha_14px.bmp  — 14×14 variant for Nano S
#
# To generate: use ledger-app-sdk's icon converter:
#   python3 $(BOLOS_SDK)/icon3.py --hexbitmaponly icon.png --colorize 1 > icon.bmp
#
# Placeholder: if the files are missing the build still succeeds but shows a
# blank icon on device.  Replace before shipping.

ICONNAME = glyphs/app_shisha_16px.bmp

include $(BOLOS_SDK)/Makefile.glyphs
include $(BOLOS_SDK)/Makefile.rules

# ── Sideload (development) ────────────────────────────────────────────────────
# Requires: pip install ledgerctl; device unlocked with developer mode on.

load: all
	ledgerctl install $(APP_LOAD_PARAMS) bin/app.hex
