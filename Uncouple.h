/*
 *  © 2025 Mathew WInters
 *  © 2021 Neil McKechnie
 *  © 2021 M Steve Todd
 *  © 2021 Fred Decker
 *  © 2020-2021 Harald Barth
 *  © 2020-2022 Chris Harlow
 *  © 2013-2016 Gregg E. Berman
 *  All rights reserved.
 *  
 *  This file is part of CommandStation-EX
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

#ifndef UNCOUPLE_H
#define UNCOUPLE_H

//#define EESTOREDEBUG 
#include "Arduino.h"
#include "IODevice.h"
#include "StringFormatter.h"

// Uncouple type definitions
enum {
  UNCOUPLE_DCC = 1,
  UNCOUPLE_SERVO = 2,
  UNCOUPLE_VPIN = 3,
};

/*************************************************************************************
 * Uncouple - Base class for uncouples.
 * 
 *************************************************************************************/

class Uncouple {
protected:
  /* 
   * Object data
   */

  // The UncoupleData struct contains data common to all uncouple types, that 
  // is written to EEPROM when the uncouple is saved.
  // The first byte of this struct contains the 'uncoupled' flag which is
  // updated whenever the uncouple changes from uncoupled to coupled and
  // vice versa.  If the uncouple has been saved, then this byte is rewritten
  // when changed in RAM.  The 'uncoupled' flag must be located in the first byte.
  struct UncoupleData {
    union {
      struct {
        bool uncouple : 1;
        bool hidden : 1;
        bool _rfu : 1;
        uint8_t uncoupleType : 5;
      };
      uint8_t flags;
    };
    uint16_t id;
  } _uncoupleData;  // 3 bytes

#ifndef DISABLE_EEPROM
  // Address in eeprom of first byte of the _uncoupleData struct (containing the coupled flag).
  // Set to zero if the object has not been saved in EEPROM, e.g. for newly created Uncouples, and 
  // for all LCN uncouples.
  uint16_t _eepromAddress = 0;
#endif

  // Pointer to next uncouple on linked list.
  Uncouple *_nextUncouple = 0;

  /*
   * Constructor
   */
  Uncouple(uint16_t id, uint8_t uncoupleType, bool uncouple) {
    _uncoupleData.id = id;
    _uncoupleData.uncoupleType = uncoupleType;
    _uncoupleData.uncouple = uncouple;
    _uncoupleData.hidden=false;
    add(this);
  }

  /* 
   * Static data
   */ 

  static Uncouple *_firstUncouple;
  static int _uncouplelistHash;

  /* 
   * Virtual functions
   */

  virtual bool setUncoupleInternal(bool uncouple) = 0;  // Mandatory in subclass
  virtual void save() {}
  
  /*
   * Static functions
   */


  static void add(Uncouple *uc);
  
public:
  static Uncouple *get(uint16_t id);
  /* 
   * Static data
   */
  static int uncouplelistHash;
  static const bool useClassicUncoupleCommands;
  
  /*
   * Public base class functions
   */
  inline bool isCouple() { return !_uncoupleData.uncouple; };
  inline bool isUncouple() { return _uncoupleData.uncouple; }
  inline bool isHidden() { return _uncoupleData.hidden; }
  inline void setHidden(bool h) { _uncoupleData.hidden=h; }
  inline bool isType(uint8_t type) { return _uncoupleData.uncoupleType == type; }
  inline uint16_t getId() { return _uncoupleData.id; }
  inline Uncouple *next() { return _nextUncouple; }
  void printState(Print *stream);
  /* 
   * Virtual functions
   */
  virtual void print(Print *stream) {
    (void)stream;  // avoid compiler warnings.
  }
  virtual ~Uncouple() {}   // Destructor

  /*
   * Public static functions
   */
  inline static bool exists(uint16_t id) { return get(id) != 0; }

  static bool remove(uint16_t id);

  static bool isUncouple(uint16_t id);

  inline static bool isCouple(uint16_t id) {
    return !isUncouple(id);
  }

  static bool setUncouple(uint16_t id, bool uncouple);
  
  inline static bool setUncouple(uint16_t id) {
    return setUncouple(id, true);
  }

  inline static bool setCouple(uint16_t id) {
    return setUncouple(id, false);
  }

  static bool setUncoupleStateOnly(uint16_t id, bool uncouple);

  inline static Uncouple *first() { return _firstUncouple; }

#ifndef DISABLE_EEPROM
  // Load all uncouple definitions.
  static void load();
  // Load one uncouple definition
  static Uncouple *loadUncouple();
  // Save all uncouple definitions
  static void store();
#endif
  static bool printAll(Print *stream) {
    bool gotOne=false;
    for (Uncouple *uc = _firstUncouple; uc != 0; uc = uc->_nextUncouple)
      if (!uc->isHidden()) {
        gotOne=true;
        StringFormatter::send(stream, F("<H %d %d>\n"),uc->getId(), uc->isUncouple());
      }
    return gotOne;
  }


};


/*************************************************************************************
 * ServoUncouple - Uncouple controlled by servo device.
 * 
 *************************************************************************************/
class ServoUncouple : public Uncouple {
private:
  // ServoUncoupleData contains data specific to this subclass that is 
  // written to EEPROM when the uncouple is saved.
  struct ServoUncoupleData {
    VPIN vpin;
    uint16_t uncouplePosition : 12;
    uint16_t couplePosition : 12;
    uint8_t profile;
  } _servoUncoupleData; // 6 bytes

  // Constructor
  ServoUncouple(uint16_t id, VPIN vpin, uint16_t uncouplePosition, uint16_t couplePosition, uint8_t profile, bool uncouple);

public:
  // Create function
  static Uncouple *create(uint16_t id, VPIN vpin, uint16_t uncouplePosition, uint16_t couplePosition, uint8_t profile, bool uncouple=false);

  // Load a Servo uncouple definition from EEPROM.  The common Uncouple data has already been read at this point.
  static Uncouple *load(struct UncoupleData *uncoupleData);
  void print(Print *stream) override;

protected:
  // ServoUncouple-specific code for throwing or closing a servo uncouple.
  bool setUncoupleInternal(bool uncouple) override;
  void save() override;

};

/*************************************************************************************
 * DCCUncouple - Uncouple controlled by DCC Accessory Controller.
 * 
 *************************************************************************************/
class DCCUncouple : public Uncouple {
private:
  // DCCUncoupleData contains data specific to this subclass that is 
  // written to EEPROM when the uncouple is saved.
  struct DCCUncoupleData {
    // DCC address (Address in bits 15-2, subaddress in bits 1-0)
    struct {
      uint16_t address : 14;
      uint8_t subAddress : 2;
    };
  } _dccUncoupleData; // 2 bytes

  // Constructor
  DCCUncouple(uint16_t id, uint16_t address, uint8_t subAdd);

public:
  // Create function
  static Uncouple *create(uint16_t id, uint16_t add, uint8_t subAdd);
  // Load a VPIN uncouple definition from EEPROM.  The common Uncouple data has already been read at this point.
  static Uncouple *load(struct UncoupleData *uncoupleData);
  void print(Print *stream) override;

protected:
  bool setUncoupleInternal(bool uncouple) override;
  void save() override;

};


/*************************************************************************************
 * VpinUncouple - Uncouple controlled through a HAL vpin.
 * 
 *************************************************************************************/
class VpinUncouple : public Uncouple {
private:
  // VpinUncoupleData contains data specific to this subclass that is 
  // written to EEPROM when the uncouple is saved.
  struct VpinUncoupleData {
    VPIN vpin;
  } _vpinUncoupleData; // 2 bytes

  // Constructor
 VpinUncouple(uint16_t id, VPIN vpin, bool uncouple);

public:
  // Create function
  static Uncouple *create(uint16_t id, VPIN vpin, bool uncouple=false);

  // Load a VPIN uncouple definition from EEPROM.  The common Uncouple data has already been read at this point.
  static Uncouple *load(struct UncoupleData *uncoupleData);
  void print(Print *stream) override;

protected:
  bool setUncoupleInternal(bool uncouple) override;
  void save() override;

};


// /*************************************************************************************
//  * LCNUncouple - Uncouple controlled by Loconet
//  * 
//  *************************************************************************************/
// class LCNUncouple : public Uncouple {
// private:
//   // LCNUncouple has no specific data, and in any case is not written to EEPROM!
//   // struct LCNUncoupleData {
//   // } _lcnUncoupleData; // 0 bytes

//   // Constructor 
//   LCNUncouple(uint16_t id, bool coupled);

// public:
//   // Create function
//   static Uncouple *create(uint16_t id, bool coupled=true);


//   bool setClosedInternal(bool close) override;

//   // LCN uncouples not saved to EEPROM.
//   //void save() override {  }
//   //static Uncouple *load(struct UncoupleData *uncoupleData) {

//   void print(Print *stream) override;

// };

#endif
