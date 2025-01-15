#ifndef DISPLAY_H
#define DISPLAY_H

#define DS_PIN   7 
#define SH_CP_PIN 9  
#define ST_CP_PIN 10  


void initializeDisplay(void);
void writeByte(uint8_t number, bool last);
void writeHighAndLowNumber(uint8_t tens, uint8_t ones);
void showResults(byte result);

#endif
