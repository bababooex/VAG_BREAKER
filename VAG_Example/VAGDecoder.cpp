#include <stdint.h>
#include "VAGDecoder.h"
#include "VAGKeys.h"
#include <string.h>


//simplified working manchester state machine
static int8_t manchester_decode(uint8_t pulse_type, VagManchesterState* state) {
    switch (pulse_type) {
    case 2: // LONG_HIGH
        *state = MAN_IDLE;
        return 1;
        
    case 3: // LONG_LOW
        *state = MAN_IDLE;
        return 0;
        
    case 0: // SHORT_HIGH
        if (*state == MAN_GOT_SHORT_LOW) {
            *state = MAN_IDLE;
            return 1;
        } else {
            *state = MAN_GOT_SHORT_HIGH;
            return -1;
        }
        
    case 1: // SHORT_LOW
        if (*state == MAN_GOT_SHORT_HIGH) {
            *state = MAN_IDLE;
            return 0;
        } else {
            *state = MAN_GOT_SHORT_LOW;
            return -1;
        }
        
    default:
        *state = MAN_IDLE;
        return -1;
    }
}
//shift register for storing data
static void push_bit(VagDecoder* v, bool bit) {
    uint8_t carry = (v->data_low >> 31) & 1;
    v->data_low = (v->data_low << 1) | (bit ? 1 : 0);
    v->data_high = (v->data_high << 1) | carry;
    v->bit_count++;
}

//check if in tolerance
static inline bool duration_near(uint32_t dur, uint32_t target,
                                  uint32_t delta) {
    uint32_t diff = (dur > target) ? (dur - target) : (target - dur);
    return diff <= delta;
}
//classify lenghts
static int8_t classify_data1(uint32_t dur, bool level) {
    if (duration_near(dur, TE_SHORT_12, TOL_12)) {
        return level ? 0 : 1;
    }
    if (duration_near(dur, TE_LONG_12, TOL_12)) {
        return level ? 2 : 3;
    }
    return -1;
}

static int8_t classify_data2(uint32_t dur, bool level) {
    if (duration_near(dur, TE_SHORT_34, TOL_34))  {
        return level ? 0 : 1;
    }
    if (duration_near(dur, TE_LONG_34, TOL_34)){
        return level ? 2 : 3;
    }
    return -1;
}

//try aut64 decrypt cipher
static bool try_aut64_decrypt(uint8_t block[8], uint8_t key_pos) {
    const Aut64Key* key = vag_get_key_by_position(key_pos);
    if (!key) return false;
    aut64_decrypt(key, block);
    return true;
}

static bool button_valid(const uint8_t dec[8]) {
    uint8_t btn = (dec[7] >> 4) & 0xF;
    //accept even corrupted (unknown)button
    return (btn == 1 || btn == 2 || btn == 4 || dec[7] == 0);
}
//check if button matches
static bool button_matches(const uint8_t dec[8], uint8_t dispatch) {
    uint8_t expected = (dispatch >> 4) & 0xF;
    uint8_t dec_btn  = (dec[7] >> 4) & 0xF;
    if (dec_btn == expected) return true;
    //handle the corrupted button as lock
    if (dec[7] == 0 && expected == 2) return true;
    return false;
}

//fill decoded signal 
static void fill_from_decrypted(VagDecodedSignal* sig,
                                 const uint8_t dec[8],
                                 uint8_t dispatch) {
    //serial vag byteswap
    uint32_t raw = dec[0] | ((uint32_t)dec[1] << 8)
                 | ((uint32_t)dec[2] << 16) | ((uint32_t)dec[3] << 24);
    sig->serial = (raw << 24) | ((raw & 0xFF00) << 8)
                | ((raw >> 8) & 0xFF00) | (raw >> 24);

    sig->counter  = dec[4] | ((uint32_t)dec[5] << 8)
                  | ((uint32_t)dec[6] << 16);
    sig->button   = (dec[7] >> 4) & 0xF;
    sig->btn_byte = dec[7];//additional data for btn byte
    sig->dispatch = dispatch;
    sig->decrypted = true;
}
//vag data parser
static void parse_data(VagDecoder* v) {
    VagDecodedSignal* sig = &v->last_signal;
    memset(sig, 0, sizeof(VagDecodedSignal));
    sig->vag_type = v->vag_type;

    uint8_t dispatch  = (uint8_t)(v->key2 & 0xFF);
    uint8_t key2_high = (uint8_t)((v->key2 >> 8) & 0xFF);

    uint8_t key1_bytes[8];
    key1_bytes[0] = (uint8_t)(v->key1_high >> 24);
    key1_bytes[1] = (uint8_t)(v->key1_high >> 16);
    key1_bytes[2] = (uint8_t)(v->key1_high >> 8);
    key1_bytes[3] = (uint8_t)(v->key1_high);
    key1_bytes[4] = (uint8_t)(v->key1_low >> 24);
    key1_bytes[5] = (uint8_t)(v->key1_low >> 16);
    key1_bytes[6] = (uint8_t)(v->key1_low >> 8);
    key1_bytes[7] = (uint8_t)(v->key1_low);

    sig->type_byte = key1_bytes[0];
    //correct manchester machine error with 8 not C to type byte is ok, bandage fix but whatever
    if ((sig->type_byte & 0xF0) == 0x80) {
        sig->type_byte |= 0x40;
        v->key1_high = (v->key1_high & 0x00FFFFFF) | ((uint32_t)sig->type_byte << 24);
    }

    uint8_t block[8];
    memcpy(block, key1_bytes + 1, 7);
    block[7] = key2_high;

    sig->key1_high = v->key1_high;
    sig->key1_low  = v->key1_low;
    sig->key2      = v->key2;

    switch (v->vag_type) {
    case VAG_TYPE_1: {
        if (dispatch != 0x2A && dispatch != 0x1C && dispatch != 0x46) return;
        for (uint8_t ki = 0; ki < vag_keys_count(); ki++) {
            uint8_t test[8];
            memcpy(test, block, 8);
            if (!try_aut64_decrypt(test, ki)) continue;
            if (!button_valid(test)) continue;
            sig->serial   = ((uint32_t)test[0] << 24)
                          | ((uint32_t)test[1] << 16)
                          | ((uint32_t)test[2] << 8)
                          |  (uint32_t)test[3];
            sig->counter  = test[4] | ((uint32_t)test[5] << 8)
                          | ((uint32_t)test[6] << 16);
            sig->button   = test[7];
            sig->btn_byte = test[7];
            sig->dispatch = dispatch;
            sig->key_idx  = ki;
            sig->decrypted = true;
            return;
        }
        break;
    }
    case VAG_TYPE_2: {
        if (dispatch != 0x2A && dispatch != 0x1C && dispatch != 0x46) return;
        uint32_t v0 = ((uint32_t)block[0] << 24)
                    | ((uint32_t)block[1] << 16)
                    | ((uint32_t)block[2] << 8)
                    |  (uint32_t)block[3];
        uint32_t v1 = ((uint32_t)block[4] << 24)
                    | ((uint32_t)block[5] << 16)
                    | ((uint32_t)block[6] << 8)
                    |  (uint32_t)block[7];
        tea_decrypt(&v0, &v1, VAG_TEA_KEY_SCHEDULE);
        uint8_t tea_dec[8];
        tea_dec[0] = (uint8_t)(v0 >> 24);
        tea_dec[1] = (uint8_t)(v0 >> 16);
        tea_dec[2] = (uint8_t)(v0 >> 8);
        tea_dec[3] = (uint8_t)(v0);
        tea_dec[4] = (uint8_t)(v1 >> 24);
        tea_dec[5] = (uint8_t)(v1 >> 16);
        tea_dec[6] = (uint8_t)(v1 >> 8);
        tea_dec[7] = (uint8_t)(v1);
        if (!button_matches(tea_dec, dispatch)) return;
        fill_from_decrypted(sig, tea_dec, dispatch);
        sig->key_idx = 0xFF;
        break;
    }
    case VAG_TYPE_3: {
        uint8_t trial_order[] = {2, 1, 0};
        for (uint8_t t = 0; t < 3; t++) {
            uint8_t ki = trial_order[t];
            uint8_t test[8];
            memcpy(test, block, 8);
            if (!try_aut64_decrypt(test, ki)) continue;
            if (!button_valid(test)) continue;
            if (ki == 2) {
                sig->vag_type = VAG_TYPE_4;
                v->vag_type = VAG_TYPE_4;
            }
            fill_from_decrypted(sig, test, dispatch);
            sig->key_idx = ki;
            return;
        }
        break;
    }
    case VAG_TYPE_4: {
        if (dispatch != 0x2B && dispatch != 0x1D && dispatch != 0x47) return;
        uint8_t test[8];
        memcpy(test, block, 8);
        if (!try_aut64_decrypt(test, 2)) return;
        if (!button_matches(test, dispatch)) return;
        fill_from_decrypted(sig, test, dispatch);
        sig->key_idx = 2;
        break;
    }
    default:
        break;
    }
}

//init manchester machine and set correct start step
void vag_init(VagDecoder* v) {
    memset(v, 0, sizeof(VagDecoder));
    v->step      = VAG_STEP_RESET;
    v->man_state = MAN_IDLE;
}
//feed captured data to parser
bool vag_feed(VagDecoder* v, bool level, uint32_t dur) {
    switch (v->step) {

    case VAG_STEP_RESET:
        if (!level) return false;
        
        if (duration_near(dur, TE_SHORT_12, TOL_PREAMBLE1)) {
            v->step = VAG_STEP_PREAMBLE1;
            v->data_low = 0; v->data_high = 0;
            v->header_count = 0; v->mid_count = 0;
            v->bit_count = 0;
            v->vag_type = VAG_TYPE_UNKNOWN;
            v->te_last = dur;
            v->man_state = MAN_IDLE;
            return false;
        }
        
        if (duration_near(dur, TE_SHORT_34, TOL_34)) {
            v->step = VAG_STEP_PREAMBLE2;
            v->data_low = 0; v->data_high = 0;
            v->header_count = 0; v->mid_count = 0;
            v->bit_count = 0;
            v->vag_type = VAG_TYPE_UNKNOWN;
            v->te_last = dur;
            v->man_state = MAN_IDLE;
            return false;
        }
        return false;

    case VAG_STEP_PREAMBLE1:
        if (level) return false;
        
        if (duration_near(dur, TE_SHORT_12, TOL_PREAMBLE1)) {
            if (duration_near(v->te_last, TE_SHORT_12, TOL_PREAMBLE1)) {
                v->te_last = dur;
                v->header_count++;
                return false;
            }
            v->step = VAG_STEP_RESET;
            return false;
        }
        
        if (v->header_count >= MIN_PREAMBLE1) {
            if (duration_near(dur, TE_LONG_12, TOL_12)) {
                if (duration_near(v->te_last, TE_SHORT_12, TOL_PREAMBLE1)) {
                    v->man_state = MAN_IDLE;
                    v->step = VAG_STEP_DATA1;
                    return false;
                }
            }
        }
        
        v->step = VAG_STEP_RESET;
        return false;

    case VAG_STEP_DATA1: {
        int8_t pulse_type = classify_data1(dur, level);
        
        if (pulse_type >= 0) {
            int8_t bit = manchester_decode((uint8_t)pulse_type, &v->man_state);
            if (bit >= 0) {
                push_bit(v, (bool)bit);
                
                if (v->bit_count == 16) {
                    uint16_t prefix = v->data_low & 0xFFFF;
                    if (prefix == 0xAF3F && v->data_high == 0) {
                        v->data_low = 0; v->data_high = 0;
                        v->bit_count = 0;
                        v->vag_type = VAG_TYPE_1;
                    } else if (prefix == 0xAF1C && v->data_high == 0) {
                        v->data_low = 0; v->data_high = 0;
                        v->bit_count = 0;
                        v->vag_type = VAG_TYPE_2;
                    }

                }else if (v->bit_count == 64) {
                    v->key1_low  = ~v->data_low;
                    v->key1_high = ~v->data_high;
                    v->data_low = 0; v->data_high = 0;
                }
            }
            return false;
        }

        if (!level) {
            uint32_t gap_diff = (dur > 6000) ? (dur - 6000) : (6000 - dur);
            if (gap_diff < 4000 && v->bit_count == 80) {
                v->key2 = (~v->data_low) & 0xFFFF;
                parse_data(v);
                v->has_signal = true;
                v->data_low = 0; v->data_high = 0;
                v->bit_count = 0;
                v->step = VAG_STEP_RESET;
                return true;
            }
        }
        v->data_low = 0; v->data_high = 0;
        v->bit_count = 0;
        v->step = VAG_STEP_RESET;
        return false;
    }

    case VAG_STEP_PREAMBLE2:
        if (!level) {
            if (duration_near(dur, TE_SHORT_34, TOL_34)) {
                if (duration_near(v->te_last, TE_SHORT_34, TOL_34)) {
                    v->te_last = dur;
                    v->header_count++;
                    return false;
                }
            }
            v->step = VAG_STEP_RESET;
            return false;
        }
        
        if (v->header_count < MIN_PREAMBLE2) return false;
        if (!duration_near(dur, TE_LONG_34, TOL_34)) return false;
        if (!duration_near(v->te_last, TE_SHORT_34, TOL_34)) return false;
        
        v->te_last = dur;
        v->step = VAG_STEP_SYNC2A;
        return false;

    case VAG_STEP_SYNC2A:
        if (!level) {
            if (duration_near(dur, TE_SHORT_34, TOL_34)) {
                if (duration_near(v->te_last, TE_LONG_34, TOL_34)) {
                    v->te_last = dur;
                    v->step = VAG_STEP_SYNC2B;
                    return false;
                }
            }
        }
        v->step = VAG_STEP_RESET;
        return false;

    case VAG_STEP_SYNC2B:
        if (level) {
            if (duration_near(dur, 750, TOL_34)) {
                v->te_last = dur;
                v->step = VAG_STEP_SYNC2C;
                return false;
            }
        }
        v->step = VAG_STEP_RESET;
        return false;

    case VAG_STEP_SYNC2C:
        if (!level) {
            if (duration_near(dur, 750, TOL_34)) {
                if (duration_near(v->te_last, 750, TOL_34)) {
                    v->mid_count++;
                    v->step = VAG_STEP_SYNC2B;
                    if (v->mid_count == 3) {
                        v->data_low = 1; 
                        v->data_high = 0;
                        v->bit_count = 1;
                        v->man_state = MAN_IDLE;
                        v->step = VAG_STEP_DATA2;
                    }
                    return false;
                }
            }
        }
        v->step = VAG_STEP_RESET;
        return false;

    case VAG_STEP_DATA2: {
        int8_t pulse_type = classify_data2(dur, level);
        
        if (pulse_type >= 0) {
            int8_t bit = manchester_decode((uint8_t)pulse_type, &v->man_state);
            if (bit >= 0) {
                push_bit(v, (bool)bit);
                
                if (v->bit_count == 64) {
                    v->key1_low  = v->data_low;
                    v->key1_high = v->data_high;
                    v->data_low = 0; v->data_high = 0;
                }
            }
        }
        
        if (v->bit_count == 80) {
            v->key2 = v->data_low & 0xFFFF;
            v->vag_type = VAG_TYPE_3;
            parse_data(v);
            v->has_signal = true;
            v->data_low = 0; v->data_high = 0;
            v->bit_count = 0;
            v->step = VAG_STEP_RESET;
            return true;
        }
        return false;
    }

    default:
        v->step = VAG_STEP_RESET;
        return false;
    }
}
//singal getter
const VagDecodedSignal* vag_get_signal(const VagDecoder* v) {
    return v->has_signal ? &v->last_signal : NULL;
}
//decoded number to button name
const char* vag_button_name(uint8_t button) {
    switch (button) {
    case 1:  case 0x10: return "Unlock";
    case 2:  case 0x20: return "Lock";
    case 4:  case 0x40: return "Boot";
    default: return "Unknown";
    }
}

//vehicle name taken from flipper arf, because it is just better for identification
const char* vag_vehicle_name(uint8_t type_byte) {
    switch (type_byte) {
    case 0x00: return "VAG NEW";
    case 0xC0: return "VAG OLD";
    case 0xC1: return "Audi";
    case 0xC2: return "Seat";
    case 0xC3: return "Skoda";
    default:   return "VAG GEN";
    }
}
//dispatch getter
static uint8_t get_dispatch_byte(uint8_t btn_byte, uint8_t vag_type) {
    if (vag_type == 1 || vag_type == 2) {
        switch (btn_byte) {
        case 0x20: case 2:  return 0x2A;
        case 0x40: case 4:  return 0x46;
        case 0x10: case 1:  return 0x1C;
        default:            return 0x2A;
        }
    } else {
        switch (btn_byte) {
        case 0x20: case 2:  return 0x2B;
        case 0x40: case 4:  return 0x47;
        case 0x10: case 1:  return 0x1D;
        default:            return 0x2B;
        }
    }
}
//button to byte translation
static uint8_t btn_to_byte(uint8_t btn, uint8_t vag_type) {
    if (vag_type == 1) return btn;
    switch (btn) {
    case 1:  return 0x10;
    case 2:  return 0x20;
    case 4:  return 0x40;
    default: return btn;
    }
}
//avoiding buffer and encode pulses directly
typedef void (*PulseCallback)(bool level, uint16_t dur);

static bool emit_pulse(PulseCallback tx, bool level, uint16_t dur) {
    if (tx) {
        tx(level, dur);
        return true;
    }
    return false;
}
//handle manchester encode
static bool encode_manchester_bits_stream(PulseCallback tx,
                                           uint64_t data, uint8_t nbits,
                                           uint16_t te) {
    for (int8_t i = nbits - 1; i >= 0; i--) {
        bool bit = (data >> i) & 1;
        if (bit) {
            emit_pulse(tx, true, te);
            emit_pulse(tx, false, te);
        } else {
            emit_pulse(tx, false, te);
            emit_pulse(tx, true, te);
        }
    }
    return true;
}
//full encryption + encode function via toggling
uint16_t vag_encode(uint8_t vag_type,
                    uint32_t serial, uint32_t counter,
                    uint8_t button, uint8_t type_byte, uint8_t key_idx,
                    PulseCallback tx) {
    //VW Passat - VAG NEW switcher to type 2 like in original code                
    if (vag_type == VAG_TYPE_1 && type_byte == 0x00) {
        vag_type = VAG_TYPE_2;
        #ifdef CORR_TX
        corr=true;    
        #endif    
    }
    else{
        #ifdef CORR_TX
        corr=false;
        #endif  
    }
    uint8_t  btn_byte = btn_to_byte(button, vag_type);
    uint8_t  dispatch = get_dispatch_byte(btn_byte, vag_type);

    uint8_t block[8];
    block[0] = (uint8_t)(serial >> 24);
    block[1] = (uint8_t)(serial >> 16);
    block[2] = (uint8_t)(serial >> 8);
    block[3] = (uint8_t)(serial);
    block[4] = (uint8_t)(counter);
    block[5] = (uint8_t)(counter >> 8);
    block[6] = (uint8_t)(counter >> 16);
    block[7] = btn_byte;

    uint8_t enc_block[8];

    if (vag_type == VAG_TYPE_2) {
        memcpy(enc_block, block, 8);
        uint32_t v0 = ((uint32_t)enc_block[0] << 24)
                    | ((uint32_t)enc_block[1] << 16)
                    | ((uint32_t)enc_block[2] << 8)
                    |  (uint32_t)enc_block[3];
        uint32_t v1 = ((uint32_t)enc_block[4] << 24)
                    | ((uint32_t)enc_block[5] << 16)
                    | ((uint32_t)enc_block[6] << 8)
                    |  (uint32_t)enc_block[7];
        tea_encrypt(&v0, &v1, VAG_TEA_KEY_SCHEDULE);
        enc_block[0] = (uint8_t)(v0 >> 24);
        enc_block[1] = (uint8_t)(v0 >> 16);
        enc_block[2] = (uint8_t)(v0 >> 8);
        enc_block[3] = (uint8_t)(v0);
        enc_block[4] = (uint8_t)(v1 >> 24);
        enc_block[5] = (uint8_t)(v1 >> 16);
        enc_block[6] = (uint8_t)(v1 >> 8);
        enc_block[7] = (uint8_t)(v1);
    } else {
        uint8_t kpos;
        if (key_idx != 0xFF) {
            kpos = key_idx;
        } else if (vag_type == VAG_TYPE_4) {
            kpos = 2;//vag type 4 uses key 2 as const
        } else {
            kpos = 1;
        }
        const Aut64Key* key = vag_get_key_by_position(kpos);
        if (!key) return 0;
        memcpy(enc_block, block, 8);
        aut64_encrypt(key, enc_block);
    }

    uint32_t key1_high = ((uint32_t)type_byte << 24)
                       | ((uint32_t)enc_block[0] << 16)
                       | ((uint32_t)enc_block[1] << 8)
                       |  (uint32_t)enc_block[2];
    uint32_t key1_low  = ((uint32_t)enc_block[3] << 24)
                       | ((uint32_t)enc_block[4] << 16)
                       | ((uint32_t)enc_block[5] << 8)
                       |  (uint32_t)enc_block[6];
    uint16_t key2_val  = (uint16_t)(((uint16_t)enc_block[7] << 8) | dispatch);
    #ifdef DEBUG
        //this debugs sent data before ASK tx
        Serial.println(key1_high,HEX);
        Serial.println(key1_low,HEX);
        Serial.println(key2_val,HEX);
    #endif
    uint16_t pulse_count = 0;
    if (vag_type == VAG_TYPE_1 || vag_type == VAG_TYPE_2) {
        //preamble 
        for (uint16_t i = 0; i < 220; i++) {
            emit_pulse(tx, true, 300);
            emit_pulse(tx, false, 300);
            pulse_count += 2;
        }
        //"sync"
        emit_pulse(tx, false, 300);
        emit_pulse(tx, true, 300);
        pulse_count+=2;
        //prefix
        uint16_t prefix = (vag_type == VAG_TYPE_1) ? 0xAF3F : 0xAF1C;
        encode_manchester_bits_stream(tx, prefix, 16, 300);
        pulse_count += 32;
        //inverted data
        uint64_t key1 = ((uint64_t)key1_high << 32) | key1_low;
        uint64_t key1_inv = ~key1;
        encode_manchester_bits_stream(tx, key1_inv, 64, 300);
        pulse_count += 128;

        uint16_t key2_inv = ~key2_val;
        encode_manchester_bits_stream(tx, key2_inv, 16, 300);
        pulse_count += 32;
        //end
        emit_pulse(tx, false, 6000);
        pulse_count++;

    } else {
        uint64_t key1 = ((uint64_t)key1_high << 32) | key1_low;
        //original has repeat and octavia keyfob also repeats, so it stays
        for (uint8_t rep = 0; rep < 2; rep++) {
            //preamble
            for (uint8_t i = 0; i < 45; i++) {
                emit_pulse(tx, true, 500);
                emit_pulse(tx, false, 500);
                pulse_count += 2;
            }
            //sync
            emit_pulse(tx, true, 1000);
            emit_pulse(tx, false, 500);
            pulse_count += 2;
            
            for (uint8_t i = 0; i < 3; i++) {
                emit_pulse(tx, true, 750);
                emit_pulse(tx, false, 750);
                pulse_count += 2;
            }
            //data 
            encode_manchester_bits_stream(tx, key1, 64, 500);
            pulse_count += 128;
            
            encode_manchester_bits_stream(tx, key2_val, 16, 500);
            pulse_count += 32;
            //end
            if (rep == 0) {
                emit_pulse(tx, false, 10000);
                pulse_count++;
            }
        }
    }

    return pulse_count;
}