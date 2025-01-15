#include <Arduino.h>  
#include "display.h"

const uint8_t digitToSegments[] = {
    0x3F,  // 0
    0x06,  // 1
    0x5B,  // 2
    0x4F,  // 3
    0x66,  // 4
    0x6D,  // 5
    0x7D,  // 6
    0x07,  // 7
    0x7F,  // 8
    0x6F   // 9
};

void initializeDisplay(void) {
    pinMode(DS_PIN, OUTPUT);
    pinMode(SH_CP_PIN, OUTPUT);
    pinMode(ST_CP_PIN, OUTPUT);
}


void writeByte(uint8_t number, bool last) {
    for (int i = 7; i >= 0; i--) {
        if (number & (1 << i)) {
            digitalWrite(DS_PIN, HIGH);  
        } else {
            digitalWrite(DS_PIN, LOW);   
        }

        digitalWrite(SH_CP_PIN, HIGH);  
        digitalWrite(SH_CP_PIN, LOW);
    }

    if (last) {
        digitalWrite(ST_CP_PIN, HIGH);  
        digitalWrite(ST_CP_PIN, LOW);
    }
}


void writeHighAndLowNumber(uint8_t tens, uint8_t ones) {
    writeByte(tens, false);  
    writeByte(ones, true);   
}


void showResults(byte result) {
    uint8_t tens = result / 10;  
    uint8_t ones = result % 10;  
    writeHighAndLowNumber(digitToSegments[tens], digitToSegments[ones]);
}
