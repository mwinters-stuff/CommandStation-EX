/*
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

#include "IO_CMRI.h"

// Main loop function for CMRIbus.
// Work through list of nodes.  For each node, in separate loop entries
// send initialisation message (once only);  then send
// output message;  then send prompt for input data, and 
// process any response data received.
// When the slot time has finished, move on to the next device.
void CMRIbus::_loop(unsigned long currentMicros) {
  
  _currentMicros = currentMicros;

  while (_serial->available())
    processIncoming();

  // Send any data that needs sending.
  processOutgoing();

}

void CMRIbus::processOutgoing() {
  if (_currentNode == NULL) {
    // If we're between read/write cycles then don't do anything else.
    if (_currentMicros - _cycleStartTime < _cycleTime) return;
    // ... otherwise start processing the first node in the list
    _currentNode = _nodeListStart;
    _transmitState = TD_INIT;
    _cycleStartTime = _currentMicros;
  }
  if (_currentNode == NULL) return;
  switch (_transmitState) {
    case TD_IDLE:
    case TD_INIT:
      if (!_currentNode->isInitialised()) {
        sendInitialisation(_currentNode);
        _currentNode->setInitialised();
        _transmitState = TD_TRANSMIT;
        break;
      }
      /* fallthrough */
    case TD_TRANSMIT:
      sendData(_currentNode);
      _transmitState = TD_PROMPT;
      break;
    case TD_PROMPT:
      requestData(_currentNode);
      _transmitState = TD_RECEIVE;
      _timeoutStart = _currentMicros; // Start timeout on response
      break;
    case TD_RECEIVE: // Waiting for response / timeout
      if (_currentMicros - _timeoutStart > _timeoutPeriod) { 
        // End of time slot allocated for responses.
        _transmitState = TD_IDLE;
        // Reset state of receiver
        _receiveState = RD_SYN1;
        // Move to next node
        _currentNode = _currentNode->getNext();
      }
      break;
  }
}

// Process any data bytes received from a CMRInode.
void CMRIbus::processIncoming() {
  int data = _serial->read();
  if (data < 0) return;     // No characters to read

  if (!_currentNode) return;   // Not waiting for input, so ignore.

  uint8_t nextState = RD_SYN1;  // default to resetting state machine
  switch(_receiveState) {
    case RD_SYN1: 
      if (data == SYN) nextState = RD_SYN2; 
      break;
    case RD_SYN2:
      if (data == SYN) nextState = RD_STX; 
      break;
    case RD_STX:
      if (data == STX) nextState = RD_ADDR;
      break;
    case RD_ADDR:
      // If address doesn't match, then ignore everything until next SYN-SYN-STX.
      if (data == _currentNode->getAddress() + 65) nextState = RD_TYPE;
      break;
    case RD_TYPE:
      _receiveDataIndex = 0;  // Initialise data pointer
      if (data == 'R') nextState = RD_DATA;
      break;
    case RD_DATA: // data body
      if (data == DLE) // escape next character
        nextState = RD_ESCDATA;
      else if (data == ETX) { // end of data
        // End of data message.  Protocol has all data in one
        // message, so we don't need to wait any more.  Allow
        // transmitter to proceed with next node in list.
        _currentNode = _currentNode->getNext();
        _transmitState = TD_IDLE;
      } else {
        // Not end yet, so save data byte
        _currentNode->saveIncomingData(_receiveDataIndex++, data);
        nextState = RD_DATA; // wait for more data
      }
      break;
    case RD_ESCDATA: // escaped data byte
      _currentNode->saveIncomingData(_receiveDataIndex++, data);
      nextState = RD_DATA;
      break;
  }
  _receiveState = nextState;
}

// Constructor for CMRInode object
CMRInode::CMRInode(VPIN firstVpin, int nPins, uint8_t busNo, uint8_t address, char type, uint16_t inputs, uint16_t outputs) {
  _firstVpin = firstVpin;
  _nPins = nPins;
  _busNo = busNo;
  _address = address;
  _type = type;

  switch (_type) {
    case 'M': // SMINI, fixed 24 inputs and 48 outputs
      _numInputs = 24;
      _numOutputs = 48;
      break;
    case 'C': // CPNODE with 16 to 144 inputs/outputs using 8-bit cards
      _numInputs = inputs;
      _numOutputs = outputs;
      break;
    case 'N': // Classic USIC and SUSIC using 24 bit i/o cards
    case 'X': // SUSIC using 32 bit i/o cards
    default:
      DIAG(F("CMRInode: bus:%d address:%d ERROR unsupported type %c"), _busNo, _address, _type);
      return; // Don't register device.
  }
  if ((unsigned int)_nPins < _numInputs + _numOutputs)
    DIAG(F("CMRInode: bus:%d address:%d WARNING number of Vpins does not cover all inputs and outputs"), _busNo, _address);

  // Allocate memory for states
  _inputStates = (uint8_t *)calloc((_numInputs+7)/8, 1);
  _outputStates = (uint8_t *)calloc((_numOutputs+7)/8, 1);
  if (!_inputStates || !_outputStates) {
    DIAG(F("CMRInode: ERROR insufficient memory"));
    return;
  }

  // Add this device to HAL device list
  IODevice::addDevice(this);

  // Add CMRInode to CMRIbus object.
  CMRIbus *bus = CMRIbus::findBus(_busNo);
  if (bus != NULL) {
    bus->addNode(this);
    return;
  }
}

// Link to chain of CMRI bus instances
CMRIbus *CMRIbus::_busList = NULL;
