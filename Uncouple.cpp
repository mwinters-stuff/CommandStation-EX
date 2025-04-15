/*
 *  © 2025 Mathew Winters
 *  © 2021 Neil McKechnie
 *  © 2021 M Steve Todd
 *  © 2021 Fred Decker
 *  © 2020-2021 Harald Barth
 *  © 2020-2021 Chris Harlow
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

#include "defines.h"  // includes config.h
#ifndef DISABLE_EEPROM
#include "EEStore.h"
#endif
#include "CommandDistributor.h"
#include "DCC.h"
#include "EXRAIL2.h"
#include "LCN.h"
#include "StringFormatter.h"
#include "Uncouple.h"
#ifdef EESTOREDEBUG
#include "DIAG.h"
#endif

/*
 * Protected static data
 */

/* static */ Uncouple *Uncouple::_firstUncouple = 0;

/*
 * Public static data
 */
/* static */ int Uncouple::uncouplelistHash = 0;

/*
 * Protected static functions
 */

/* static */ Uncouple *Uncouple::get(uint16_t id) {
  // Find uncouple object from list.
  for (Uncouple *uc = _firstUncouple; uc != NULL; uc = uc->_nextUncouple)
    if (uc->_uncoupleData.id == id) return uc;
  return NULL;
}

// Add new uncouple to end of chain
/* static */ void Uncouple::add(Uncouple *uc) {
  if (!_firstUncouple)
    _firstUncouple = uc;
  else {
    // Find last object on chain
    Uncouple *ptr = _firstUncouple;
    for (; ptr->_nextUncouple != 0; ptr = ptr->_nextUncouple) {
    }
    // Line new object to last object.
    ptr->_nextUncouple = uc;
  }
  uncouplelistHash++;
}

// Remove nominated uncouple from uncouple linked list and delete the object.
/* static */ bool Uncouple::remove(uint16_t id) {
  Uncouple *uc, *pp = NULL;

  for (uc = _firstUncouple; uc != NULL && uc->_uncoupleData.id != id;
       pp = uc, uc = uc->_nextUncouple) {
  }
  if (uc == NULL) return false;

  if (uc == _firstUncouple)
    _firstUncouple = uc->_nextUncouple;
  else
    pp->_nextUncouple = uc->_nextUncouple;

  delete (ServoUncouple *)uc;

  uncouplelistHash++;
  return true;
}

/*
 * Public static functions
 */

/* static */ bool Uncouple::isUncouple(uint16_t id) {
  Uncouple *uc = get(id);
  if (uc)
    return uc->isUncouple();
  else
    return false;
}

/* static */ bool Uncouple::setUncoupleStateOnly(uint16_t id, bool uncouple) {
  Uncouple *uc = get(id);
  if (!uc) return false;
  // I know it says setUnCoupleStateOnly, but we need to tell others
  // that the state has changed too. But we only broadcast if there
  // really has been a change.
  if (uc->_uncoupleData.uncouple != uncouple) {
    uc->_uncoupleData.uncouple = uncouple;
    CommandDistributor::broadcastUncouple(id, uncouple);
  }
#if defined(EXRAIL_ACTIVE)
  RMFT2::uncoupleEvent(id, uncouple);
#endif
  return true;
}

// Static setClosed function is invoked from close(), throw() etc. to perform
// the
//  common parts of the uncouple operation.  Code which is specific to a
//  uncouple type should be placed in the virtual function
//  setClosedInternal(bool) which is called from here.
/* static */ bool Uncouple::setUncouple(uint16_t id, bool uncouple) {
#if defined(DIAG_IO)
  DIAG(F("Uncouple(%d,%c)"), id, uncouple ? 'u' : 'c');
#endif
  Uncouple *uc = Uncouple::get(id);
  if (!uc) return false;
  bool ok = uc->setUncoupleInternal(uncouple);

  if (ok) {
    uc->setUncoupleStateOnly(id, uncouple);
#ifndef DISABLE_EEPROM
    // Write byte containing new coupled/thrown state to EEPROM if required.
    // Note that eepromAddress is always zero for LCN uncouples.
    if (EEStore::eeStore->data.nUncouples > 0 && uc->_eepromAddress > 0)
      EEPROM.put(uc->_eepromAddress, uc->_uncoupleData.flags);
#endif
  }
  return ok;
}

#ifndef DISABLE_EEPROM
// Load all uncouple objects
/* static */ void Uncouple::load() {
  for (uint16_t i = 0; i < EEStore::eeStore->data.nUncouples; i++) {
    Uncouple::loadUncouple();
  }
}

// Save all uncouple objects
/* static */ void Uncouple::store() {
  EEStore::eeStore->data.nUncouples = 0;
  for (Uncouple *uc = _firstUncouple; uc != 0; uc = uc->_nextUncouple) {
    uc->save();
    EEStore::eeStore->data.nUncouples++;
  }
}

// Load one uncouple from EEPROM
/* static */ Uncouple *Uncouple::loadUncouple() {
  Uncouple *uc = 0;
  // Read uncouple type from EEPROM
  struct UncoupleData uncoupleData;
  int eepromAddress =
      EEStore::pointer() +
      offsetof(struct UncoupleData,
               flags);  // Address of byte containing the coupled flag.
  EEPROM.get(EEStore::pointer(), uncoupleData);
  EEStore::advance(sizeof(uncoupleData));

  switch (uncoupleData.uncoupleType) {
    case UNCOUPLE_SERVO:
      // Servo uncouple
      uc = ServoUncouple::load(&uncoupleData);
      break;
    case UNCOUPLE_DCC:
      // DCC Accessory uncouple
      uc = DCCUncouple::load(&uncoupleData);
      break;
    case UNCOUPLE_VPIN:
      // VPIN uncouple
      uc = VpinUncouple::load(&uncoupleData);
      break;
    default:
      // If we find anything else, then we don't know what it is or how long it
      // is, so we can't go any further through the EEPROM!
      return NULL;
  }
  if (uc) {
    // Save EEPROM address in object.  Note that LCN uncouples always have
    // eepromAddress of zero.
    uc->_eepromAddress = eepromAddress + offsetof(struct UncoupleData, flags);
  }

#ifdef EESTOREDEBUG
  printAll(&USB_SERIAL);
#endif
  return uc;
}
#endif

/*************************************************************************************
 * ServoUncouple - Uncouple controlled by servo device.
 *
 *************************************************************************************/

// Private Constructor
ServoUncouple::ServoUncouple(uint16_t id, VPIN vpin, uint16_t uncouplePosition,
                             uint16_t couplePosition, uint8_t profile,
                             bool uncouple)
    : Uncouple(id, UNCOUPLE_SERVO, uncouple) {
  _servoUncoupleData.vpin = vpin;
  _servoUncoupleData.uncouplePosition = uncouplePosition;
  _servoUncoupleData.couplePosition = couplePosition;
  _servoUncoupleData.profile = profile;
}

// Create function
/* static */ Uncouple *ServoUncouple::create(uint16_t id, VPIN vpin,
                                             uint16_t uncouplePosition,
                                             uint16_t couplePosition,
                                             uint8_t profile, bool uncouple) {
#ifndef IO_NO_HAL
  Uncouple *uc = get(id);
  if (uc) {
    // Object already exists, check if it is usable
    if (uc->isType(UNCOUPLE_SERVO)) {
      // Yes, so set parameters
      ServoUncouple *st = (ServoUncouple *)uc;
      st->_servoUncoupleData.vpin = vpin;
      st->_servoUncoupleData.uncouplePosition = uncouplePosition;
      st->_servoUncoupleData.couplePosition = couplePosition;
      st->_servoUncoupleData.profile = profile;
      // Don't touch the _closed parameter, retain the original value.

      // We don't really need to do the following, since a call to
      // IODevice::_writeAnalogue
      //  will provide all the data that is required!  However, if someone has
      //  configured a Uncouple, we should ensure that the SET() RESET() and
      //  other commands that use write() behave consistently with the uncouple
      //  commands.
      IODevice::configureServo(vpin, uncouplePosition, couplePosition, profile,
                               0, uncouple);

      // Set position directly to specified position - we don't know where it is
      // moving from.
      IODevice::writeAnalogue(
          vpin, uncouple ? uncouplePosition : couplePosition, PCA9685::Instant);

      return uc;
    } else {
      // Incompatible object, delete and recreate
      remove(id);
    }
  }
  uc = (Uncouple *)new ServoUncouple(id, vpin, uncouplePosition, couplePosition,
                                     profile, uncouple);
  DIAG(F("Uncouple 0x%x size %d size %d"), uc, sizeof(Uncouple),
       sizeof(struct UncoupleData));
  IODevice::writeAnalogue(vpin, uncouple ? uncouplePosition : couplePosition,
                          PCA9685::Instant);
  return uc;
#else
  (void)id;
  (void)vpin;
  (void)uncouplePosition;
  (void)couplePosition;
  (void)profile;
  (void)uncouple;  // avoid compiler warnings.
  return NULL;
#endif
}

// Load a Servo uncouple definition from EEPROM.  The common Uncouple data has
// already been read at this point.
Uncouple *ServoUncouple::load(struct UncoupleData *uncoupleData) {
#ifndef DISABLE_EEPROM
  ServoUncoupleData servoUncoupleData;
  // Read class-specific data from EEPROM
  EEPROM.get(EEStore::pointer(), servoUncoupleData);
  EEStore::advance(sizeof(servoUncoupleData));

  // Create new object
  Uncouple *uc = ServoUncouple::create(
      uncoupleData->id, servoUncoupleData.vpin,
      servoUncoupleData.uncouplePosition, servoUncoupleData.couplePosition,
      servoUncoupleData.profile, uncoupleData->uncouple);
  return uc;
#else
  (void)uncoupleData;
  return NULL;
#endif
}

// For DCC++ classic compatibility, state reported to JMRI is 1 for thrown and 0
// for coupled
void ServoUncouple::print(Print *stream) {
  StringFormatter::send(stream, F("<H %d SERVO %d %d %d %d %d>\n"),
                        _uncoupleData.id, _servoUncoupleData.vpin,
                        _servoUncoupleData.uncouplePosition,
                        _servoUncoupleData.couplePosition,
                        _servoUncoupleData.profile, _uncoupleData.uncouple);
}

// ServoUncouple-specific code for throwing or closing a servo uncouple.
bool ServoUncouple::setUncoupleInternal(bool uncouple) {
#ifndef IO_NO_HAL
  IODevice::writeAnalogue(_servoUncoupleData.vpin,
                          uncouple ? _servoUncoupleData.uncouplePosition
                                   : _servoUncoupleData.couplePosition,
                          _servoUncoupleData.profile);
#else
  (void)uncouple;  // avoid compiler warnings
#endif
  return true;
}

void ServoUncouple::save() {
#ifndef DISABLE_EEPROM
  // Write uncouple definition and current position to EEPROM
  // First write common servo data, then
  // write the servo-specific data
  EEPROM.put(EEStore::pointer(), _uncoupleData);
  EEStore::advance(sizeof(_uncoupleData));
  EEPROM.put(EEStore::pointer(), _servoUncoupleData);
  EEStore::advance(sizeof(_servoUncoupleData));
#endif
}

/*************************************************************************************
 * DCCUncouple - Uncouple controlled by DCC Accessory Controller.
 *
 *************************************************************************************/

// DCCUncoupleData contains data specific to this subclass that is
// written to EEPROM when the uncouple is saved.
struct DCCUncoupleData {
  // DCC address (Address in bits 15-2, subaddress in bits 1-0
  uint16_t address;  // CS currently supports linear address 1-2048
                     // That's DCC accessory address 1-512 and subaddress 0-3.
} _dccUncoupleData;  // 2 bytes

// Constructor
DCCUncouple::DCCUncouple(uint16_t id, uint16_t address, uint8_t subAdd)
    : Uncouple(id, UNCOUPLE_DCC, false) {
  _dccUncoupleData.address = address;
  _dccUncoupleData.subAddress = subAdd;
}

// Create function
/* static */ Uncouple *DCCUncouple::create(uint16_t id, uint16_t add,
                                           uint8_t subAdd) {
  Uncouple *uc = get(id);
  if (uc) {
    // Object already exists, check if it is usable
    if (uc->isType(UNCOUPLE_DCC)) {
      // Yes, so set parameters<T>
      DCCUncouple *dt = (DCCUncouple *)uc;
      dt->_dccUncoupleData.address = add;
      dt->_dccUncoupleData.subAddress = subAdd;
      // Don't touch the _closed parameter, retain the original value.
      return uc;
    } else {
      // Incompatible object, delete and recreate
      remove(id);
    }
  }
  uc = (Uncouple *)new DCCUncouple(id, add, subAdd);
  return uc;
}

// Load a DCC uncouple definition from EEPROM.  The common Uncouple data has
// already been read at this point.
/* static */ Uncouple *DCCUncouple::load(struct UncoupleData *uncoupleData) {
#ifndef DISABLE_EEPROM
  DCCUncoupleData dccUncoupleData;
  // Read class-specific data from EEPROM
  EEPROM.get(EEStore::pointer(), dccUncoupleData);
  EEStore::advance(sizeof(dccUncoupleData));

  // Create new object
  DCCUncouple *uc = new DCCUncouple(uncoupleData->id, dccUncoupleData.address,
                                    dccUncoupleData.subAddress);

  return uc;
#else
  (void)uncoupleData;
  return NULL;
#endif
}

void DCCUncouple::print(Print *stream) {
  StringFormatter::send(stream, F("<H %d DCC %d %d %d>\n"), _uncoupleData.id,
                        _dccUncoupleData.address, _dccUncoupleData.subAddress,
                        !_uncoupleData.uncouple);
  // Also report using classic DCC++ syntax for DCC accessory uncouples, since
  // JMRI expects this.
  StringFormatter::send(stream, F("<H %d %d %d %d>\n"), _uncoupleData.id,
                        _dccUncoupleData.address, _dccUncoupleData.subAddress,
                        !_uncoupleData.uncouple);
}

bool DCCUncouple::setUncoupleInternal(bool uncouple) {
  // DCC++ Classic behaviour is that Throw writes a 1 in the packet,
  // and Close writes a 0.
  // RCN-213 specifies that Throw is 0 and Close is 1.
#ifndef DCC_UNCOUPLES_RCN_213
  uncouple = !uncouple;
#endif
  DCC::setAccessory(_dccUncoupleData.address, _dccUncoupleData.subAddress,
                    uncouple);
  return true;
}

void DCCUncouple::save() {
#ifndef DISABLE_EEPROM
  // Write uncouple definition and current position to EEPROM
  // First write common servo data, then
  // write the servo-specific data
  EEPROM.put(EEStore::pointer(), _uncoupleData);
  EEStore::advance(sizeof(_uncoupleData));
  EEPROM.put(EEStore::pointer(), _dccUncoupleData);
  EEStore::advance(sizeof(_dccUncoupleData));
#endif
}

/*************************************************************************************
 * VpinUncouple - Uncouple controlled through a HAL vpin.
 *
 *************************************************************************************/

// Constructor
VpinUncouple::VpinUncouple(uint16_t id, VPIN vpin, bool coupled)
    : Uncouple(id, UNCOUPLE_VPIN, coupled) {
  _vpinUncoupleData.vpin = vpin;
}

// Create function
/* static */ Uncouple *VpinUncouple::create(uint16_t id, VPIN vpin,
                                            bool uncouple) {
  Uncouple *uc = get(id);
  if (uc) {
    // Object already exists, check if it is usable
    if (uc->isType(UNCOUPLE_VPIN)) {
      // Yes, so set parameters
      VpinUncouple *vt = (VpinUncouple *)uc;
      vt->_vpinUncoupleData.vpin = vpin;
      // Don't touch the _closed parameter, retain the original value.
      return uc;
    } else {
      // Incompatible object, delete and recreate
      remove(id);
    }
  }
  uc = (Uncouple *)new VpinUncouple(id, vpin, uncouple);
  return uc;
}

// Load a VPIN uncouple definition from EEPROM.  The common Uncouple data has
// already been read at this point.
/* static */ Uncouple *VpinUncouple::load(struct UncoupleData *uncoupleData) {
#ifndef DISABLE_EEPROM
  VpinUncoupleData vpinUncoupleData;
  // Read class-specific data from EEPROM
  EEPROM.get(EEStore::pointer(), vpinUncoupleData);
  EEStore::advance(sizeof(vpinUncoupleData));

  // Create new object
  VpinUncouple *uc = new VpinUncouple(uncoupleData->id, vpinUncoupleData.vpin,
                                      uncoupleData->uncouple);

  return uc;
#else
  (void)uncoupleData;
  return NULL;
#endif
}

// Report 1 for thrown, 0 for coupled.
void VpinUncouple::print(Print *stream) {
  StringFormatter::send(stream, F("<H %d VPIN %d %d>\n"), _uncoupleData.id,
                        _vpinUncoupleData.vpin, !_uncoupleData.uncouple);
}

bool VpinUncouple::setUncoupleInternal(bool uncouple) {
  IODevice::write(_vpinUncoupleData.vpin, uncouple);
  return true;
}

void VpinUncouple::save() {
#ifndef DISABLE_EEPROM
  // Write uncouple definition and current position to EEPROM
  // First write common servo data, then
  // write the servo-specific data
  EEPROM.put(EEStore::pointer(), _uncoupleData);
  EEStore::advance(sizeof(_uncoupleData));
  EEPROM.put(EEStore::pointer(), _vpinUncoupleData);
  EEStore::advance(sizeof(_vpinUncoupleData));
#endif
}

// /*************************************************************************************
//  * LCNUncouple - Uncouple controlled by Loconet
//  *
//  *************************************************************************************/

//   // LCNUncouple has no specific data, and in any case is not written to
//   EEPROM!
//   // struct LCNUncoupleData {
//   // } _lcnUncoupleData; // 0 bytes

//   // Constructor
//   LCNUncouple::LCNUncouple(uint16_t id, bool coupled) :
//     Uncouple(id, UNCOUPLE_LCN, coupled)
//   { }

//   // Create function
//   /* static */ Uncouple *LCNUncouple::create(uint16_t id, bool coupled) {
//     Uncouple *uc = get(id);
//     if (uc) {
//       // Object already exists, check if it is usable
//       if (uc->isType(UNCOUPLE_LCN)) {
//         // Yes, so return this object
//         return uc;
//       } else {
//         // Incompatible object, delete and recreate
//         remove(id);
//       }
//     }
//     uc = (Uncouple *)new LCNUncouple(id, coupled);
//     return uc;
//   }

//   bool LCNUncouple::setClosedInternal(bool close) {
//     // Assume that the LCN command still uses 1 for throw and 0 for close...
//     LCN::send('T', _uncoupleData.id, !close);
//     // The _uncoupleData.coupled flag should be updated by a message from the
//     LCN master.
//     // but in this implementation it is updated in setClosedStateOnly()
//     instead.
//     // If the LCN master updates this, setClosedStateOnly() and all
//     setClosedInternal()
//     // have to be updated accordingly so that the coupled flag is only set
//     once. return true;
//   }

//   // LCN uncouples not saved to EEPROM.
//   //void save() override {  }
//   //static Uncouple *load(struct UncoupleData *uncoupleData) {

//   // Report 1 for thrown, 0 for coupled.
//   void LCNUncouple::print(Print *stream) {
//     StringFormatter::send(stream, F("<H %d LCN %d>\n"), _uncoupleData.id,
//     !_uncoupleData.coupled);
//   }
