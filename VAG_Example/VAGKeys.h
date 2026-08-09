#ifndef VAG_KEYS_H
#define VAG_KEYS_H
//handling crypto stuff library - tea/aut64
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "Arduino.h"
#ifdef __cplusplus
extern "C" {
#endif

//aut64 macros
#define AUT64_NUM_ROUNDS   12
#define AUT64_BLOCK_SIZE   8
#define AUT64_KEY_SIZE     8
#define AUT64_PBOX_SIZE    8
#define AUT64_SBOX_SIZE    16
#define AUT64_PACKED_SIZE  16
//max key set, can be changed, but as const here
#define AUT64_MAX_KEYS     4

//aut64 key struct
typedef struct {
    uint8_t index;                     
    uint8_t key[AUT64_KEY_SIZE];      
    uint8_t pbox[AUT64_PBOX_SIZE];       
    uint8_t sbox[AUT64_SBOX_SIZE];   
} Aut64Key;

//tea macros and const
#define TEA_DELTA   0x9E3779B9
#define TEA_ROUNDS  32

extern const uint32_t VAG_TEA_KEY_SCHEDULE[4];


//decrypt 8 byte block by specific key
void aut64_decrypt(const Aut64Key* key, uint8_t block[8]);
//encrypt 8 byte block by specific key
void aut64_encrypt(const Aut64Key* key, uint8_t block[8]);
//unpack 16 byte block into aut64 struct
void aut64_unpack(const uint8_t packed[16], Aut64Key* out);
//key validation
bool aut64_validate_key(const Aut64Key* key);

//tea decrypt with key schedule
void tea_decrypt(uint32_t* v0, uint32_t* v1, const uint32_t key[4]);

//tea encrypt with key schedule
void tea_encrypt(uint32_t* v0, uint32_t* v1, const uint32_t key[4]);

//vag key store
//init, this is important!
void vag_keys_init(void);
//returns number of loaded keys (3)
uint8_t vag_keys_count(void);
//get back key by its index
const Aut64Key* vag_get_key_by_index(uint8_t index);
//get back key by its position
const Aut64Key* vag_get_key_by_position(uint8_t pos);
//get all keyz
const Aut64Key* vag_get_all_keys(void);

#ifdef __cplusplus
}
#endif

#endif /* VAG_KEYS_H */
