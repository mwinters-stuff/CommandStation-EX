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
    
  // Constructor
   TM1638(VPIN firstVpin, byte clk_pin,byte dio_pin,byte stb_pin);

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
    _DF_RAW=0x20 // bytes are raw 7-segment pattern (max length 4)
  };

  static void create(VPIN firstVpin, byte clk_pin,byte dio_pin,byte stb_pin);
  
  // Functions overridden in IODevice
  void _begin();
  void _loop(unsigned long currentMicros) override ;
  void _writeAnalogue(VPIN vpin, int value, uint8_t param1, uint16_t param2) override;
  void _display() override ;
  int _read(VPIN pin) override;
  void _write(VPIN pin,int value) override;
};
#endif
