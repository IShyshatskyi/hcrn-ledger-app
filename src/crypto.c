// BIP32 derivation + Schnorr signing for the Shisha Ledger app

#include "globals.h"

// ── BIP32 path parser ─────────────────────────────────────────────────────────
//
// Wire format (from ledger.rs encode_bip32_path):
//   [count: 1 byte][level0: 4 bytes BE][level1: 4 bytes BE]...
//
// Returns 0 on success, -1 on invalid input.

int parse_bip32_path(const uint8_t *data, uint16_t data_len, bip32_path_t *out) {
    if (data_len < 1) return -1;

    uint8_t depth = data[0];
    if (depth == 0 || depth > MAX_BIP32_DEPTH) return -1;
    if (data_len < (uint16_t)(1 + depth * 4)) return -1;

    out->path_len = depth;
    for (uint8_t i = 0; i < depth; i++) {
        out->path[i] = U4BE(data, 1 + i * 4);
    }
    return 0;
}

// ── Derive key pair at bip32 path ─────────────────────────────────────────────
//
// Fills privkey and pubkey from the device seed + BIP32 path.
// pubkey is in compressed SEC1 form: [parity(1)] [x(32)].

int derive_keypair(const bip32_path_t *path,
                   cx_ecfp_private_key_t *privkey,
                   cx_ecfp_public_key_t  *pubkey) {
    uint8_t privkey_bytes[32];
    uint8_t chain_code[32];
    cx_err_t err;

    // os_perso_derive_node_bip32 fills privkey_bytes and chain_code.
    os_perso_derive_node_bip32(CX_CURVE_256K1,
                               path->path, path->path_len,
                               privkey_bytes, chain_code);

    err = cx_ecfp_init_private_key_no_throw(CX_CURVE_256K1,
                                             privkey_bytes, 32,
                                             privkey);
    explicit_bzero(privkey_bytes, sizeof(privkey_bytes));
    if (err != CX_OK) return -1;

    err = cx_ecfp_generate_pair_no_throw(CX_CURVE_256K1, pubkey, privkey,
                                          /*keep_private=*/1);
    if (err != CX_OK) return -1;

    // pubkey->W is uncompressed: [0x04][x(32)][y(32)] (65 bytes).
    // Convert to compressed [parity][x(32)].
    uint8_t parity = (pubkey->W[64] & 1) ? 0x03 : 0x02;
    pubkey->W[0] = parity;
    pubkey->W_len = 33;

    return 0;
}

// ── x-only public key (BIP-340 / Shisha address) ─────────────────────────────
//
// Extracts the 32-byte x coordinate from a (just-derived) compressed pubkey.
// pubkey->W must be in compressed form after derive_keypair().

void get_x_only_pubkey(const cx_ecfp_public_key_t *pubkey, uint8_t *x32_out) {
    // compressed: W[0]=parity, W[1..32]=x
    memcpy(x32_out, pubkey->W + 1, 32);
}

// ── SHA-256 of the Shisha signing payload ─────────────────────────────────────

int hash_payload(const uint8_t *payload, uint16_t payload_len, uint8_t *hash32_out) {
    cx_sha256_t sha;
    cx_err_t err;

    err = cx_sha256_init_no_throw(&sha);
    if (err != CX_OK) return -1;

    err = cx_hash_no_throw((cx_hash_t *)&sha, CX_LAST,
                            payload, payload_len,
                            hash32_out, 32);
    if (err != CX_OK) return -1;
    return 0;
}

// ── BIP-340 Schnorr signature ─────────────────────────────────────────────────
//
// Signs `msg_hash` (32 bytes) with the private key at `path`.
// Writes the 64-byte (r || s) signature to `sig_out`.
//
// Uses CX_ECSCHNORR_BIP0340 which handles the BIP-340 nonce derivation
// (tagged hash for k) internally.  The message must already be the
// 32-byte hash of the data to sign — the device does not hash it again.

int schnorr_sign(const bip32_path_t *path,
                 const uint8_t *msg_hash,  // 32 bytes
                 uint8_t *sig_out,         // 64 bytes
                 size_t  *sig_len_out) {
    cx_ecfp_private_key_t privkey;
    cx_ecfp_public_key_t  pubkey;

    if (derive_keypair(path, &privkey, &pubkey) != 0) {
        explicit_bzero(&privkey, sizeof(privkey));
        return -1;
    }

    *sig_len_out = 64;
    cx_err_t err = cx_ecschnorr_sign_no_throw(
        &privkey,
        CX_ECSCHNORR_BIP0340 | CX_RND_RFC6979,
        CX_SHA256,
        msg_hash, 32,
        sig_out, sig_len_out,
        NULL
    );

    explicit_bzero(&privkey, sizeof(privkey));
    return (err == CX_OK) ? 0 : -1;
}
