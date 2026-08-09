#ifndef VAG_DECODER_H
#define VAG_DECODER_H
//arduino vag decoder and encoder
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
//vag consts
#define TE_SHORT_12   300
#define TE_LONG_12    600
#define TE_SHORT_34   500
#define TE_LONG_34    1000
#define TOL_12        79   
#define TOL_34        120  
#define TOL_PREAMBLE1 100
#define MIN_PREAMBLE1 201
#define MIN_PREAMBLE2 41
//if defined enables extern bool that applies visual correction to lcd 
#define CORR_TX
//all vag types with static factory codes
typedef enum {
    VAG_TYPE_UNKNOWN = 0,
    VAG_TYPE_1 = 1,   // AUT64, 300µs
    VAG_TYPE_2 = 2,   // TEA,  300µs 
    VAG_TYPE_3 = 3,   // AUT64, 500µs, auto-detect key 
    VAG_TYPE_4 = 4    // AUT64, 500µs, preset key index 2 
} VagType;
//step of decoder
typedef enum {
    VAG_STEP_RESET = 0,
    VAG_STEP_PREAMBLE1,
    VAG_STEP_DATA1,
    VAG_STEP_PREAMBLE2,
    VAG_STEP_SYNC2A,
    VAG_STEP_SYNC2B,
    VAG_STEP_SYNC2C,
    VAG_STEP_DATA2
} VagDecoderStep;
//manchester machine
typedef enum {
    MAN_IDLE = 0,
    MAN_GOT_SHORT_HIGH = 1,
    MAN_GOT_SHORT_LOW = 2,
} VagManchesterState;
//manchester events
typedef enum {
    VAG_EVENT_SHORT_HIGH = 0,
    VAG_EVENT_SHORT_LOW,
    VAG_EVENT_LONG_HIGH,
    VAG_EVENT_LONG_LOW,
    VAG_EVENT_RESET
} VagManchesterEvent;
//decoder struct
typedef struct {
    uint8_t  vag_type;      // VAG_TYPE_1..4
    uint8_t  type_byte;     // Vehicle type identifier
    uint32_t serial;        // decrypted serial number
    uint32_t counter;       // decrypted counter (24-bit)
    uint8_t  button;        // button code (1=Unlock, 2=Lock, 4=Boot)
    uint8_t  btn_byte;      // raw button byte
    uint8_t  dispatch;      // dispatch byte
    uint8_t  key_idx;       // AUT64 key index used (0xFF for TEA)
    bool     decrypted;     // only true if successfully decrypted
    
    // raw key values
    uint32_t key1_high;
    uint32_t key1_low;
    uint16_t key2;
} VagDecodedSignal;
//decoder states
typedef struct {
    VagDecoderStep     step;
    VagManchesterState man_state;
    
    uint32_t data_low;
    uint32_t data_high;
    uint16_t bit_count;
    
    uint32_t key1_low;
    uint32_t key1_high;
    uint16_t key2;
    
    uint32_t te_last;
    uint16_t header_count;
    uint8_t  mid_count;
    uint8_t  vag_type;
    
    bool            has_signal;
    VagDecodedSignal last_signal;
} VagDecoder;


//initialize decoder and prepare it for pulses
void vag_init(VagDecoder* v);
//probably doesnt need to be uint32, but just to be safe
bool vag_feed(VagDecoder* v, bool level, uint32_t duration_us);
// Get the last decoded signal (valid after vag_feed returns true)
const VagDecodedSignal* vag_get_signal(const VagDecoder* v);

//button name from remote
const char* vag_button_name(uint8_t button);

//what vag car is it (audi, seat, skoda etc.)?
const char* vag_vehicle_name(uint8_t type_byte);

//encoder
/**
 * This encodes vag signal as valid -> because of rolling code nature -> button always must be previous count + 1
 * 
 * Params:
 *   vag_type:  VAG_TYPE_1..4
 *   serial:    32-bit serial
 *   counter:   24-bit counter
 *   button:    1=Unlock, 2=Lock, 4=Boot
 *   type_byte: Vehicle type byte (e.g. 0xC0 for VW)
 *   key_idx:   AUT64 key index (0-2), ignored for Type 2
 * 
 * Sends data pulses starting on high level to avoid buffer and heavy ram usage, it is possible to decode in real time fine
 * 
 */
typedef void (*PulseCallback)(bool level, uint16_t dur);
uint16_t vag_encode(uint8_t vag_type,
                    uint32_t serial, uint32_t counter,
                    uint8_t button, uint8_t type_byte, uint8_t key_idx,
                    PulseCallback tx);
//this variable visually correct from keyidx 0 to X when VW Passat - VAG NEW is txed
#ifdef CORR_TX
extern bool corr;   
#endif                       
#ifdef __cplusplus
}
#endif

#endif /* VAG_DECODER_H */
