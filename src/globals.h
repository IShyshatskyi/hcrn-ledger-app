#pragma once

#include "os.h"
#include "cx.h"
#include "ux.h"
#include "os_io_seproxyhal.h"
#include "apdu_constants.h"

// ── BIP32 ─────────────────────────────────────────────────────────────────────

#define MAX_BIP32_DEPTH  10

typedef struct {
    uint32_t  path[MAX_BIP32_DEPTH];
    uint8_t   path_len;
} bip32_path_t;

#include "shisha_tx.h"

// ── App global context ────────────────────────────────────────────────────────

typedef enum {
    STATE_IDLE,
    STATE_SIGNING,
} app_state_t;

typedef struct {
    app_state_t state;

    // For GET_PUBLIC_KEY and SIGN_TRANSACTION
    bip32_path_t bip32;

    // Raw payload accumulation (SIGN_TRANSACTION chunked)
    uint8_t  payload[SHISHA_MAX_PAYLOAD_LEN];
    uint16_t payload_len;

    // Parsed fields (populated after full payload received)
    shisha_tx_t tx;

    // Human-readable display strings
    char  total_out_str[32];  // e.g. "5.123456 SHISHA"
    char  fee_str[32];        // e.g. "0.000100 SHISHA"
    char  shard_str[12];      // "beacon" or "shard N" (see format_chain_label)
    char  outputs_str[128];   // "2 output(s): 4.00 + 1.12 SHISHA"
    char  recipient_str[72];  // hex of first output recipient (scrollable)
} app_ctx_t;

extern app_ctx_t G_ctx;

// ── APDU buffer ───────────────────────────────────────────────────────────────

extern unsigned char G_io_apdu_buffer[IO_APDU_BUFFER_SIZE];

// ── UX flows (defined in ui_flow.c) ──────────────────────────────────────────

extern const ux_flow_step_t * const ux_idle_flow[];
extern const ux_flow_step_t * const ux_sign_flow[];
extern const ux_flow_step_t * const ux_get_pk_flow[];

// ── Handler declarations ──────────────────────────────────────────────────────

void handle_get_public_key(uint8_t p1, uint8_t p2,
                           const uint8_t *data, uint16_t data_len,
                           volatile unsigned int *flags,
                           volatile unsigned int *tx);

void handle_sign_tx(uint8_t p1, uint8_t p2,
                    const uint8_t *data, uint16_t data_len,
                    volatile unsigned int *flags,
                    volatile unsigned int *tx);

// ── Utility ───────────────────────────────────────────────────────────────────

static inline void send_sw(volatile unsigned int *tx, uint16_t sw) {
    G_io_apdu_buffer[(*tx)++] = (sw >> 8) & 0xFF;
    G_io_apdu_buffer[(*tx)++] = sw & 0xFF;
}

// Encode `value` (in atoms) as "X.XXXXXX SHISHA\0" into `out` (len >= 32).
void format_amount(char *out, size_t out_len, uint64_t atoms);

// Encode `bytes` (len bytes) as lowercase hex into `out` (must be 2*len+1 bytes).
void bytes_to_hex(char *out, const uint8_t *bytes, size_t len);
