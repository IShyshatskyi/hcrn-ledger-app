// APDU handler: INS_SIGN_TRANSACTION_V2 (0x05) — sighash v2 (0x04/v1 removed in 2.0.0)
//
// Accepts the Shisha signing payload in one or more 250-byte chunks.
//
// First chunk (P1 = P1_FIRST_CHUNK = 0x00):
//   [bip32_path_bytes][payload_header + first payload data...]
//
// Subsequent chunks (P1 = P1_NEXT_CHUNK = 0x80):
//   [continuation bytes...]
//
// P2 = P2_MORE (0x00) → more chunks follow
// P2 = P2_LAST (0x80) → this is the last chunk; parse, show UX, sign
//
// Response on last chunk after user approval:
//   [signature: 64B][SW_OK: 2B]

#include "globals.h"
#include <string.h>

static volatile unsigned int *s_flags;
static volatile unsigned int *s_tx_len;

// ── Build display strings ─────────────────────────────────────────────────────

static void prepare_display_strings(void) {
    const shisha_tx_t *tx = &G_ctx.tx;

    format_amount(G_ctx.total_out_str, sizeof(G_ctx.total_out_str), tx->total_out);

    uint64_t fee = (tx->total_in >= tx->total_out)
                   ? (tx->total_in - tx->total_out) : 0;
    format_amount(G_ctx.fee_str, sizeof(G_ctx.fee_str), fee);

    format_chain_label(G_ctx.shard_str, sizeof(G_ctx.shard_str), tx->chain_id);

    if (tx->n_outputs == 1) {
        bytes_to_hex(G_ctx.recipient_str, tx->outputs[0].recipient, 32);
    } else {
        snprintf(G_ctx.recipient_str, sizeof(G_ctx.recipient_str),
                 "%u outputs", (unsigned)tx->n_outputs);
    }

    snprintf(G_ctx.outputs_str, sizeof(G_ctx.outputs_str),
             "input %u of %u", (unsigned)tx->input_index + 1, (unsigned)tx->n_inputs);
}

// ── Sign and write response ───────────────────────────────────────────────────

void finish_sign_tx(bool approved) {
    if (!approved) {
        G_ctx.state = STATE_IDLE;
        send_sw(s_tx_len, SW_USER_REJECTED);
        return;
    }

    uint8_t sighash[32];
    if (shisha_compute_sighash_v2(&G_ctx.tx, sighash) != 0) {
        G_ctx.state = STATE_IDLE;
        send_sw(s_tx_len, SW_INVALID_DATA);
        return;
    }

    uint8_t sig[64];
    size_t  sig_len = sizeof(sig);
    if (schnorr_sign(&G_ctx.bip32, sighash, sig, &sig_len) != 0) {
        G_ctx.state = STATE_IDLE;
        send_sw(s_tx_len, SW_INVALID_DATA);
        return;
    }

    memcpy(G_io_apdu_buffer + *s_tx_len, sig, sig_len);
    (*s_tx_len) += (volatile unsigned int)sig_len;
    send_sw(s_tx_len, SW_OK);
    G_ctx.state = STATE_IDLE;
}

void user_action_sign_flow_approved(void) {
    finish_sign_tx(true);
    *s_flags |= IO_RETURN_AFTER_TX;
}

void user_action_sign_flow_rejected(void) {
    finish_sign_tx(false);
    *s_flags |= IO_RETURN_AFTER_TX;
}

// ── Entry point ───────────────────────────────────────────────────────────────

void handle_sign_tx(uint8_t p1, uint8_t p2,
                    const uint8_t *data, uint16_t data_len,
                    volatile unsigned int *flags,
                    volatile unsigned int *tx) {
    bool is_first = (p1 == P1_FIRST_CHUNK);
    bool is_last  = (p2 == P2_LAST);

    if (!is_first && p1 != P1_NEXT_CHUNK) {
        send_sw(tx, SW_WRONG_P1_P2); return;
    }
    if (p2 != P2_MORE && p2 != P2_LAST) {
        send_sw(tx, SW_WRONG_P1_P2); return;
    }

    if (is_first) {
        G_ctx.state       = STATE_SIGNING;
        G_ctx.payload_len = 0;

        // First chunk starts with BIP32 path: 1-byte depth + depth*4 bytes.
        if (data_len < 1) { send_sw(tx, SW_WRONG_DATA_LENGTH); return; }
        uint8_t  path_depth = data[0];
        uint16_t path_bytes = 1 + (uint16_t)path_depth * 4;

        if (data_len < path_bytes) { send_sw(tx, SW_WRONG_DATA_LENGTH); return; }
        if (parse_bip32_path(data, path_bytes, &G_ctx.bip32) != 0) {
            send_sw(tx, SW_INVALID_DATA); return;
        }

        const uint8_t *payload_start = data + path_bytes;
        uint16_t       payload_part  = data_len - path_bytes;

        if (payload_part > SHISHA_MAX_PAYLOAD_LEN) {
            send_sw(tx, SW_WRONG_DATA_LENGTH); return;
        }
        memcpy(G_ctx.payload, payload_start, payload_part);
        G_ctx.payload_len = payload_part;

    } else {
        if (G_ctx.state != STATE_SIGNING) {
            send_sw(tx, SW_WRONG_P1_P2); return;
        }
        if ((uint32_t)G_ctx.payload_len + data_len > SHISHA_MAX_PAYLOAD_LEN) {
            send_sw(tx, SW_WRONG_DATA_LENGTH); return;
        }
        memcpy(G_ctx.payload + G_ctx.payload_len, data, data_len);
        G_ctx.payload_len += data_len;
    }

    if (!is_last) {
        send_sw(tx, SW_OK); return;
    }

    // Full payload received — parse it.
    if (shisha_parse_payload(G_ctx.payload, G_ctx.payload_len, &G_ctx.tx) != 0) {
        G_ctx.state = STATE_IDLE;
        send_sw(tx, SW_INVALID_DATA); return;
    }

    // TX-11: there is exactly one transaction version (1); the node refuses anything else.
    if (G_ctx.tx.tx_version != 1) {
        G_ctx.state = STATE_IDLE;
        send_sw(tx, SW_INVALID_DATA); return;
    }

    // Validate fee (total_in must cover total_out)
    if (G_ctx.tx.total_in < G_ctx.tx.total_out) {
        G_ctx.state = STATE_IDLE;
        send_sw(tx, SW_INVALID_DATA); return;
    }

    prepare_display_strings();

    s_flags  = flags;
    s_tx_len = tx;

    ui_display_sign_tx();
    *flags |= IO_ASYNCH_REPLY;
}
