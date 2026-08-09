#include "VAGKeys.h"

//valid vag keys used for decrypt, positions matter in decoder! dont change
//for different serial number different aut64 keys are used, atleast that seems how it works
static const uint8_t VAG_KEY_BLOB[64] PROGMEM = {
    //key 1
    0x01, 0x37, 0x6C, 0x86, 0xAD, 0xAB, 0xCC, 0x43,
    0x07, 0x4D, 0xE8, 0x59, 0xC1, 0x2F, 0x36, 0xAB,
    //key 2
    0x02, 0x37, 0x7C, 0x65, 0xCE, 0xDC, 0x42, 0xEA,
    0xA4, 0x53, 0xE8, 0x61, 0xD9, 0xB7, 0x20, 0xFC,
    //key 3
    0x03, 0x8A, 0xA3, 0x7B, 0x1E, 0x56, 0x1F, 0x83,
    0x84, 0xB6, 0x19, 0xC5, 0x2E, 0x0A, 0x3F, 0xD7,
    //key 4 - none, but was in rust code, serves as placeholder
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    
};
//tea schedule and logic - crypto for variant type 2 only
const uint32_t VAG_TEA_KEY_SCHEDULE[4] = {
    0x0B46502D, 0x5E253718, 0x2BF93A19, 0x622C1206
};
//aut64 crypto stuff
static const uint8_t TABLE_LN[AUT64_NUM_ROUNDS][8] PROGMEM = {
    {0x4,0x5,0x6,0x7,0x0,0x1,0x2,0x3},
    {0x5,0x4,0x7,0x6,0x1,0x0,0x3,0x2},
    {0x6,0x7,0x4,0x5,0x2,0x3,0x0,0x1},
    {0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0},
    {0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7},
    {0x1,0x0,0x3,0x2,0x5,0x4,0x7,0x6},
    {0x2,0x3,0x0,0x1,0x6,0x7,0x4,0x5},
    {0x3,0x2,0x1,0x0,0x7,0x6,0x5,0x4},
    {0x5,0x4,0x7,0x6,0x1,0x0,0x3,0x2},
    {0x4,0x5,0x6,0x7,0x0,0x1,0x2,0x3},
    {0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0},
    {0x6,0x7,0x4,0x5,0x2,0x3,0x0,0x1},
};

static const uint8_t TABLE_UN[AUT64_NUM_ROUNDS][8] PROGMEM = {
    {0x1,0x0,0x3,0x2,0x5,0x4,0x7,0x6},
    {0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7},
    {0x3,0x2,0x1,0x0,0x7,0x6,0x5,0x4},
    {0x2,0x3,0x0,0x1,0x6,0x7,0x4,0x5},
    {0x5,0x4,0x7,0x6,0x1,0x0,0x3,0x2},
    {0x4,0x5,0x6,0x7,0x0,0x1,0x2,0x3},
    {0x7,0x6,0x5,0x4,0x3,0x2,0x1,0x0},
    {0x6,0x7,0x4,0x5,0x2,0x3,0x0,0x1},
    {0x3,0x2,0x1,0x0,0x7,0x6,0x5,0x4},
    {0x2,0x3,0x0,0x1,0x6,0x7,0x4,0x5},
    {0x1,0x0,0x3,0x2,0x5,0x4,0x7,0x6},
    {0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7},
};

static const uint8_t TABLE_OFFSET[256] PROGMEM = {
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xA,0xB,0xC,0xD,0xE,0xF,
    0x0,0x2,0x4,0x6,0x8,0xA,0xC,0xE,0x3,0x1,0x7,0x5,0xB,0x9,0xF,0xD,
    0x0,0x3,0x6,0x5,0xC,0xF,0xA,0x9,0xB,0x8,0xD,0xE,0x7,0x4,0x1,0x2,
    0x0,0x4,0x8,0xC,0x3,0x7,0xB,0xF,0x6,0x2,0xE,0xA,0x5,0x1,0xD,0x9,
    0x0,0x5,0xA,0xF,0x7,0x2,0xD,0x8,0xE,0xB,0x4,0x1,0x9,0xC,0x3,0x6,
    0x0,0x6,0xC,0xA,0xB,0xD,0x7,0x1,0x5,0x3,0x9,0xF,0xE,0x8,0x2,0x4,
    0x0,0x7,0xE,0x9,0xF,0x8,0x1,0x6,0xD,0xA,0x3,0x4,0x2,0x5,0xC,0xB,
    0x0,0x8,0x3,0xB,0x6,0xE,0x5,0xD,0xC,0x4,0xF,0x7,0xA,0x2,0x9,0x1,
    0x0,0x9,0x1,0x8,0x2,0xB,0x3,0xA,0x4,0xD,0x5,0xC,0x6,0xF,0x7,0xE,
    0x0,0xA,0x7,0xD,0xE,0x4,0x9,0x3,0xF,0x5,0x8,0x2,0x1,0xB,0x6,0xC,
    0x0,0xB,0x5,0xE,0xA,0x1,0xF,0x4,0x7,0xC,0x2,0x9,0xD,0x6,0x8,0x3,
    0x0,0xC,0xB,0x7,0x5,0x9,0xE,0x2,0xA,0x6,0x1,0xD,0xF,0x3,0x4,0x8,
    0x0,0xD,0x9,0x4,0x1,0xC,0x8,0x5,0x2,0xF,0xB,0x6,0x3,0xE,0xA,0x7,
    0x0,0xE,0xF,0x1,0xD,0x3,0x2,0xC,0x9,0x7,0x6,0x8,0x4,0xA,0xB,0x5,
    0x0,0xF,0xD,0x2,0x9,0x6,0x4,0xB,0x1,0xE,0xC,0x3,0x8,0x7,0x5,0xA,
};

static const uint8_t TABLE_SUB[16] PROGMEM = {
    0x0,0x1,0x9,0xE,0xD,0xB,0x7,0x6,
    0xF,0x2,0xC,0x5,0xA,0x4,0x3,0x8,
};

static inline uint8_t pgm_byte(const uint8_t* p) {
    return pgm_read_byte(p);
}

static uint8_t key_nibble(const Aut64Key* key, uint8_t nibble,
                          const uint8_t* table,
                          uint8_t iter) {
    uint8_t kv = key->key[pgm_read_byte(&table[iter])];
    uint16_t offset = ((uint16_t)kv << 4) | nibble;
    return pgm_read_byte(&TABLE_OFFSET[offset]);
}
static uint8_t round_key(const Aut64Key* key, const uint8_t state[8],
                         uint8_t round_n) {
    uint8_t rh = 0, rl = 0;
    for (uint8_t i = 0; i < 7; i++) {
        rh ^= key_nibble(key, state[i] >> 4, TABLE_UN[round_n], i);
        rl ^= key_nibble(key, state[i] & 0x0F, TABLE_LN[round_n], i);
    }
    return (rh << 4) | rl;
}
static uint8_t final_byte_nibble(const Aut64Key* key,
                                 const uint8_t* table) {
    uint8_t idx = pgm_read_byte(&table[7]);
    return pgm_read_byte(&TABLE_SUB[key->key[idx]]) << 4;
}

static uint8_t encrypt_final_byte_nibble(const Aut64Key* key,
                                          uint8_t nibble,
                                          const uint8_t* table) {
    uint8_t off = final_byte_nibble(key, table);
    for (uint8_t i = 0; i < 16; i++) {
        if (pgm_read_byte(&TABLE_OFFSET[off + i]) == nibble)
            return i;
    }
    return 0;
}

static uint8_t decrypt_final_byte_nibble(const Aut64Key* key,
                                          uint8_t nibble,
                                          const uint8_t* table,
                                          uint8_t result) {
    uint8_t off = final_byte_nibble(key, table);
    return pgm_read_byte(&TABLE_OFFSET[(result ^ nibble) + off]);
}

static uint8_t decrypt_compress(const Aut64Key* key, const uint8_t state[8],
                                uint8_t round_n) {
    uint8_t rk = round_key(key, state, round_n);
    uint8_t rh = rk >> 4, rl = rk & 0x0F;
    uint8_t hi = decrypt_final_byte_nibble(key, state[7] >> 4,
                                            TABLE_UN[round_n], rh);
    uint8_t lo = decrypt_final_byte_nibble(key, state[7] & 0x0F,
                                            TABLE_LN[round_n], rl);
    return (hi << 4) | lo;
}

static uint8_t encrypt_compress(const Aut64Key* key, const uint8_t state[8],
                                uint8_t round_n) {
    uint8_t rk = round_key(key, state, round_n);
    uint8_t rh = rk >> 4, rl = rk & 0x0F;
    rh ^= encrypt_final_byte_nibble(key, state[7] >> 4, TABLE_UN[round_n]);
    rl ^= encrypt_final_byte_nibble(key, state[7] & 0x0F, TABLE_LN[round_n]);
    return (rh << 4) | rl;
}

//sbox substitution
static uint8_t substitute(const Aut64Key* key, uint8_t byte_val) {
    return (key->sbox[byte_val >> 4] << 4) | key->sbox[byte_val & 0x0F];
}

static uint8_t permute_bits(const Aut64Key* key, uint8_t byte_val) {
    uint8_t result = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (byte_val & (1 << i)) result |= (uint8_t)(1 << key->pbox[i]);
    }
    return result;
}

static void permute_bytes(const Aut64Key* key, uint8_t state[8]) {
    uint8_t result[8];
    for (uint8_t i = 0; i < 8; i++) {
        result[key->pbox[i]] = state[i];
    }
    memcpy(state, result, 8);
}

static void reverse_box(const uint8_t* box_in, uint8_t len, uint8_t* out) {
    for (uint8_t i = 0; i < len; i++) {
        for (uint8_t j = 0; j < len; j++) {
            if (box_in[j] == i) { out[i] = j; break; }
        }
    }
}

static bool box_is_permutation(const uint8_t* box_in, uint8_t len) {
    bool seen[16] = {false};
    for (uint8_t i = 0; i < len; i++) {
        uint8_t v = box_in[i];
        if (v >= len || seen[v]) return false;
        seen[v] = true;
    }
    for (uint8_t i = 0; i < len; i++) {
        if (!seen[i]) return false;
    }
    return true;
}


void aut64_decrypt(const Aut64Key* key, uint8_t message[8]) {
    for (int8_t i = AUT64_NUM_ROUNDS - 1; i >= 0; i--) {
        message[7] = substitute(key, message[7]);
        message[7] = permute_bits(key, message[7]);
        message[7] = substitute(key, message[7]);
        message[7] = decrypt_compress(key, message, (uint8_t)i);
        permute_bytes(key, message);
    }
}

void aut64_encrypt(const Aut64Key* key, uint8_t message[8]) {
    Aut64Key rev_key;
    memcpy(&rev_key, key, sizeof(Aut64Key));
    reverse_box(key->pbox, AUT64_PBOX_SIZE, rev_key.pbox);
    reverse_box(key->sbox, AUT64_SBOX_SIZE, rev_key.sbox);

    for (uint8_t i = 0; i < AUT64_NUM_ROUNDS; i++) {
        permute_bytes(&rev_key, message);
        message[7] = encrypt_compress(&rev_key, message, i);
        message[7] = substitute(&rev_key, message[7]);
        message[7] = permute_bits(&rev_key, message[7]);
        message[7] = substitute(&rev_key, message[7]);
    }
}

void aut64_unpack(const uint8_t packed[16], Aut64Key* out) {
    memset(out, 0, sizeof(Aut64Key));
    out->index = packed[0];

    for (uint8_t i = 0; i < 4; i++) {
        out->key[i * 2]     = packed[i + 1] >> 4;
        out->key[i * 2 + 1] = packed[i + 1] & 0x0F;
    }

    uint32_t pbox = ((uint32_t)packed[5] << 16)
                  | ((uint32_t)packed[6] << 8)
                  |  (uint32_t)packed[7];
    for (int8_t i = 7; i >= 0; i--) {
        out->pbox[i] = (uint8_t)(pbox & 0x7);
        pbox >>= 3;
    }

    for (uint8_t i = 0; i < 8; i++) {
        out->sbox[i * 2]     = packed[i + 8] >> 4;
        out->sbox[i * 2 + 1] = packed[i + 8] & 0x0F;
    }
}


bool aut64_validate_key(const Aut64Key* key) {
    for (uint8_t i = 0; i < AUT64_KEY_SIZE; i++) {
        if (key->key[i] >= AUT64_SBOX_SIZE) return false;
    }
    if (!box_is_permutation(key->pbox, AUT64_PBOX_SIZE)) return false;
    if (!box_is_permutation(key->sbox, AUT64_SBOX_SIZE)) return false;
    return true;
}

void tea_decrypt(uint32_t* v0, uint32_t* v1, const uint32_t key[4]) {
    uint32_t sum = TEA_DELTA * TEA_ROUNDS;
    for (uint8_t i = 0; i < TEA_ROUNDS; i++) {
        *v1 -= (((*v0 << 4) ^ (*v0 >> 5)) + *v0)
               ^ (sum + key[(sum >> 11) & 3]);
        sum -= TEA_DELTA;
        *v0 -= (((*v1 << 4) ^ (*v1 >> 5)) + *v1)
               ^ (sum + key[sum & 3]);
    }
}

void tea_encrypt(uint32_t* v0, uint32_t* v1, const uint32_t key[4]) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < TEA_ROUNDS; i++) {
        *v0 += (((*v1 << 4) ^ (*v1 >> 5)) + *v1)
               ^ (sum + key[sum & 3]);
        sum += TEA_DELTA;
        *v1 += (((*v0 << 4) ^ (*v0 >> 5)) + *v0)
               ^ (sum + key[(sum >> 11) & 3]);
    }
}

//key storage
static Aut64Key vag_keys[AUT64_MAX_KEYS];
static uint8_t  vag_key_count = 0;
static bool     vag_keys_loaded = false;

//load keys to progmem
void vag_keys_init(void) {
    if (vag_keys_loaded) return;

    vag_key_count = 0;
    memset(vag_keys, 0, sizeof(vag_keys));

    for (uint8_t i = 0; i < AUT64_MAX_KEYS; i++) {
        uint8_t packed[16];
        for (uint8_t j = 0; j < 16; j++) {
            packed[j] = pgm_read_byte(&VAG_KEY_BLOB[i * 16 + j]);
        }

        if (packed[0] == 0) continue;

        aut64_unpack(packed, &vag_keys[vag_key_count]);
        vag_key_count++;
    }

    vag_keys_loaded = true;
}

uint8_t vag_keys_count(void) {
    return vag_key_count;
}

const Aut64Key* vag_get_key_by_index(uint8_t index) {
    for (uint8_t i = 0; i < vag_key_count; i++) {
        if (vag_keys[i].index == index) return &vag_keys[i];
    }
    return NULL;
}

const Aut64Key* vag_get_key_by_position(uint8_t pos) {
    if (pos >= vag_key_count) return NULL;
    return &vag_keys[pos];
}

const Aut64Key* vag_get_all_keys(void) {
    return vag_keys;
}
