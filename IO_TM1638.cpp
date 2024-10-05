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

#include <Arduino.h>
#include "IODevice.h"
#include "DIAG.h"
#include "IO_TM1638.h"
#include "TM1638x.h"
   
const uint8_t HIGHFLASH _digits[16]={
      0b00111111,0b00000110,0b01011011,0b01001111,
      0b01100110,0b01101101,0b01111101,0b00000111,
      0b01111111,0b01101111,0b01110111,0b01111100,
      0b00111001,0b01011110,0b01111001,0b01110001
    };

  // Constructor
   TM1638::TM1638(VPIN firstVpin, byte clk_pin,byte dio_pin,byte stb_pin){
    _firstVpin = firstVpin;
    _nPins = 8;
    tm=new TM1638x(clk_pin,dio_pin,stb_pin);
    _buttons=0;
    _leds=0;
    _lastLoop=micros();
    addDevice(this);
   } 

    
  void TM1638::create(VPIN firstVpin, byte clk_pin,byte dio_pin,byte stb_pin) {
    if (checkNoOverlap(firstVpin,8)) 
     new TM1638(firstVpin, clk_pin,dio_pin,stb_pin); 
  }

  void TM1638::_begin()  {
    tm->reset();
    tm->test();
    _display();
  }
  
 
  void TM1638::_loop(unsigned long currentMicros)  {
     if (currentMicros - _lastLoop > (1000000UL/LoopHz)) {
         _buttons=tm->getButtons();// Read the buttons
         _lastLoop=currentMicros;
     }
     // DIAG(F("TM1638 buttons %x"),_buttons); 
  }
           
  void TM1638::_display()  {
    DIAG(F("TM1638 Configured on Vpins:%u-%u"), _firstVpin, _firstVpin+_nPins-1);
  }

// digital read gets button state 
int TM1638::_read(VPIN vpin)  {
  byte pin=vpin - _firstVpin;
  bool result=bitRead(_buttons,pin);
  // DIAG(F("TM1638 read (%d) buttons %x = %d"),pin,_buttons,result);  
  return result;
}

// digital write sets led state 
void TM1638::_write(VPIN vpin, int value)  {
  // TODO.. skip if no state change  
  tm->writeLed(vpin - _firstVpin + 1,value!=0);
  }

// Analog write sets digit displays 

void TM1638::_writeAnalogue(VPIN vpin, int lowBytes, uint8_t mode, uint16_t highBytes)  {  
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
                tm->displayDig(startDigit,GETHIGHFLASH(_digits,(value%10))); 
                value=value/10;
                break; 
            case _DF_HEX:// HEX (leading zeros)
                tm->displayDig(startDigit,GETHIGHFLASH(_digits,(value & 0x0F))); 
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


