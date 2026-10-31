// Shisha Network Ledger App — main APDU dispatcher
//
// BOLOS entry points:
//   app_main()   ← called by the SDK after the app is launched
//   io_event()   ← USB / BLE events (called by SDK, not by us)

#include "globals.h"

app_ctx_t G_ctx;

// G_ux / G_ux_params come from lib_standard_app.

// ── Utility implementations ───────────────────────────────────────────────────

void format_amount(char *out, size_t out_len, uint64_t atoms) {
    uint64_t whole = atoms / SHISHA_ATOMS_PER_COIN;
    uint64_t frac  = atoms % SHISHA_ATOMS_PER_COIN;
    snprintf(out, out_len, "%llu.%06llu SHISHA",
             (unsigned long long)whole, (unsigned long long)frac);
}

void app_bytes_to_hex(char *out, const uint8_t *bytes, size_t len) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[2*i]     = hex[(bytes[i] >> 4) & 0xF];
        out[2*i + 1] = hex[bytes[i] & 0xF];
    }
    out[2*len] = '\0';
}

// ── Reset context ─────────────────────────────────────────────────────────────

static void reset_ctx(void) {
    explicit_bzero(&G_ctx, sizeof(G_ctx));
    G_ctx.state = STATE_IDLE;
}

// ── APDU dispatcher ───────────────────────────────────────────────────────────

static void handle_apdu(volatile unsigned int *flags, volatile unsigned int *tx) {
    const uint8_t cla  = G_io_apdu_buffer[0];
    const uint8_t ins  = G_io_apdu_buffer[1];
    const uint8_t p1   = G_io_apdu_buffer[2];
    const uint8_t p2   = G_io_apdu_buffer[3];
    const uint8_t lc   = G_io_apdu_buffer[4];
    const uint8_t *data = G_io_apdu_buffer + 5;

    if (cla != CLA) {
        send_sw(tx, SW_CLA_NOT_SUPPORTED);
        return;
    }

    switch (ins) {

    case INS_GET_APP_NAME: {
        // Return the ASCII app name (no status word yet — added below).
        static const char name[] = "Shisha";
        size_t n = sizeof(name) - 1;
        memcpy(G_io_apdu_buffer, name, n);
        *tx = n;
        send_sw(tx, SW_OK);
        break;
    }

    case INS_GET_VERSION: {
        G_io_apdu_buffer[0] = APPVERSION_MAJOR;
        G_io_apdu_buffer[1] = APPVERSION_MINOR;
        G_io_apdu_buffer[2] = APPVERSION_PATCH;
        *tx = 3;
        send_sw(tx, SW_OK);
        break;
    }

    case INS_GET_PUBLIC_KEY:
        handle_get_public_key(p1, p2, data, lc, flags, tx);
        break;

    case INS_SIGN_TRANSACTION_V2:
        handle_sign_tx(p1, p2, data, lc, flags, tx);
        break;

    default:
        send_sw(tx, SW_INS_NOT_SUPPORTED);
        break;
    }
}

// ── io_seproxyhal glue ────────────────────────────────────────────────────────

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;
    case CHANNEL_SPI:
    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

// Called by the SDK for every SEPROXYHAL event (button press, USB packet, etc.)
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default(element);
}

unsigned char io_event(unsigned char channel) {
    (void) channel;
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;
    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;
    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
            !(U4BE(G_io_seproxyhal_spi_buffer, 3) & SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
            THROW(EXCEPTION_IO_RESET);
        }
        UX_DEFAULT_EVENT();
        break;
    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;
    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
        break;
    default:
        UX_DEFAULT_EVENT();
        break;
    }

    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }
    return 1;
}

// ── App main loop ─────────────────────────────────────────────────────────────

void app_main(void) {
    volatile unsigned int rx   = 0;
    volatile unsigned int tx   = 0;
    volatile unsigned int flags = 0;

    reset_ctx();

    // Show idle / main menu.
    UX_INIT();
    ux_flow_init(0, ux_idle_flow, NULL);

    BEGIN_TRY {
        TRY {
            for (;;) {
                rx = io_exchange(CHANNEL_APDU | flags, tx);
                flags = 0;
                tx    = 0;

                if (rx == 0) {
                    // Nothing received — stay in idle.
                    THROW(0x6982);
                }

                handle_apdu(&flags, &tx);
            }
        }
        CATCH(EXCEPTION_IO_RESET) {
            CLOSE_TRY;
            reset_ctx();
            return;
        }
        CATCH_OTHER(e) {
            // Turn any unhandled exception into a status word.
            G_io_apdu_buffer[tx++] = (e >> 8) & 0xFF;
            G_io_apdu_buffer[tx++] = e & 0xFF;
        }
        FINALLY {}
    }
    END_TRY;
}

// ── Boot entry point ──────────────────────────────────────────────────────────

__attribute__((section(".boot")))
int main(void) {
    __asm volatile ("cpsie i");   // enable interrupts
    os_boot();

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

#ifdef TARGET_NANOX
            G_io_app.plane_mode = os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
#endif
            USB_power(0);
            USB_power(1);

            ui_idle(); // defined in ui_flow.c

#ifdef HAVE_BLE
            BLE_power(0, NULL);
            BLE_power(1, "Nano X");
#endif
            app_main();
        }
        CATCH_OTHER(e) {}
        FINALLY {}
    }
    END_TRY;

    // os_sched_exit() is not supported on all SDK versions; app restarts via reset.
    for (;;);
}
