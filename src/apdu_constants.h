#pragma once

// ── CLA / INS ─────────────────────────────────────────────────────────────────

#define CLA                    0xE0

#define INS_GET_APP_NAME       0x00
#define INS_GET_VERSION        0x01
#define INS_GET_PUBLIC_KEY     0x02
// 0x04 (SIGN_TRANSACTION, v1 sighash) was removed in app 2.0.0 before any
// network ran v1; it now answers SW_INS_NOT_SUPPORTED.
#define INS_SIGN_TRANSACTION_V2 0x05  // sighash v2; input records carry the prevout recipient

// GET_PUBLIC_KEY P1
#define P1_NO_CONFIRM          0x00
#define P1_CONFIRM             0x01

// SIGN_TRANSACTION P1 / P2
#define P1_FIRST_CHUNK         0x00
#define P1_NEXT_CHUNK          0x80
#define P2_MORE                0x00
#define P2_LAST                0x80

// ── Status words ──────────────────────────────────────────────────────────────

#define SW_OK                  0x9000
#define SW_USER_REJECTED       0x6985
#define SW_WRONG_P1_P2         0x6B00
#define SW_WRONG_DATA_LENGTH   0x6700
#define SW_INS_NOT_SUPPORTED   0x6D00
#define SW_CLA_NOT_SUPPORTED   0x6E00
#define SW_INVALID_DATA        0x6A80
#define SW_WRONG_RESPONSE_LEN  0xB000

// ── Shisha transaction signing payload ───────────────────────────────────────
//
// The host streams this payload in chunks (APDU P1/P2 protocol above).
// The device accumulates, parses, shows the UX, then computes and returns the
// Schnorr signature.
//
// The sighash computed on-device MUST match the node's sighash v2
// (common-types `sighash_v2`, `prevouts_hash`, `txid_from_parts`):
//
//   txid      = BLAKE3("shisha/txid/v1" || chain_id (u16 LE) || tx_version || locktime
//                      || expiration_height || n_inputs || inputs || n_outputs
//                      || outputs || n_ext || extension records)   (validity record skipped)
//   prevouts  = BLAKE3("shisha/prevouts/v1" || n_inputs || [value || recipient] x n)
//   sighash   = BLAKE3("shisha/sighash/v2" || txid || prevouts || input_index)
//
// The spent outputs' values and recipients are committed: a host that lies
// about an input value (to hide the fee on screen) makes the signature invalid.
//
// Payload layout (INS_SIGN_TRANSACTION_V2; all integers little-endian):
//
//   ┌─ Fixed header (27 bytes) ──────────────────────────────────┐
//   │  chain_id          : u16  (2 B)  — beacon 0, shard N = N+1 │
//   │  tx_version        : u8   (1 B)  — must be 1               │
//   │  locktime          : u32  (4 B)  — min inclusion height     │
//   │  expiration_height : u32  (4 B)  — max inclusion height     │
//   │  input_index       : u32  (4 B)  — which input to sign      │
//   │  n_inputs          : u32  (4 B)                             │
//   │  n_outputs         : u32  (4 B)                             │
//   │  n_ext_bytes       : u32  (4 B)  — total extension bytes    │
//   └────────────────────────────────────────────────────────────┘
//   ┌─ Per-input records (46 bytes × n_inputs) ──────────────────┐
//   │  prev_height       : u32  (4 B)                             │
//   │  prev_output_idx   : u16  (2 B)                             │
//   │  prevout_value     : u64  (8 B)  — committed (prevouts)     │
//   │  prevout_recipient : [u8;32]     — committed (prevouts)     │
//   └────────────────────────────────────────────────────────────┘
//   ┌─ Per-output records (40 bytes × n_outputs) ────────────────┐
//   │  value             : u64  (8 B)                             │
//   │  recipient         : [u8;32]     — x-only secp256k1 pubkey  │
//   └────────────────────────────────────────────────────────────┘
//   ┌─ Extension data (n_ext_bytes bytes) ───────────────────────┐
//   │  Whole ExtensionRecords: type u16, version u16, len u32,    │
//   │  payload. Hashed into the txid (validity record skipped).   │
//   └────────────────────────────────────────────────────────────┘
//
// Constraints:
//   MAX_INPUTS   ≤ SHISHA_MAX_INPUTS
//   MAX_OUTPUTS  ≤ SHISHA_MAX_OUTPUTS
//   n_ext_bytes  ≤ SHISHA_MAX_EXT_BYTES
//
// Maximum payload = 27 + 46×8 + 40×32 + 256 = 1931 bytes.
// Streamed in 250-byte APDU chunks (8 chunks worst case).

#define SHISHA_ATOMS_PER_COIN   1000000ULL

#define SHISHA_MAX_INPUTS       8
#define SHISHA_MAX_OUTPUTS      32
#define SHISHA_MAX_EXT_BYTES    256

// Fixed header length (bytes before the first input record)
#define SHISHA_HEADER_LEN       27

// Per-record sizes
#define SHISHA_INPUT_RECORD_LEN_V2 46 // prev_height(4) + prev_output_idx(2) + prevout_value(8) + prevout_recipient(32)
#define SHISHA_OUTPUT_RECORD_LEN 40   // value(8) + recipient(32)

// Max total payload the device will buffer
#define SHISHA_MAX_PAYLOAD_LEN \
    (SHISHA_HEADER_LEN + \
     SHISHA_MAX_INPUTS  * SHISHA_INPUT_RECORD_LEN_V2 + \
     SHISHA_MAX_OUTPUTS * SHISHA_OUTPUT_RECORD_LEN + \
     SHISHA_MAX_EXT_BYTES)
