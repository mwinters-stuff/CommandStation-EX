   /*
 *  © 2024, Henk Kruisbrink & Chris Harlow. All rights reserved.
 *  © 2023, Neil McKechnie. All rights reserved.
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
#include "I2CManager.h"
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
  tm->writeLed(vpin - _firstVpin + 1,value!=0);
  }

};
#endif // IO_TM1638_h
