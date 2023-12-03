#ifndef Websockets_h
#define Websockets_h
#include <Arduino.h>
#include "RingStream.h"
class Websockets {
    public:
    static bool checkConnectionString(byte clientId,byte * cmd, RingStream * outbound );
};

#endif