// Parsed Shisha transaction types and sighash v2.
//
// Kept free of BOLOS SDK headers so tests/host can compile and test the
// sighash and display helpers on a normal host compiler.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "apdu_constants.h"

// ── Per-input record (display + sighash) ──────────────────────────────────────

typedef struct {
    uint32_t  prev_height;
    uint16_t  prev_output_idx;
    uint64_t  prevout_value;         // committed (prevouts_hash)
    uint8_t   prevout_recipient[32]; // committed (prevouts_hash)
} shisha_input_t;

// ── Per-output record ─────────────────────────────────────────────────────────

typedef struct {
    uint64_t value;
    uint8_t  recipient[32];   // x-only secp256k1 pubkey = Shisha address
} shisha_output_t;

// ── Parsed transaction ────────────────────────────────────────────────────────

typedef struct {
    uint16_t  chain_id;
    uint8_t   tx_version;
    uint32_t  locktime;
    uint32_t  expiration_height;
    uint32_t  input_index;       // which input this signing round covers

    uint32_t  n_inputs;
    uint32_t  n_outputs;
    uint32_t  n_ext_bytes;

    shisha_input_t  inputs[SHISHA_MAX_INPUTS];
    shisha_output_t outputs[SHISHA_MAX_OUTPUTS];

    // Extension records (ShortAddrHints etc.), hashed into the txid.
    uint8_t   ext_bytes[SHISHA_MAX_EXT_BYTES];

    // Computed for display
    uint64_t  total_in;    // Σ prevout_values
    uint64_t  total_out;   // Σ output values
    // fee = total_in - total_out (computed after parse)
} shisha_tx_t;

// Parse a signing payload (46-byte input records, apdu_constants.h).
// Returns 0 on success, -1 if malformed or over the limits.
int shisha_parse_payload(const uint8_t *buf, uint32_t len, shisha_tx_t *tx);

// VF-3 txid (common-types txid_from_parts): BLAKE3("shisha/txid/v1" || chain_id
//   || version || locktime || expiration || n_in || inputs || n_out || outputs
//   || n_ext || [type u16 || version u16 || len u32 || payload] x k), skipping
//   the validity-window record (type 0x0001). ext_bytes must be whole records.
// Returns 0, or -1 if ext_bytes is not a sequence of whole records.
int shisha_compute_txid(const shisha_tx_t *tx, uint8_t out[32]);

// BLAKE3("shisha/prevouts/v1" || n:u32 || [value u64 || recipient] x n).
void shisha_compute_prevouts_hash(const shisha_tx_t *tx, uint8_t out[32]);

// BLAKE3("shisha/sighash/v2" || txid || prevouts_hash || input_index:u32).
int shisha_compute_sighash_v2(const shisha_tx_t *tx, uint8_t out[32]);


// Display label for a chain_id: 0 -> "beacon", otherwise "shard <chain_id>".
// chain_id = shard_id + 1, and every Shisha UI shows shards 1-indexed
// ("Shard 1" = shard_id 0), so the label number equals chain_id.
void format_chain_label(char *out, size_t out_len, uint16_t chain_id);
