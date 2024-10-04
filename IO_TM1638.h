   /*
 *  © 2024, Chris Harlow. All rights reserved.
 *  
 *  This file is part of DCC++EX API
 *
 *  This is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  It is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CommandStation.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef IO_TM1638_h
#define IO_TM1638_h
#include <Arduino.h>
#include "IODevice.h"
#include "DIAG.h"
#include "TM1638x.h"

class TM1638 : public IODevice {
private: 
    
    TM1638x * tm;    
    uint8_t _buttons;
    uint8_t _leds;
    unsigned long _lastLoop;
    static const int LoopHz=20; 

private:
  // Constructor
   TM1638(VPIN firstVpin, byte clk_pin,byte dio_pin,byte stb_pin){
    _firstVpin = firstVpin;
    _nPins = 8;
    tm=new TM1638x(clk_pin,dio_pin,stb_pin);
    _buttons=0;
    _leds=0;
    _lastLoop=micros();
    addDevice(this);
   } 
  
public:
  enum DigitFormat : byte {
    // last 4 bits are length.
    // DF_1.. DF_8 decimal 
    DF_1=0x01,DF_2=0x02,DF_3=0x03,DF_4=0x04,
    DF_5=0x05,DF_6=0x06,DF_7=0x07,DF_8=0x08,
    // DF_1X.. DF_8X HEX 
    DF_1X=0x11,DF_2X=0x12,DF_3X=0x13,DF_4X=0x14,
    DF_5X=0x15,DF_6X=0x16,DF_7X=0x17,DF_8X=0x18,
    // DF_1R .. DF_4R raw 7 segmnent data 
    // only 4 because HAL analogWrite only passes 4 bytes 
    DF_1R=0x21,DF_2R=0x22,DF_3R=0x23,DF_4R=0x24,
    
    //  bits of data conversion type  (ored with length) 
    _DF_DECIMAL=0x00,// right adjusted decimal unsigned leading zeros
    _DF_HEX=0x10,    // right adjusted hex leading zeros 
    _DF_RAW=0x20,    // bytes are raw 7-segment pattern (max length 4)
  };

  static void create(VPIN firstVpin, byte clk_pin,byte dio_pin,byte stb_pin) {
    if (checkNoOverlap(firstVpin,8)) 
     new TM1638(firstVpin, clk_pin,dio_pin,stb_pin); 
  }

  void _begin() override {
    tm->reset();
    tm->test();
    _display();
  }
  
 
  void _loop(unsigned long currentMicros) override {
     if (currentMicros - _lastLoop > (1000000UL/LoopHz)) {
         _buttons=tm->getButtons();// Read the buttons
         _lastLoop=currentMicros;
     }
     // DIAG(F("TM1638 buttons %x"),_buttons); 
  }
           
  void _display() override {
    DIAG(F("TM1638 Configured on Vpins:%u-%u"), _firstVpin, _firstVpin+_nPins-1);
  }

// digital read gets button state 
int _read(VPIN vpin) override {
  byte pin=vpin - _firstVpin;
  bool result=bitRead(_buttons,pin);
  // DIAG(F("TM1638 read (%d) buttons %x = %d"),pin,_buttons,result);  
  return result;
}

// digital write sets led state 
void _write(VPIN vpin, int value) override {
  // TODO.. skip if no state change  
  tm->writeLed(vpin - _firstVpin + 1,value!=0);
  }

// Analog write sets digit displays 

void _writeAnalogue(VPIN vpin, int lowBytes, uint8_t mode, uint16_t highBytes) override {  
   DIAG(F("TM1638 w(v=%d,l=%d,m=%d,h=%d,lx=%x,hx=%x"),
   vpin,lowBytes,mode,highBytes,lowBytes,highBytes);
   // mode is in DataFormat defined above.
   byte formatLength=mode & 0x0F;  // last 4 bits 
   byte formatType=mode & 0xF0;         //           
   int8_t leftDigit=vpin-_firstVpin; // 0..7 from left
   int8_t rightDigit=leftDigit+formatLength-1; // 0..7 from left
   
   // loading is done right to left startDigit first
   int8_t startDigit=7-rightDigit; // reverse as 7 on left
   int8_t lastDigit=7-leftDigit; // reverse as 7 on left
   uint32_t value=highBytes;
   value<<=16;
   value |= (uint16_t)lowBytes;
   
   DIAG(F("TM1638 fl=%d ft=%x sd=%d ld=%d v=%l vx=%X"),
   formatLength,formatType,
   startDigit,lastDigit,value,value);
    while(startDigit<=lastDigit) {
        switch (formatType) {
            case _DF_DECIMAL:// decimal (leading zeros)
                tm->displayVal(startDigit,value%10); 
                value=value/10;
                break; 
            case _DF_HEX:// HEX (leading zeros)
                tm->displayVal(startDigit,value & 0x0F); 
                value>>=4;
                break;  
            case _DF_RAW:// Raw 7-segment pattern 
                tm->displayDig(startDigit,value & 0xFF); 
                value>>=8;
                break;
            default:
                DIAG(F("TM1368 invalid mode 0x%x"),mode);
                return;
            }
    startDigit++;
    } 
     
}
};
#endif // IO_TM1638_h
