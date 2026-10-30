// Signing-payload parser. No BOLOS SDK dependency: also built by
// tests/host/test_sighash.c. Layout: apdu_constants.h.

#include "shisha_tx.h"

static uint16_t u16le(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t u64le(const uint8_t *p) {
    return (uint64_t)u32le(p) | ((uint64_t)u32le(p + 4) << 32);
}

int shisha_parse_payload(const uint8_t *buf, uint32_t len, shisha_tx_t *tx) {
    if (len < SHISHA_HEADER_LEN) return -1;

    uint32_t off = 0;
    tx->chain_id          = u16le(buf + off); off += 2;
    tx->tx_version        = buf[off++];
    tx->locktime          = u32le(buf + off); off += 4;
    tx->expiration_height = u32le(buf + off); off += 4;
    tx->input_index       = u32le(buf + off); off += 4;
    tx->n_inputs          = u32le(buf + off); off += 4;
    tx->n_outputs         = u32le(buf + off); off += 4;
    tx->n_ext_bytes       = u32le(buf + off); off += 4;

    if (tx->n_inputs  > SHISHA_MAX_INPUTS)      return -1;
    if (tx->n_outputs > SHISHA_MAX_OUTPUTS)     return -1;
    if (tx->n_ext_bytes > SHISHA_MAX_EXT_BYTES) return -1;
    if (tx->input_index >= tx->n_inputs)        return -1;

    uint32_t in_rec = SHISHA_INPUT_RECORD_LEN_V2;
    uint32_t expected_len = SHISHA_HEADER_LEN
        + tx->n_inputs  * in_rec
        + tx->n_outputs * SHISHA_OUTPUT_RECORD_LEN
        + tx->n_ext_bytes;
    if (len != expected_len) return -1;

    tx->total_in = 0;
    for (uint32_t i = 0; i < tx->n_inputs; i++) {
        shisha_input_t *in = &tx->inputs[i];
        in->prev_height     = u32le(buf + off); off += 4;
        in->prev_output_idx = u16le(buf + off); off += 2;
        in->prevout_value   = u64le(buf + off); off += 8;
        for (int k = 0; k < 32; k++) in->prevout_recipient[k] = buf[off + k];
        off += 32;
        if (tx->total_in + in->prevout_value < tx->total_in) return -1;  // overflow
        tx->total_in += in->prevout_value;
    }

    tx->total_out = 0;
    for (uint32_t i = 0; i < tx->n_outputs; i++) {
        tx->outputs[i].value = u64le(buf + off); off += 8;
        for (int k = 0; k < 32; k++) tx->outputs[i].recipient[k] = buf[off + k];
        off += 32;
        if (tx->total_out + tx->outputs[i].value < tx->total_out) return -1;  // overflow
        tx->total_out += tx->outputs[i].value;
    }

    for (uint32_t i = 0; i < tx->n_ext_bytes; i++) tx->ext_bytes[i] = buf[off + i];
    return 0;
}
