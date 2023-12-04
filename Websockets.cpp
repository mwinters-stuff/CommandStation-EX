#include <Arduino.h>
#include "FSH.h"
#include "RingStream.h"
#include "libsha1.h"
#include "Websockets.h"
#include "DIAG.h"
static const char b64_table[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};

bool Websockets::checkConnectionString(byte clientId,byte * cmd, RingStream * outbound ) {
    // returns true if this input is a websocket connect    
    DIAG(F("In websock check"));   
    /* Heuristic suppose this is a websocket GET
    typically looking like this:
  
    GET / HTTP/1.1
    Host: 192.168.1.242:2560
    Connection: Upgrade
    Pragma: no-cache
    Cache-Control: no-cache
    User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36 Edg/119.0.0.0
    Upgrade: websocket
    Origin: null
    Sec-WebSocket-Version: 13
    Accept-Encoding: gzip, deflate
    Accept-Language: en-US,en;q=0.9
    Sec-WebSocket-Key: SpRkQKPPNZcO62pYf1X6Yg==
    Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits
    */
   
   // check contents to find Sec-WebSocket-Key: and get key up to \n 
   if (strlen((char*)cmd)<200) return false;
   auto keyPos=strstr((char*)cmd,"Sec-WebSocket-Key: ");
   if (!keyPos) return false;
   keyPos+=19; // length of Sec-Websocket-Key: 
   auto endkeypos=strstr(keyPos,"\r");
   if (!endkeypos) return false; 
   *endkeypos=0;
   
   DIAG(F("websock key=\"%s\""),keyPos);
// generate the reply key 
    uint8_t sha1HashBin[21] = { 0 }; // 21 to make it base64 div 3
    char replyKey[100];
    strlcpy(replyKey,keyPos, sizeof(replyKey));
    strlcat(replyKey,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11", sizeof(replyKey));
     
     DIAG(F("websock replykey=%s"),replyKey);

    SHA1_CTX ctx;
    SHA1Init(&ctx);
    SHA1Update(&ctx, (unsigned char *)replyKey, strlen(replyKey));
    SHA1Final(sha1HashBin, &ctx);
      
    // generate the response and embed the base64 encode 
    // of the key
    outbound->mark(clientId);
    outbound->print("HTTP/1.1 101 Switching Protocols\r\n"
                    "Server: DCCEX-WebSocketsServer\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Origin: null\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "Sec-WebSocket-Protocol: DCCEX\r\n"
                    "Sec-WebSocket-Accept: ");
// encode and emit the reply key as base 64
    auto * tmp=sha1HashBin;
    for (int i=0;i<7;i++) { 
      outbound->print(b64_table[(tmp[0] & 0xfc) >> 2]);
      outbound->print(b64_table[((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4)]);
      outbound->print(b64_table[((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6)]);
      if (i<6) outbound->print(b64_table[tmp[2] & 0x3f]);
      tmp+=3;
    }
    outbound->print("=\r\n\r\n");  // because we have padded 1 byte
    outbound->commit();
    return true;          
}

byte * Websockets::unmask(byte clientId,RingStream *ring, byte * buffer) {
 // buffer should have a websocket header
 //byte opcode=buffer[0] & 0x0f;
 DIAG(F("Websock in: %x %x %x %x %x %x %x %x"),
      buffer[0],buffer[1],buffer[2],buffer[3],
       buffer[4],buffer[5],buffer[6]);

 byte opcode=buffer[0];
 if (opcode!=0x81) {
  DIAG(F("unknown opcode "));
  return nullptr;
 }
 bool maskbit=buffer[1]&0x80;
 int16_t payloadLength=buffer[1]&0x7f;
 
 byte * mask;
 if (payloadLength<126) {
     mask=buffer+2;
     }
 else {
     payloadLength=(buffer[3]<<8)|(buffer[2]);
     mask=buffer+4;
 }
 DIAG(F("Websock mb=%b pl=%d m=%x %x %x %x"), maskbit, payloadLength, 
      mask[0],mask[1],mask[2], mask[3]);

 if (payloadLength>100) return nullptr;  // remove this check
 byte * payload=mask+4;
 for (int i=0;i<payloadLength;i++) {
     payload[i]^=mask[i%4];
 }
 
 return payload; // payload will be parsed as normal
     
 }

 int16_t Websockets::getOutboundHeaderSize(uint16_t dataLength) {
   return (dataLength>=126)? 4:2;
 }

 void Websockets::writeOutboundHeader(Print * stream,uint16_t dataLength) {
    // write the outbound header 
    // length patched if necessary.
    // text opcode, flag(126= use 2 length bytes, no mask bit) , length
    if (dataLength>=126) {
      const byte prefix[]={0x81,126,
           (byte)(dataLength & 0xFF), (byte)(dataLength>>8)}; 
      stream->write(prefix,sizeof(prefix));
    }
    else {
      const byte prefix[]={0x81,(byte)dataLength}; 
      stream->write(prefix,sizeof(prefix));
    }
     
 }



