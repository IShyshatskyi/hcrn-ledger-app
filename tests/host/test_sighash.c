// Host-side test of the device's sighash v2, BLAKE3, payload parser and chain label.
// Build: cc -std=c99 -Wall -Werror -I src tests/host/test_sighash.c src/sighash.c src/payload.c
//
// The sighash v2 vectors are the ones common-types pins in
// crates/common-types/src/common/transaction/txid.rs (sighash_v2_vectors),
// also pinned by tools/tx-spammer and the light-client copy. If they change
// there, change them here in the same commit.

#include <stdio.h>
#include <string.h>
#include "shisha_tx.h"
#include "blake3/blake3.h"

static int failures = 0;

static void hex(const uint8_t *b, char *out) {
    for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", b[i]);
}

static void base_tx(shisha_tx_t *tx) {
    memset(tx, 0, sizeof(*tx));
    tx->chain_id = 7;
    tx->tx_version = 1;
    tx->n_inputs = 1;
    tx->inputs[0].prev_height = 1;
    tx->inputs[0].prev_output_idx = 0;
    tx->inputs[0].prevout_value = 12345;   // display-only; must not change the hash
    tx->n_outputs = 1;
    tx->outputs[0].value = 100;
    memset(tx->outputs[0].recipient, 7, 32);
    tx->input_index = 0;
}

static void expect_label(uint16_t chain_id, const char *want) {
    char out[12];
    format_chain_label(out, sizeof(out), chain_id);
    if (strcmp(out, want) != 0) {
        printf("FAIL label(%u): got \"%s\" want \"%s\"\n", chain_id, out, want);
        failures++;
    } else {
        printf("ok   label(%u) = \"%s\"\n", chain_id, out);
    }
}

// ── BLAKE3 across block and chunk boundaries (input i % 251) ────────────────
// Expected values from the Rust blake3 crate (1.8.7). The old single-chunk
// implementation got every length above 1024 wrong.
static const struct { size_t n; const char *hex; } B3[] = {
    { 0, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262" },
    { 1, "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213" },
    { 63, "e9bc37a594daad83be9470df7f7b3798297c3d834ce80ba85d6e207627b7db7b" },
    { 64, "4eed7141ea4a5cd4b788606bd23f46e212af9cacebacdc7d1f4c6dc7f2511b98" },
    { 65, "de1e5fa0be70df6d2be8fffd0e99ceaa8eb6e8c93a63f2d8d1c30ecb6b263dee" },
    { 1023, "10108970eeda3eb932baac1428c7a2163b0e924c9a9e25b35bba72b28f70bd11" },
    { 1024, "42214739f095a406f3fc83deb889744ac00df831c10daa55189b5d121c855af7" },
    { 1025, "d00278ae47eb27b34faecf67b4fe263f82d5412916c1ffd97c8cb7fb814b8444" },
    { 2048, "e776b6028c7cd22a4d0ba182a8bf62205d2ef576467e838ed6f2529b85fba24a" },
    { 2049, "5f4d72f40d7a5f82b15ca2b2e44b1de3c2ef86c426c95c1af0b6879522563030" },
    { 3072, "b98cb0ff3623be03326b373de6b9095218513e64f1ee2edd2525c7ad1e5cffd2" },
    { 5000, "ee78d92070de3df1c57c37002abf0a6b1a6589acdeef4d8ffac7cf3d9e8f2836" },
};

static void check_hex(const char *name, const uint8_t h[32], const char *want) {
    char got[65];
    hex(h, got);
    if (strcmp(got, want) != 0) {
        printf("FAIL %s: got %s want %s\n", name, got, want);
        failures++;
    } else {
        printf("ok   %s\n", name);
    }
}

static void test_blake3(void) {
    static uint8_t buf[5000];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)(i % 251);
    for (size_t k = 0; k < sizeof(B3) / sizeof(B3[0]); k++) {
        for (size_t step = 1; step <= 1100; step += 1099) {   // byte-wise, then 1100-byte pieces
            blake3_hasher h;
            blake3_hasher_init(&h);
            for (size_t off = 0; off < B3[k].n; off += step) {
                size_t take = B3[k].n - off < step ? B3[k].n - off : step;
                blake3_hasher_update(&h, buf + off, take);
            }
            uint8_t out[32];
            blake3_hasher_finalize(&h, out, 32);
            char name[48];
            sprintf(name, "blake3 len %u step %u", (unsigned)B3[k].n, (unsigned)step);
            check_hex(name, out, B3[k].hex);
        }
    }
}

// ── VF-3 Phase 2 vectors (common-types txid.rs::sighash_v2_vectors) ─────────

static void v2_base(shisha_tx_t *tx, uint16_t chain_id) {
    base_tx(tx);
    tx->chain_id = chain_id;
    tx->inputs[0].prevout_value = 1000;
    memset(tx->inputs[0].prevout_recipient, 9, 32);
}

static void test_v2_vectors(void) {
    shisha_tx_t tx;
    uint8_t h[32];

    v2_base(&tx, 0);
    shisha_compute_prevouts_hash(&tx, h);
    check_hex("V1 prevouts_hash", h, "09d0937f5d233942e5bb94d6dadde0d92d43bfff3c7bc178ae2c64c3f792728b");
    shisha_compute_sighash_v2(&tx, h);
    check_hex("V2 beacon", h, "d8a8766d6ac64a127729917dc052e354206add9245effc131fef5dee74ce0752");

    v2_base(&tx, 1);
    shisha_compute_sighash_v2(&tx, h);
    check_hex("V3 shard 0", h, "40c2e9fe25b385064a95c81b61f7baa129d617bcc0d5514b05953f55b6328f96");

    // V4: + application extension 0x1234 v1, payload [1].
    static const uint8_t app_ext[9] = { 0x34, 0x12, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01 };
    memcpy(tx.ext_bytes, app_ext, 9);
    tx.n_ext_bytes = 9;
    shisha_compute_sighash_v2(&tx, h);
    check_hex("V4 + extension", h, "3e0ff2935ada6ea04f0be68b996790a218af91cf88e1809288b7cf10aebedf74");

    // V5: a validity-window record (type 0x0001) is not hashed into the txid.
    v2_base(&tx, 1);
    static const uint8_t validity[17] = { 0x01, 0x00, 0x01, 0x00, 0x09, 0x00, 0x00, 0x00,
                                          1, 0, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(tx.ext_bytes, validity, 17);
    tx.n_ext_bytes = 17;
    shisha_compute_sighash_v2(&tx, h);
    check_hex("V5 validity record ignored", h, "40c2e9fe25b385064a95c81b61f7baa129d617bcc0d5514b05953f55b6328f96");

    // V6: 2 inputs, signing input 1.
    v2_base(&tx, 1);
    tx.n_inputs = 2;
    tx.inputs[1].prev_height = 2;
    tx.inputs[1].prev_output_idx = 3;
    tx.inputs[1].prevout_value = 500;
    memset(tx.inputs[1].prevout_recipient, 8, 32);
    tx.input_index = 1;
    shisha_compute_sighash_v2(&tx, h);
    check_hex("V6 input 1", h, "2a8762df4154b5a830a909efba5633556054f854ff8f72aa2491b8c8d1dc5463");

    // Malformed extension bytes (truncated record) are refused, not hashed.
    v2_base(&tx, 1);
    memcpy(tx.ext_bytes, app_ext, 8);
    tx.n_ext_bytes = 8;
    if (shisha_compute_sighash_v2(&tx, h) != -1) { printf("FAIL truncated extension accepted\n"); failures++; }
    else printf("ok   truncated extension refused\n");
}

// A full v2 payload, byte-identical to filament_app ledger.rs
// payload_tests::v2_payload_layout_matches_device_test, through the parser:
// must reproduce V8 (V3 at version 2).
static void test_v2_payload(void) {
    uint8_t p[27 + 46 + 40];
    memset(p, 0, sizeof(p));
    // header: chain(2) ver(1) lock(4) exp(4) input_index(4) n_in(4) n_out(4) n_ext(4)
    memset(p, 0, 27);
    p[0] = 1; p[1] = 0; p[2] = 2;   // chain_id 1 (shard 0), tx_version 2
    p[15] = 1;                // n_inputs
    p[19] = 1;                // n_outputs
    uint8_t *in = p + 27;
    in[0] = 1;                // prev_height = 1
    in[6] = 0xE8; in[7] = 0x03;   // prevout_value = 1000
    memset(in + 14, 9, 32);   // prevout_recipient
    uint8_t *out = in + 46;
    out[0] = 100;             // value
    memset(out + 8, 7, 32);   // recipient

    shisha_tx_t tx;
    uint8_t h[32];
    if (shisha_parse_payload(p, sizeof(p), &tx) != 0) {
        printf("FAIL v2 payload did not parse\n"); failures++; return;
    }
    shisha_compute_sighash_v2(&tx, h);
    check_hex("v2 payload -> V8", h, "6b295c3692527389e268448d59c29be251bcb7e70c7f8afd814908d02a5f208d");

    if (shisha_parse_payload(p, sizeof(p) - 1, &tx) == 0) {
        printf("FAIL short payload accepted\n"); failures++;
    } else printf("ok   short payload refused\n");
    // A 14-byte-record (old v1) payload of the same transaction is refused.
    if (shisha_parse_payload(p, sizeof(p) - 32, &tx) == 0) {
        printf("FAIL old v1-framed payload accepted\n"); failures++;
    } else printf("ok   old v1-framed payload refused\n");
}

int main(void) {
    // chain_id 0 is the beacon; shards are 1-indexed in every UI (= chain_id).
    expect_label(0, "beacon");
    expect_label(1, "shard 1");
    expect_label(16, "shard 16");
    expect_label(255, "shard 255");
    expect_label(256, "shard 256");
    expect_label(1024, "shard 1024");

    test_blake3();
    test_v2_vectors();
    test_v2_payload();

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
