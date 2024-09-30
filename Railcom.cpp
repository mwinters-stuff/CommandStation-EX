/*
 *  SEE ADDITIONAL COPYRIGHT ATTRIBUTION BELOW
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

/** Sections of this code (the decode table constants) 
 * are taken from openmrn under the following copyright.
 * 
 * Copyright (c) 2014, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 **/

#include "Railcom.h"
#include "FSH.h"

  /** Table for 8-to-6 decoding of railcom data. This table can be indexed by the
   * 8-bit value read from the railcom channel, and the return value will be
   * either a 6-bit number, or one of the defined Railcom constrantrs. If the
   * value is invalid, the INV constant is returned. */
    
    // These values appear in the railcom_decode table to mean special symbols.
    static constexpr uint8_t
        // highest valid 6-bit value 
        MAX_VALID = 0x3F,
         /// invalid value (not conforming to the 4bit weighting requirement)
        INV = 0xff,
        /// Railcom ACK; the decoder received the message ok. NOTE: There are
        /// two codepoints that map to this.
        ACK = 0xfe,
        /// The decoder rejected the packet.
        NACK = 0xfd,
        /// The decoder is busy; send the packet again. This is typically
        /// returned when a POM CV write is still pending; the caller must
        /// re-try sending the packet later.
        RCBUSY = 0xfc,

        /// Reserved for future expansion.
        RESVD1 = 0xfb,
        /// Reserved for future expansion.
        RESVD2 = 0xfa;

const uint8_t HIGHFLASH decode[256] =
    // 0|8     1|9     2|a     3|b     4|c     5|d     6|e     7|f  
{      INV,    INV,    INV,    INV,    INV,    INV,    INV,    INV, // 0
       INV,    INV,    INV,    INV,    INV,    INV,    INV,    ACK, // 0
       INV,    INV,    INV,    INV,    INV,    INV,    INV,   0x33, // 1
       INV,    INV,    INV,   0x34,    INV,   0x35,   0x36,    INV, // 1
       INV,    INV,    INV,    INV,    INV,    INV,    INV,   0x3A, // 2
       INV,    INV,    INV,   0x3B,    INV,   0x3C,   0x37,    INV, // 2
       INV,    INV,    INV,   0x3F,    INV,   0x3D,   0x38,    INV, // 3
       INV,   0x3E,   0x39,    INV,   NACK,    INV,    INV,    INV, // 3
       INV,    INV,    INV,    INV,    INV,    INV,    INV,   0x24, // 4
       INV,    INV,    INV,   0x23,    INV,   0x22,   0x21,    INV, // 4
       INV,    INV,    INV,   0x1F,    INV,   0x1E,   0x20,    INV, // 5
       INV,   0x1D,   0x1C,    INV,   0x1B,    INV,    INV,    INV, // 5
       INV,    INV,    INV,   0x19,    INV,   0x18,   0x1A,    INV, // 6
       INV,   0x17,   0x16,    INV,   0x15,    INV,    INV,    INV, // 6
       INV,   0x25,   0x14,    INV,   0x13,    INV,    INV,    INV, // 7
      0x32,    INV,    INV,    INV,    INV,    INV,    INV,    INV, // 7
       INV,    INV,    INV,    INV,    INV,    INV,    INV, RESVD2, // 8
       INV,    INV,    INV,   0x0E,    INV,   0x0D,   0x0C,    INV, // 8
       INV,    INV,    INV,   0x0A,    INV,   0x09,   0x0B,    INV, // 9
       INV,   0x08,   0x07,    INV,   0x06,    INV,    INV,    INV, // 9
       INV,    INV,    INV,   0x04,    INV,   0x03,   0x05,    INV, // a
       INV,   0x02,   0x01,    INV,   0x00,    INV,    INV,    INV, // a
       INV,   0x0F,   0x10,    INV,   0x11,    INV,    INV,    INV, // b
      0x12,    INV,    INV,    INV,    INV,    INV,    INV,    INV, // b
       INV,    INV,    INV, RESVD1,    INV,   0x2B,   0x30,    INV, // c
       INV,   0x2A,   0x2F,    INV,   0x31,    INV,    INV,    INV, // c
       INV,   0x29,   0x2E,    INV,   0x2D,    INV,    INV,    INV, // d
      0x2C,    INV,    INV,    INV,    INV,    INV,    INV,    INV, // d
       INV, RCBUSY,   0x28,    INV,   0x27,    INV,    INV,    INV, // e
      0x26,    INV,    INV,    INV,    INV,    INV,    INV,    INV, // e
       ACK,    INV,    INV,    INV,    INV,    INV,    INV,    INV, // f
       INV,    INV,    INV,    INV,    INV,    INV,    INV,    INV, // f       
};
   /// Packet identifiers from Mobile Decoders.
    enum RailcomMobilePacketId
    {
        RMOB_POM = 0,
        RMOB_ADRHIGH = 1,
        RMOB_ADRLOW = 2,
        RMOB_EXT = 3,
        RMOB_DYN = 7,
        RMOB_XPOM0 = 8,
        RMOB_XPOM1 = 9,
        RMOB_XPOM2 = 10,
        RMOB_XPOM3 = 11,
        RMOB_SUBID = 12,
        RMOB_LOGON_ASSIGN_FEEDBACK = 13,
        RMOB_LOGON_ENABLE_FEEDBACK = 15,
};


Railcom::Railcom() {
    haveHigh=false;
    haveLow=false;
}

    /* returns -1: Call again next packet
                0: No loco on track
               >0: loco id
    */
int16_t Railcom::getChannel1Loco(uint8_t * inbound) {
    auto v1=GETHIGHFLASH(decode,inbound[0]);
    if (v1>MAX_VALID) return -1;
    auto v2=GETHIGHFLASH(decode,inbound[1]);
    if (v2>MAX_VALID) return -1;
    auto packet=(v1<<6) | v2;
    // packet is 12 bits TTTTDDDDDDDD
    auto type=packet>>8;   
    auto data= packet & 0xFF;
    if (type==RMOB_ADRHIGH) {
        holdoverHigh=data;
        haveHigh=true;
        }
    else if (type==RMOB_ADRLOW) {
        holdoverLow=data;
        haveLow=true;
        }
    else if (type==RMOB_EXT) {
        return -1; /* ignore*/
        }
    else {
        haveHigh=false;
        haveLow=false;
        return 0; // treat as no loco 
    }    
    if (haveHigh && haveLow) return ((holdoverHigh<<8)| holdoverLow);
    return -1; // call again, need next packet 
}
