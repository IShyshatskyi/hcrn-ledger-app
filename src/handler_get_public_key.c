// APDU handler: INS_GET_PUBLIC_KEY (0x02)
//
// P1 = 0x00  derive silently, return immediately
// P1 = 0x01  show address on screen, wait for user confirm
//
// Data:  [bip32_path encoded per encode_bip32_path() in ledger.rs]
//
// Response (success):
//   [pubkey_len: 1B = 33][compressed_pubkey: 33B]
//   [addr_len:   1B = 64][address_hex: 64B]
//   SW_OK (2B)

#include "globals.h"

// Forward declarations for UX callbacks (defined in ui_flow.c)
extern void ui_display_public_key(void);
extern void user_action_get_pk_approved(void);
extern void user_action_get_pk_rejected(void);

// Stored while waiting for UX confirmation
static volatile unsigned int *s_flags;
static volatile unsigned int *s_tx;

// ── Build and send the GET_PUBLIC_KEY response ────────────────────────────────

void finish_get_public_key(bool approved) {
    if (!approved) {
        send_sw(s_tx, SW_USER_REJECTED);
        return;
    }

    cx_ecfp_private_key_t privkey;
    cx_ecfp_public_key_t  pubkey;

    if (derive_keypair(&G_ctx.bip32, &privkey, &pubkey) != 0) {
        explicit_bzero(&privkey, sizeof(privkey));
        send_sw(s_tx, SW_INVALID_DATA);
        return;
    }
    explicit_bzero(&privkey, sizeof(privkey));

    // Compressed pubkey (33 bytes)
    G_io_apdu_buffer[*s_tx] = 33;
    (*s_tx)++;
    memcpy(G_io_apdu_buffer + *s_tx, pubkey.W, 33);
    (*s_tx) += 33;

    // Address = hex(x-only pubkey) = 64 chars
    uint8_t x_only[32];
    get_x_only_pubkey(&pubkey, x_only);

    char addr_hex[65];
    app_bytes_to_hex(addr_hex, x_only, 32);

    G_io_apdu_buffer[*s_tx] = 64;
    (*s_tx)++;
    memcpy(G_io_apdu_buffer + *s_tx, addr_hex, 64);
    (*s_tx) += 64;

    send_sw(s_tx, SW_OK);
}

// Called by UX confirm button
void user_action_get_pk_approved(void) {
    finish_get_public_key(true);
    *s_flags |= IO_RETURN_AFTER_TX;
}

// Called by UX reject button
void user_action_get_pk_rejected(void) {
    finish_get_public_key(false);
    *s_flags |= IO_RETURN_AFTER_TX;
}

// ── Entry point ───────────────────────────────────────────────────────────────

void handle_get_public_key(uint8_t p1, uint8_t p2,
                            const uint8_t *data, uint16_t data_len,
                            volatile unsigned int *flags,
                            volatile unsigned int *tx) {
    (void)p2;

    if (p1 != P1_NO_CONFIRM && p1 != P1_CONFIRM) {
        send_sw(tx, SW_WRONG_P1_P2);
        return;
    }

    if (parse_bip32_path(data, data_len, &G_ctx.bip32) != 0) {
        send_sw(tx, SW_INVALID_DATA);
        return;
    }

    // Derive key early to populate the display string.
    cx_ecfp_private_key_t privkey;
    cx_ecfp_public_key_t  pubkey;
    if (derive_keypair(&G_ctx.bip32, &privkey, &pubkey) != 0) {
        explicit_bzero(&privkey, sizeof(privkey));
        send_sw(tx, SW_INVALID_DATA);
        return;
    }
    explicit_bzero(&privkey, sizeof(privkey));

    uint8_t x_only[32];
    get_x_only_pubkey(&pubkey, x_only);
    app_bytes_to_hex(G_ctx.recipient_str, x_only, 32);
    G_ctx.recipient_str[64] = '\0';

    if (p1 == P1_NO_CONFIRM) {
        // Silent derivation — respond immediately without a UX flow.
        finish_get_public_key(true);
    } else {
        // Show address on device screen, block until user accepts or rejects.
        s_flags = flags;
        s_tx    = tx;
        ui_display_public_key();
        *flags |= IO_ASYNCH_REPLY;
    }
}
