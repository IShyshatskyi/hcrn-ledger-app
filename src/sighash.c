// Sighash v2 and chain label. No BOLOS SDK dependency: also built by
// tests/host/test_sighash.c.

#include "shisha_tx.h"
#include "blake3/blake3.h"

static void put_u32(blake3_hasher *h, uint32_t v) {
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    blake3_hasher_update(h, b, 4);
}

static void put_u64(blake3_hasher *h, uint64_t v) {
    uint8_t b[8];
    for (int i = 0; i < 8; i++) b[i] = (uint8_t)(v >> (8 * i));
    blake3_hasher_update(h, b, 8);
}

// ── VF-3 Phase 2: sighash v2 ─────────────────────────────────────────────────

#define EXT_VALIDITY_WINDOW 0x0001u

static void put_u16(blake3_hasher *h, uint16_t v) {
    uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
    blake3_hasher_update(h, b, 2);
}

static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Walk ext_bytes as whole records. With `h` NULL: count the records the
// txid hashes. Otherwise hash them. Returns the count, or -1 if malformed.
static int32_t walk_extensions(const shisha_tx_t *tx, blake3_hasher *h) {
    uint32_t off = 0;
    int32_t n = 0;
    while (off < tx->n_ext_bytes) {
        if (tx->n_ext_bytes - off < 8) return -1;
        const uint8_t *r = tx->ext_bytes + off;
        uint16_t type = (uint16_t)(r[0] | (r[1] << 8));
        uint16_t ver  = (uint16_t)(r[2] | (r[3] << 8));
        uint32_t plen = rd_u32(r + 4);
        if (plen > tx->n_ext_bytes - off - 8) return -1;
        if (type != EXT_VALIDITY_WINDOW) {
            if (h) {
                put_u16(h, type);
                put_u16(h, ver);
                put_u32(h, plen);
                blake3_hasher_update(h, r + 8, plen);
            }
            n++;
        }
        off += 8 + plen;
    }
    return n;
}

int shisha_compute_txid(const shisha_tx_t *tx, uint8_t out[32]) {
    int32_t n_ext = walk_extensions(tx, NULL);
    if (n_ext < 0) return -1;

    blake3_hasher h;
    blake3_hasher_init(&h);
    blake3_hasher_update(&h, "shisha/txid/v1", 14);
    uint8_t b[3] = { (uint8_t)(tx->chain_id & 0xFF), (uint8_t)(tx->chain_id >> 8), tx->tx_version };
    blake3_hasher_update(&h, b, 3);
    put_u32(&h, tx->locktime);
    put_u32(&h, tx->expiration_height);

    put_u32(&h, tx->n_inputs);
    for (uint32_t i = 0; i < tx->n_inputs; i++) {
        put_u32(&h, tx->inputs[i].prev_height);
        put_u16(&h, tx->inputs[i].prev_output_idx);
    }
    put_u32(&h, tx->n_outputs);
    for (uint32_t i = 0; i < tx->n_outputs; i++) {
        put_u64(&h, tx->outputs[i].value);
        blake3_hasher_update(&h, tx->outputs[i].recipient, 32);
    }
    put_u32(&h, (uint32_t)n_ext);
    walk_extensions(tx, &h);

    blake3_hasher_finalize(&h, out, 32);
    return 0;
}

void shisha_compute_prevouts_hash(const shisha_tx_t *tx, uint8_t out[32]) {
    blake3_hasher h;
    blake3_hasher_init(&h);
    blake3_hasher_update(&h, "shisha/prevouts/v1", 18);
    put_u32(&h, tx->n_inputs);
    for (uint32_t i = 0; i < tx->n_inputs; i++) {
        put_u64(&h, tx->inputs[i].prevout_value);
        blake3_hasher_update(&h, tx->inputs[i].prevout_recipient, 32);
    }
    blake3_hasher_finalize(&h, out, 32);
}

int shisha_compute_sighash_v2(const shisha_tx_t *tx, uint8_t out[32]) {
    uint8_t txid[32], prev[32];
    if (shisha_compute_txid(tx, txid) != 0) return -1;
    shisha_compute_prevouts_hash(tx, prev);

    blake3_hasher h;
    blake3_hasher_init(&h);
    blake3_hasher_update(&h, "shisha/sighash/v2", 17);
    blake3_hasher_update(&h, txid, 32);
    blake3_hasher_update(&h, prev, 32);
    put_u32(&h, tx->input_index);
    blake3_hasher_finalize(&h, out, 32);
    return 0;
}

void format_chain_label(char *out, size_t out_len, uint16_t chain_id) {
    // No snprintf: keeps this file free of libc stdio for the BOLOS build.
    char buf[12];
    size_t n = 0;
    if (chain_id == 0) {
        const char *b = "beacon";
        while (b[n]) { buf[n] = b[n]; n++; }
    } else {
        const char *p = "shard ";
        while (p[n]) { buf[n] = p[n]; n++; }
        char d[5];
        int k = 0;
        unsigned v = chain_id;
        do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v);
        while (k) buf[n++] = d[--k];
    }
    if (out_len == 0) return;
    if (n >= out_len) n = out_len - 1;
    for (size_t i = 0; i < n; i++) out[i] = buf[i];
    out[n] = '\0';
}
