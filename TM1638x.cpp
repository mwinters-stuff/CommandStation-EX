#include "Arduino.h"
#include "TM1638x.h"
#include "DIAG.h"
#include "IODevice.h"



// buttons K3/KS1-8
uint8_t TM1638x::getButtons(){
  ArduinoPins::fastWriteDigital(_stb_pin, LOW);
  writeData(INSTRUCTION_READ_KEY);
  pinMode(_dio_pin, INPUT);
  ArduinoPins::fastWriteDigital(_clk_pin, LOW);
  _buttons=0;
  for (uint8_t eachByte=0; eachByte<4;eachByte++) {
    uint8_t value = 0;
	  for (uint8_t eachBit = 0; eachBit < 8; eachBit++) {
		  ArduinoPins::fastWriteDigital(_clk_pin, HIGH);
			value |= ArduinoPins::fastReadDigital(_dio_pin) << eachBit;
		  ArduinoPins::fastWriteDigital(_clk_pin, LOW);
	  }
    _buttons |= value << eachByte; 
    delayMicroseconds(1);
  }
  pinMode(_dio_pin, OUTPUT);
  ArduinoPins::fastWriteDigital(_stb_pin, HIGH);
  return _buttons;
}


void TM1638x::displayDig(uint8_t digitId, uint8_t pgfedcba){
  if (digitId>7) return;
  setDataInstruction(DISPLAY_TURN_ON | _pulse);
  setDataInstruction(INSTRUCTION_WRITE_DATA| INSTRUCTION_ADDRESS_FIXED);
  writeDataAt(FIRST_DISPLAY_ADDRESS+14-(digitId*2), pgfedcba);
}

void TM1638x::displayClear(){
  setDataInstruction(DISPLAY_TURN_ON | _pulse);
  setDataInstruction(INSTRUCTION_WRITE_DATA | INSTRUCTION_ADDRESS_FIXED);
  for (uint8_t i=0;i<15;i+=2){
    writeDataAt(FIRST_DISPLAY_ADDRESS+i,0x00);
  }
}

void TM1638x::writeLed(uint8_t num,bool state){
  if ((num<1) | (num>8)) return;
  setDataInstruction(DISPLAY_TURN_ON | _pulse);
  setDataInstruction(INSTRUCTION_WRITE_DATA | INSTRUCTION_ADDRESS_FIXED);
  writeDataAt(FIRST_DISPLAY_ADDRESS + (num*2-1), state);
}

void TM1638x::displayTurnOn(){
  setDataInstruction(DISPLAY_TURN_ON | _pulse);
  _isOn = true;
}

void TM1638x::displayTurnOff(){
  setDataInstruction(DISPLAY_TURN_OFF | _pulse);
  _isOn = false;
}

void TM1638x::displaySetBrightness(pulse_t newpulse){
  if ((newpulse<PULSE1_16) | (newpulse>PULSE14_16)) return;
  _pulse = newpulse;
  uint8_t data = (_isOn) ? DISPLAY_TURN_ON : DISPLAY_TURN_OFF;
  data |= _pulse;
  setDataInstruction(data);
}

void TM1638x::writeData(uint8_t data){
	for (uint8_t i = 0; i < 8; i++)  {
		ArduinoPins::fastWriteDigital(_dio_pin, data & 1);
		data >>= 1;
		ArduinoPins::fastWriteDigital(_clk_pin, HIGH);
		ArduinoPins::fastWriteDigital(_clk_pin, LOW);		
	}
} 

void TM1638x::writeDataAt(uint8_t displayAddress, uint8_t data){
    ArduinoPins::fastWriteDigital(_stb_pin, LOW);
    writeData(displayAddress);
    writeData(data);
    ArduinoPins::fastWriteDigital(_stb_pin, HIGH);
    delayMicroseconds(1);
}

void TM1638x::setDataInstruction(uint8_t dataInstruction){
  ArduinoPins::fastWriteDigital(_stb_pin, LOW);
  writeData(dataInstruction);
  ArduinoPins::fastWriteDigital(_stb_pin, HIGH);
  delayMicroseconds(1);  
}

void TM1638x::test(){
  DIAG(F("TM1638x test"));
  uint8_t val=0;
  for(uint8_t i=0;i<5;i++){
    //setDataInstruction(DISPLAY_TURN_ON | _pulse);
    displayTurnOn();
    setDataInstruction(INSTRUCTION_WRITE_DATA| INSTRUCTION_ADDRESS_AUTO);
    ArduinoPins::fastWriteDigital(_stb_pin, LOW);
    writeData(FIRST_DISPLAY_ADDRESS);
    for(uint8_t i=0;i<16;i++)
      writeData(val);
    ArduinoPins::fastWriteDigital(_stb_pin, HIGH);
    delay(1000);
    val = ~val;
  }

}