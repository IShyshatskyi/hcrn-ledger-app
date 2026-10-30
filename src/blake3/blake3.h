// BLAKE3 reference implementation — portable C, single-threaded, no SIMD.
// Adapted from the official BLAKE3 reference C code (CC0 / public domain).
// https://github.com/BLAKE3-team/BLAKE3/tree/master/c
//
// Only the streaming API is exposed here (hasher_init / hasher_update /
// hasher_finalize). This is sufficient for computing the Shisha sighash on
// resource-constrained BOLOS devices (Nano S / Nano X / Nano S+).

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ── Internal constants ────────────────────────────────────────────────────────

#define BLAKE3_OUT_LEN       32
#define BLAKE3_KEY_LEN       32
#define BLAKE3_BLOCK_LEN     64
#define BLAKE3_CHUNK_LEN     1024

#define BLAKE3_FLAG_CHUNK_START  (1u << 0)
#define BLAKE3_FLAG_CHUNK_END    (1u << 1)
#define BLAKE3_FLAG_ROOT         (1u << 3)

// IV = first 8 words of SHA-256 (same as BLAKE3 spec)
static const uint32_t BLAKE3_IV[8] = {
    0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
    0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u,
};

static const uint8_t MSG_PERMUTATION[16] = {
    2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8
};

// ── Compression function ──────────────────────────────────────────────────────

static inline uint32_t rotr32(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static inline void g(uint32_t *state, int a, int b, int c, int d,
                     uint32_t x, uint32_t y) {
    state[a] = state[a] + state[b] + x;
    state[d] = rotr32(state[d] ^ state[a], 16);
    state[c] = state[c] + state[d];
    state[b] = rotr32(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + y;
    state[d] = rotr32(state[d] ^ state[a], 8);
    state[c] = state[c] + state[d];
    state[b] = rotr32(state[b] ^ state[c], 7);
}

static inline void round_fn(uint32_t *state, const uint32_t *m) {
    g(state, 0, 4,  8, 12, m[0],  m[1]);
    g(state, 1, 5,  9, 13, m[2],  m[3]);
    g(state, 2, 6, 10, 14, m[4],  m[5]);
    g(state, 3, 7, 11, 15, m[6],  m[7]);
    g(state, 0, 5, 10, 15, m[8],  m[9]);
    g(state, 1, 6, 11, 12, m[10], m[11]);
    g(state, 2, 7,  8, 13, m[12], m[13]);
    g(state, 3, 4,  9, 14, m[14], m[15]);
}

static void permute(uint32_t *m) {
    uint32_t tmp[16];
    for (int i = 0; i < 16; i++) tmp[i] = m[MSG_PERMUTATION[i]];
    for (int i = 0; i < 16; i++) m[i] = tmp[i];
}

static void compress(
    const uint32_t *chaining_value,
    const uint32_t *block_words,
    uint64_t counter,
    uint32_t block_len,
    uint32_t flags,
    uint32_t out[16])
{
    uint32_t state[16] = {
        chaining_value[0], chaining_value[1], chaining_value[2], chaining_value[3],
        chaining_value[4], chaining_value[5], chaining_value[6], chaining_value[7],
        BLAKE3_IV[0], BLAKE3_IV[1], BLAKE3_IV[2], BLAKE3_IV[3],
        (uint32_t)counter, (uint32_t)(counter >> 32), block_len, flags,
    };
    uint32_t block[16];
    for (int i = 0; i < 16; i++) block[i] = block_words[i];

    for (int round = 0; round < 7; round++) {
        round_fn(state, block);
        permute(block);
    }
    for (int i = 0; i < 8; i++) {
        out[i]   = state[i] ^ state[i + 8];
        out[i+8] = state[i + 8] ^ chaining_value[i];
    }
}

// ── Word helpers ──────────────────────────────────────────────────────────────

static inline uint32_t load32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

static void words_from_block(const uint8_t *block, uint32_t *words) {
    for (int i = 0; i < 16; i++) words[i] = load32le(block + 4*i);
}

static void store_cv(const uint32_t *cv, uint8_t *out) {
    for (int i = 0; i < 8; i++) {
        out[4*i+0] = (uint8_t)(cv[i]);
        out[4*i+1] = (uint8_t)(cv[i] >> 8);
        out[4*i+2] = (uint8_t)(cv[i] >> 16);
        out[4*i+3] = (uint8_t)(cv[i] >> 24);
    }
}

// ── Chunk state ───────────────────────────────────────────────────────────────

#define BLAKE3_FLAG_PARENT       (1u << 2)

typedef struct {
    uint32_t chaining_value[8];
    uint64_t chunk_counter;
    uint8_t  block[BLAKE3_BLOCK_LEN];
    uint8_t  block_len;
    uint8_t  blocks_compressed;
    uint32_t flags;
} blake3_chunk_state;

// A pending compression whose result is either a chaining value or the root.
typedef struct {
    uint32_t input_cv[8];
    uint32_t block_words[16];
    uint64_t counter;
    uint32_t block_len;
    uint32_t flags;
} blake3_output;

static void chunk_state_init(blake3_chunk_state *s, uint64_t chunk_counter, uint32_t flags) {
    for (int i = 0; i < 8; i++) s->chaining_value[i] = BLAKE3_IV[i];
    s->chunk_counter = chunk_counter;
    memset(s->block, 0, sizeof(s->block));
    s->block_len = 0;
    s->blocks_compressed = 0;
    s->flags = flags;
}

static size_t chunk_state_len(const blake3_chunk_state *s) {
    return (size_t)BLAKE3_BLOCK_LEN * s->blocks_compressed + s->block_len;
}

static uint32_t chunk_start_flag(const blake3_chunk_state *s) {
    return s->blocks_compressed == 0 ? BLAKE3_FLAG_CHUNK_START : 0;
}

static void chunk_state_update(blake3_chunk_state *s, const uint8_t *input, size_t input_len) {
    while (input_len > 0) {
        // Compress a full block only when more input follows: the last block
        // of a chunk is compressed by chunk_state_output with CHUNK_END.
        if (s->block_len == BLAKE3_BLOCK_LEN) {
            uint32_t block_words[16];
            words_from_block(s->block, block_words);
            uint32_t out16[16];
            compress(s->chaining_value, block_words, s->chunk_counter, BLAKE3_BLOCK_LEN,
                     s->flags | chunk_start_flag(s), out16);
            for (int i = 0; i < 8; i++) s->chaining_value[i] = out16[i];
            s->blocks_compressed++;
            s->block_len = 0;
            memset(s->block, 0, sizeof(s->block));
        }
        size_t space = BLAKE3_BLOCK_LEN - s->block_len;
        size_t take  = input_len < space ? input_len : space;
        memcpy(s->block + s->block_len, input, take);
        s->block_len += (uint8_t)take;
        input     += take;
        input_len -= take;
    }
}

static blake3_output chunk_state_output(const blake3_chunk_state *s) {
    blake3_output o;
    for (int i = 0; i < 8; i++) o.input_cv[i] = s->chaining_value[i];
    words_from_block(s->block, o.block_words);   // block is zero-padded
    o.counter   = s->chunk_counter;
    o.block_len = s->block_len;
    o.flags     = s->flags | chunk_start_flag(s) | BLAKE3_FLAG_CHUNK_END;
    return o;
}

static void output_chaining_value(const blake3_output *o, uint32_t cv[8]) {
    uint32_t out16[16];
    compress(o->input_cv, o->block_words, o->counter, o->block_len, o->flags, out16);
    for (int i = 0; i < 8; i++) cv[i] = out16[i];
}

static blake3_output parent_output(const uint32_t left[8], const uint32_t right[8]) {
    blake3_output o;
    for (int i = 0; i < 8; i++) o.input_cv[i] = BLAKE3_IV[i];
    for (int i = 0; i < 8; i++) { o.block_words[i] = left[i]; o.block_words[8 + i] = right[i]; }
    o.counter   = 0;
    o.block_len = BLAKE3_BLOCK_LEN;
    o.flags     = BLAKE3_FLAG_PARENT;
    return o;
}

// ── Public hasher API ─────────────────────────────────────────────────────────
//
// Full BLAKE3 (reference algorithm: chunk tree with a chaining-value stack).
// The stack holds up to BLAKE3_MAX_DEPTH entries, enough for inputs below
// 2^BLAKE3_MAX_DEPTH chunks of 1 KiB; the Shisha payload is under 2 KiB.
// The earlier single-chunk version silently produced wrong hashes above
// 1024 bytes (a v1 sighash with ~25+ outputs).

#define BLAKE3_MAX_DEPTH 8

typedef struct {
    blake3_chunk_state chunk;
    uint32_t cv_stack[BLAKE3_MAX_DEPTH][8];
    uint8_t  cv_stack_len;
} blake3_hasher;

static inline void blake3_hasher_init(blake3_hasher *h) {
    chunk_state_init(&h->chunk, 0, 0);
    h->cv_stack_len = 0;
}

static void hasher_add_chunk_cv(blake3_hasher *h, uint32_t new_cv[8], uint64_t total_chunks) {
    // Merge completed subtrees: one parent per trailing zero bit of total_chunks.
    while ((total_chunks & 1) == 0) {
        h->cv_stack_len--;
        blake3_output p = parent_output(h->cv_stack[h->cv_stack_len], new_cv);
        output_chaining_value(&p, new_cv);
        total_chunks >>= 1;
    }
    for (int i = 0; i < 8; i++) h->cv_stack[h->cv_stack_len][i] = new_cv[i];
    h->cv_stack_len++;
}

static inline void blake3_hasher_update(blake3_hasher *h,
                                        const void *input, size_t input_len) {
    const uint8_t *in = (const uint8_t *)input;
    while (input_len > 0) {
        if (chunk_state_len(&h->chunk) == BLAKE3_CHUNK_LEN) {
            blake3_output o = chunk_state_output(&h->chunk);
            uint32_t cv[8];
            output_chaining_value(&o, cv);
            uint64_t total_chunks = h->chunk.chunk_counter + 1;
            hasher_add_chunk_cv(h, cv, total_chunks);
            chunk_state_init(&h->chunk, total_chunks, 0);
        }
        size_t want = BLAKE3_CHUNK_LEN - chunk_state_len(&h->chunk);
        size_t take = input_len < want ? input_len : want;
        chunk_state_update(&h->chunk, in, take);
        in        += take;
        input_len -= take;
    }
}

static inline void blake3_hasher_finalize(const blake3_hasher *h,
                                          uint8_t *out, size_t out_len) {
    blake3_output o = chunk_state_output(&h->chunk);
    size_t remaining = h->cv_stack_len;
    while (remaining > 0) {
        remaining--;
        uint32_t cv[8];
        output_chaining_value(&o, cv);
        o = parent_output(h->cv_stack[remaining], cv);
    }
    uint32_t out16[16];
    compress(o.input_cv, o.block_words, 0, o.block_len, o.flags | BLAKE3_FLAG_ROOT, out16);
    uint8_t tmp[32];
    store_cv(out16, tmp);
    size_t copy = out_len < 32 ? out_len : 32;
    memcpy(out, tmp, copy);
}
