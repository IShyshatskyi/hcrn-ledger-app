# HCRN / Shisha Network — Ledger BOLOS application
#
# Build with Ledger’s app-builder image (see ./build.sh or GHA workflow):
#   docker run --rm -v $(pwd):/app -w /app \
#     -e BOLOS_SDK=$NANOSP_SDK \
#     ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder make

ifeq ($(BOLOS_SDK),)
$(error BOLOS_SDK is not set. Use the Docker build container or set it manually.)
endif

include $(BOLOS_SDK)/Makefile.target

########################################
#        Mandatory configuration       #
########################################

APPNAME = "HCRN"

APPVERSION_M = 2
APPVERSION_N = 0
APPVERSION_P = 0
APPVERSION = "$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)"

APP_SOURCE_PATH += src

# Device icons (indexed-color; replace before shipping store builds)
ICON_NANOS = glyphs/app_shisha_16px.bmp
ICON_NANOX = glyphs/app_shisha_16px.bmp
ICON_NANOSP = glyphs/app_shisha_16px.bmp

# BIP32: m/44'/20000' — Shisha coin type
CURVE_APP_LOAD_PARAMS = secp256k1
PATH_APP_LOAD_PARAMS = "44'/20000'"

VARIANT_PARAM = COIN
VARIANT_VALUES = HCRN

# Match prior --appFlags 0x200 (BOLOS settings permission)
HAVE_APPLICATION_FLAG_BOLOS_SETTINGS = 1

########################################
#          Features disablers          #
########################################
# Keep classic BAGL UX flows (ui_flow.c); do not switch Nano devices to NBGL.

include $(BOLOS_SDK)/Makefile.standard_app
