#pragma once
#ifndef UNUSED_PIN     // sync define with the one in MotorDrivers.h
#define UNUSED_PIN 255 // inside uint8_t
#endif
class Pinpair {
public:
  Pinpair(byte p1, byte p2) {
    pin = p1;
    invpin = p2;
  };
  byte pin = UNUSED_PIN;
  byte invpin = UNUSED_PIN;
};

