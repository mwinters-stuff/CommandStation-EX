#ifndef Websockets_h
#define Websockets_h
#include <Arduino.h>
#include "RingStream.h"
class Websockets {
    public:
    static bool checkConnectionString(byte clientId,byte * cmd, RingStream * outbound );
    static byte * unmask(byte clientId,RingStream *ring, byte * buffer);
    static int16_t getOutboundHeaderSize(uint16_t dataLength);
    static void writeOutboundHeader(Print * stream,uint16_t dataLength);
    static const byte WEBSOCK_CLIENT_MARKER=0x80;
};

#endif