/*
 *  © 2024 Chris Harlow
 *  All rights reserved.
 *  
 *  This file is part of DCC-EX
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

#ifndef Railcom_h
#define Railcom_h
#include "Arduino.h"

class Railcom {
  public:
    Railcom(uint16_t vpin);

    /* returns -1: Call again next packet
                0: No loco on track
               >0: loco id
    */
  void process(uint8_t * inbound,uint8_t length);
  
  private:
  void noData();
  uint16_t vpin;
 uint8_t holdoverHigh,holdoverLow;
 bool haveHigh,haveLow; 
 uint8_t packetsWithNoData; 
 uint16_t lastChannel1Loco; 
 static const byte MAX_WAIT_FOR_GLITCH=20; // number of dead or empty packets before assuming loco=0 
};

#endif