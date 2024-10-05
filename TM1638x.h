/*!
 * @file TM1638.h
 * @brief Arduino library for interface with TM1638 chip. 
 * @n read buttons, switch leds, display on 7segment.
 * @author [Damien](web@varrel.fr)
 * @version  V1.0.1
 * @date  2024-02-06
 * @url https://github.com/dvarrel/TM1638.git
 * @module https://fr.aliexpress.com/item/32832772646.html
 */

#ifndef _TM1638_H
#define _TM1638_H
#include "Arduino.h"

#ifndef ON
#define ON 1
#endif
#ifndef OFF
#define OFF 0
#endif

    typedef enum{
      PULSE1_16,
      PULSE2_16,
      PULSE4_16,
      PULSE10_16,
      PULSE11_16,
      PULSE12_16,
      PULSE13_16,
      PULSE14_16
    } pulse_t;

    typedef enum{
      S1,S2,S3,S4,
      S5,S6,S7,S8
    } button_t;

class TM1638x{
  private:
    static const byte 
    INSTRUCTION_WRITE_DATA=0x40,
    INSTRUCTION_READ_KEY=0x42,
    INSTRUCTION_ADDRESS_AUTO=0x40,
    INSTRUCTION_ADDRESS_FIXED=0x44,
    INSTRUCTION_NORMAL_MODE=0x40,
    INSTRUCTION_TEST_MODE=0x48,

    FIRST_DISPLAY_ADDRESS=0xC0,

    DISPLAY_TURN_OFF=0x80,
    DISPLAY_TURN_ON=0x88;

        
    uint8_t _clk_pin;
    uint8_t _stb_pin;
    uint8_t _dio_pin;
    uint8_t _buttons;
    uint8_t _pulse;
    bool _isOn;

  public:
    TM1638x(uint8_t clk_pin, uint8_t dio_pin, uint8_t stb_pin){
      _clk_pin = clk_pin;
      _stb_pin = stb_pin;
      _dio_pin = dio_pin;
      _pulse = PULSE1_16;
      _isOn = false;
      
      pinMode(stb_pin, OUTPUT);
      pinMode(clk_pin, OUTPUT);
      pinMode(dio_pin, OUTPUT);
      digitalWrite(stb_pin, HIGH);
      digitalWrite(clk_pin, HIGH);
      digitalWrite(dio_pin, HIGH);    
    }

    /**
    * @fn getButtons
    * @return state of 8 buttons
    */
    uint8_t getButtons();

    /**
    * @fn writeLed
    * @brief put led ON or OFF
    * @param num num of led(1-8)
    * @param state (true or false)
    */
    void writeLed(uint8_t num, bool state);
    
    
    /**
    * @fn displayDig
    * @brief set 7 segment display + dot
    * @param digitId num of digit(0-7)
    * @param val value 8 bits
    */
    void displayDig(uint8_t digitId, uint8_t pgfedcba);

    /**
    * @fn displayClear
    * @brief switch off all leds and segment display
    */
    void displayClear();

    /**
    * @fn displayTurnOff
    * @brief turn on lights
    */
    void displayTurnOff();

    /**
    * @fn displayTurnOn
    * @brief turn off lights
    */
    void displayTurnOn();

    /**
    * @fn displaySetBrightness
    * @brief set display brightness
    * @param pulse_t (0-7)
    */
    void displaySetBrightness(pulse_t pulse);

    /**
    * @fn reset
    * @brief switch off all displays-leds
    */
    void reset();

    /**
    * @fn test
    * @brief blink all displays and leds
    */
    void test();

  private:
    void writeData(uint8_t data);
    void writeDataAt(uint8_t displayAddress, uint8_t data);
    void setDisplayMode(uint8_t displayMode);
    void setDataInstruction(uint8_t dataInstruction);
};
#endif