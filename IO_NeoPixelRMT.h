/*
 *  © 2026 Mathew Winters (with Github copilot)
 * 
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

 #ifndef IO_EX_NeoPixelRMT_H
#define IO_EX_NeoPixelRMT_H

#if defined(ARDUINO_ARCH_ESP32)

#include "IO_NeoPixel.h"
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>

#define MAX_LEDS 256

class NeoPixelRMT : public IODevice {
public:
  static void create(VPIN vpin, int nPixels, int dataPin,
                     uint16_t mode=(NEO_GRB | NEO_KHZ800)) {
    if (nPixels > 0 && static_cast<uint32_t>(vpin) + nPixels <= 0x10000UL &&
        checkNoOverlapRange(vpin, nPixels))
      new NeoPixelRMT(vpin, nPixels, dataPin, mode);
  }

  static bool startChase(int brightness) {
    NeoPixelRMT *device = instance();
    if (brightness < 0 || brightness > 255 || device == nullptr ||
        !device->_rmtReady || device->_rmtFailed || device->_pixels == nullptr ||
        device->_on == nullptr)
      return false;

    portENTER_CRITICAL(&device->_bufferMux);
    for (int pixel = 0; pixel < device->_nPins; pixel++)
      device->_on[pixel] = 0;
    device->_framePending = true;
    portEXIT_CRITICAL(&device->_bufferMux);
    device->_pixelChaseBrightness = brightness;
    device->_pixelChaseIndex = 0;
    device->_pixelChaseLastStepMicros = micros();
    device->_pixelChaseActive = true;
    DIAG(F("NeoPixel RMT chase VPINs:%u-%u brightness:%u"),
         (unsigned int)device->_firstVpin,
         (unsigned int)(device->_firstVpin + device->_nPins - 1),
         (unsigned int)brightness);
    return true;
  }

private:
  struct RGB {
    byte red;
    byte green;
    byte blue;
  };

  static constexpr uint16_t TICKS_800_T0H = 14;
  static constexpr uint16_t TICKS_800_T0L = 36;
  static constexpr uint16_t TICKS_800_T1H = 28;
  static constexpr uint16_t TICKS_800_T1L = 22;
  static constexpr uint16_t TICKS_400_T0H = 35;
  static constexpr uint16_t TICKS_400_T0L = 65;
  static constexpr uint16_t TICKS_400_T1H = 70;
  static constexpr uint16_t TICKS_400_T1L = 30;
  static constexpr uint16_t RESET_TICKS = 12000;

  static bool checkNoOverlapRange(VPIN firstVpin, int nPixels) {
    while (nPixels > 0) {
      uint8_t chunk = nPixels > UINT8_MAX ? UINT8_MAX : nPixels;
      if (!checkNoOverlap(firstVpin, chunk)) return false;
      firstVpin += chunk;
      nPixels -= chunk;
    }
    return true;
  }

  NeoPixelRMT(VPIN firstVpin, int nPixels, int dataPin, uint16_t mode)
      : IODevice(firstVpin, nPixels), _dataPin(dataPin) {
    instance() = this;
    _deviceState = DEVSTATE_INITIALISING;
    _redOffset = (mode >> 4) & 0x03;
    _greenOffset = (mode >> 2) & 0x03;
    _blueOffset = mode & 0x03;
    _whiteOffset = (mode >> 6) & 0x03;
    _bytesPerPixel = _whiteOffset == _redOffset ? 3 : 4;
    _is800KHz = (mode & NEO_KHZ400) == 0;

    if (nPixels <= 0 || static_cast<size_t>(nPixels) > MAX_LEDS) {
      DIAG(F("NeoPixel RMT number of leds %d > max of %d"), nPixels, MAX_LEDS);
      _deviceState = DEVSTATE_FAILED;
    } else {
      _pixels = static_cast<RGB *>(malloc(nPixels * sizeof(RGB)));
      _on = static_cast<byte *>(calloc(nPixels, sizeof(byte)));
      _frame = static_cast<RGB *>(malloc(nPixels * sizeof(RGB)));
      _frameOn = static_cast<byte *>(malloc(nPixels * sizeof(byte)));
      _items = static_cast<rmt_item32_t *>(malloc(
          (static_cast<size_t>(nPixels) * _bytesPerPixel * 8 + 2) * sizeof(rmt_item32_t)));

      if (_pixels == nullptr || _on == nullptr || _frame == nullptr ||
          _frameOn == nullptr || _items == nullptr) {
        free(_pixels);
        free(_on);
        free(_frame);
        free(_frameOn);
        free(_items);
        _pixels = nullptr;
        _on = nullptr;
        _frame = nullptr;
        _frameOn = nullptr;
        _items = nullptr;
        DIAG(F("NeoPixel RMT Out of Memory"));
        _deviceState = DEVSTATE_FAILED;
      } else {
        memset(_pixels, 0xFF, nPixels * sizeof(RGB));
        _channel = allocateChannel();
        if (_channel == RMT_CHANNEL_MAX) {
          DIAG(F("NeoPixel RMT Out of Memory"));
          _deviceState = DEVSTATE_FAILED;
        }
      }
    }

    addDevice(this);
  }

  static rmt_channel_t allocateChannel() {
#ifdef MOTOR_SHIELD_TYPE
    static uint8_t nextChannel = 4;
#else
    static uint8_t nextChannel = 0;
#endif
    if (nextChannel >= RMT_CHANNEL_MAX) {
      DIAG(F("NeoPixel RMT Could not allocate RMT channel"));
      return RMT_CHANNEL_MAX;
    }
    return static_cast<rmt_channel_t>(nextChannel++);
  }

  void _begin() override {
    if (_rmtReady) return;
    if (_deviceState == DEVSTATE_FAILED) {
      _display();
      return;
    }

    _deviceState = DEVSTATE_INITIALISING;
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(
        static_cast<gpio_num_t>(_dataPin), _channel);
    config.clk_div = 2;
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

    auto _rmtError = rmt_config(&config);
    if (_rmtError == ESP_OK) {
      _rmtError = rmt_driver_install(_channel, 0, 0);
    }
    if (_rmtError != ESP_OK) {
      _rmtFailed = true;
      _deviceState = DEVSTATE_FAILED;
      _display();
      return;
    }

    _rmtReady = true;
  }

  bool copyFrame() {
    bool framePending;
    portENTER_CRITICAL(&_bufferMux);
    framePending = _framePending;
    if (framePending) {
      memcpy(_frame, _pixels, _nPins * sizeof(RGB));
      memcpy(_frameOn, _on, _nPins * sizeof(byte));
      _framePending = false;
    }
    portEXIT_CRITICAL(&_bufferMux);
    return framePending;
  }

  void encodeFrame() {
    const uint16_t t0h = _is800KHz ? TICKS_800_T0H : TICKS_400_T0H;
    const uint16_t t0l = _is800KHz ? TICKS_800_T0L : TICKS_400_T0L;
    const uint16_t t1h = _is800KHz ? TICKS_800_T1H : TICKS_400_T1H;
    const uint16_t t1l = _is800KHz ? TICKS_800_T1L : TICKS_400_T1L;
    size_t item = 0;

    for (int pixel = 0; pixel < _nPins; pixel++) {
      byte channels[4] = {0, 0, 0, 0};
      if (_frameOn[pixel]) {
        RGB colour = _frame[pixel];
        if (_bytesPerPixel == 4 && colour.red == colour.green &&
            colour.red == colour.blue) {
          channels[_whiteOffset] = colour.red;
        } else {
          channels[_redOffset] = colour.red;
          channels[_greenOffset] = colour.green;
          channels[_blueOffset] = colour.blue;
        }
      }

      for (byte channel = 0; channel < _bytesPerPixel; channel++) {
        for (int bit = 7; bit >= 0; bit--) {
          bool one = channels[channel] & (1 << bit);
          _items[item].level0 = 1;
          _items[item].duration0 = one ? t1h : t0h;
          _items[item].level1 = 0;
          _items[item].duration1 = one ? t1l : t0l;
          item++;
        }
      }
    }

    _items[item].level0 = 0;
    _items[item].duration0 = RESET_TICKS;
    _items[item].level1 = 0;
    _items[item].duration1 = 0;
    _itemCount = item + 1;
  }

  void _loop(unsigned long currentMicros) override {
    if (_rmtFailed && _deviceState != DEVSTATE_FAILED) {
      _deviceState = DEVSTATE_FAILED;
      _display();
    } else if (_rmtReady && _deviceState != DEVSTATE_NORMAL) {
      _deviceState = DEVSTATE_NORMAL;
      _display();
    }

    updateChase(currentMicros);
    if (!_rmtReady || _rmtFailed) return;

    if (_transmitting) {
      auto _rmtError = rmt_wait_tx_done(_channel, 0);
      if (_rmtError == ESP_ERR_TIMEOUT) return;
      if (_rmtError != ESP_OK) {
        _rmtFailed = true;
        _rmtReady = false;
        _deviceState = DEVSTATE_FAILED;
        _display();
        return;
      }
      _transmitting = false;
    }

    if (!copyFrame()) return;
    encodeFrame();
    auto _rmtError = rmt_write_items(_channel, _items, _itemCount, false);
    if (_rmtError != ESP_OK) {
      _rmtFailed = true;
      _rmtReady = false;
      _deviceState = DEVSTATE_FAILED;
      _display();
      return;
    }
    _transmitting = true;
  }

  void updateChase(unsigned long currentMicros) {
    if (!_pixelChaseActive) return;

    portENTER_CRITICAL(&_bufferMux);
    if (_pixelChaseIndex >= _nPins) {
      if (static_cast<uint32_t>(currentMicros - _pixelChaseHoldStartedMicros) <
          60000000UL) {
        portEXIT_CRITICAL(&_bufferMux);
        return;
      }
      memset(_on, 0, _nPins * sizeof(byte));
      _pixelChaseActive = false;
      _framePending = true;
      portEXIT_CRITICAL(&_bufferMux);
      DIAG(F("NeoPixel RMT chase cleared VPINs:%u-%u after 60 seconds"),
           (unsigned int)_firstVpin,
           (unsigned int)(_firstVpin + _nPins - 1));
      return;
    }

    if (static_cast<uint32_t>(currentMicros - _pixelChaseLastStepMicros) < 100000UL) {
      portEXIT_CRITICAL(&_bufferMux);
      return;
    }
    _pixelChaseLastStepMicros = currentMicros;
    uint16_t pixel = _pixelChaseIndex;
    RGB colour;
    switch (pixel % 3) {
      case 0: colour = {static_cast<byte>(_pixelChaseBrightness), 0, 0}; break;
      case 1: colour = {0, static_cast<byte>(_pixelChaseBrightness), 0}; break;
      default: colour = {0, 0, static_cast<byte>(_pixelChaseBrightness)}; break;
    }
    _pixels[pixel] = colour;
    _on[pixel] = 1;
    _pixelChaseIndex++;
    if (_pixelChaseIndex == _nPins)
      _pixelChaseHoldStartedMicros = currentMicros;
    _framePending = true;
    portEXIT_CRITICAL(&_bufferMux);

    DIAG(F("NeoPixel RMT chase VPIN:%u RGB:%u,%u,%u"),
         (unsigned int)(_firstVpin + pixel),
         (unsigned int)colour.red, (unsigned int)colour.green,
         (unsigned int)colour.blue);
  }

  int _read(VPIN vpin) override {
    if (_deviceState == DEVSTATE_FAILED || _on == nullptr) return 0;
    size_t pixel = vpin - _firstVpin;
    portENTER_CRITICAL(&_bufferMux);
    int value = _on[pixel];
    portEXIT_CRITICAL(&_bufferMux);
    return value;
  }

  void _write(VPIN vpin, int value) override {
    if (_deviceState == DEVSTATE_FAILED || _on == nullptr) return;
    size_t pixel = vpin - _firstVpin;
    byte on = value ? 1 : 0;
    portENTER_CRITICAL(&_bufferMux);
    bool changed = _on[pixel] != on;
    _on[pixel] = on;
    if (changed) _framePending = true;
    portEXIT_CRITICAL(&_bufferMux);
  }

  void _writeAnalogue(VPIN vpin, int colourRG, uint8_t onoff,
                      uint16_t colourB) override {
    if (_deviceState == DEVSTATE_FAILED || _pixels == nullptr || _on == nullptr) return;
    size_t pixel = vpin - _firstVpin;
    RGB colour = {static_cast<byte>((colourRG >> 8) & 0xFF),
                  static_cast<byte>(colourRG & 0xFF),
                  static_cast<byte>(colourB & 0xFF)};
    portENTER_CRITICAL(&_bufferMux);
    bool changed = _pixels[pixel].red != colour.red ||
                   _pixels[pixel].green != colour.green ||
                   _pixels[pixel].blue != colour.blue ||
                   _on[pixel] != (onoff ? 1 : 0);
    _pixels[pixel] = colour;
    _on[pixel] = onoff ? 1 : 0;
    if (changed) _framePending = true;
    portEXIT_CRITICAL(&_bufferMux);
  }

  void _display() override {
    DIAG(F("NeoPixel RMT GPIO:%d Vpins %u-%u %S"), _dataPin,
         (int)_firstVpin, (int)_firstVpin + _nPins - 1,
         _deviceState == DEVSTATE_FAILED ? F("OFFLINE") : F("ONLINE"));
  }

  static NeoPixelRMT *&instance() {
    static NeoPixelRMT *device = nullptr;
    return device;
  }

  const int _dataPin;
  rmt_channel_t _channel = RMT_CHANNEL_MAX;
  RGB *_pixels = nullptr;
  byte *_on = nullptr;
  RGB *_frame = nullptr;
  byte *_frameOn = nullptr;
  rmt_item32_t *_items = nullptr;
  size_t _itemCount = 0;
  byte _bytesPerPixel = 3;
  byte _redOffset = 1;
  byte _greenOffset = 0;
  byte _blueOffset = 2;
  byte _whiteOffset = 1;
  bool _is800KHz = true;
  bool _framePending = true;
  bool _transmitting = false;
  bool _pixelChaseActive = false;
  uint16_t _pixelChaseIndex = 0;
  uint8_t _pixelChaseBrightness = 255;
  unsigned long _pixelChaseLastStepMicros = 0;
  unsigned long _pixelChaseHoldStartedMicros = 0;
  volatile bool _rmtReady = false;
  volatile bool _rmtFailed = false;
  portMUX_TYPE _bufferMux = portMUX_INITIALIZER_UNLOCKED;
};

#endif // ARDUINO_ARCH_ESP32

#endif // IO_EX_NeoPixelRMT_H