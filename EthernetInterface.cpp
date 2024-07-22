/*
 *  © 2024 Morten "Doc" Nielsen
 *  © 2023-2024 Paul M. Antoine
 *  © 2022 Bruno Sanches
 *  © 2021 Fred Decker
 *  © 2020-2022 Harald Barth
 *  © 2020-2021 Chris Harlow
 *  © 2020 Gregor Baues
 *  All rights reserved.
 *  
 *  This file is part of DCC-EX/CommandStation-EX
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
 * 
 */
#include "defines.h" 
#if ETHERNET_ON == true
#include "EthernetInterface.h"
#include "DIAG.h"
#include "CommandDistributor.h"
#include "WiThrottle.h"
#include "DCCTimer.h"
#include "MDNS_Generic.h"

EthernetUDP udp;
MDNS mdns(udp);

EthernetInterface * EthernetInterface::singleton=NULL;
/**
 * @brief Setup Ethernet Connection
 * 
 */
void EthernetInterface::setup()
{
  if (singleton!=NULL) {
    DIAG(F("Prog Error!"));
    return;
  }
  DIAG(F("Ethernet starting... please be patient, especially if no cable is connected!"));
  if ((singleton=new EthernetInterface())) {
    // DIAG(F("Ethernet Class initialized"));
    return;
  }
  DIAG(F("Ethernet not initialized"));
};


#ifdef IP_ADDRESS
static IPAddress myIP(IP_ADDRESS);
#endif

/**
 * @brief Aquire IP Address from DHCP and start server
 * 
 * @return true 
 * @return false 
 */
EthernetInterface::EthernetInterface()
{
    connected=false;
#if defined(STM32_ETHERNET)
    // Set a HOSTNAME for the DHCP request - a nice to have, but hard it seems on LWIP for STM32
    // The default is "lwip", which is **always** set in STM32Ethernet/src/utility/ethernetif.cpp
    // for some reason. One can edit it to instead read:
    //      #if LWIP_NETIF_HOSTNAME
    //      /* Initialize interface hostname */
    //      if (netif->hostname == NULL)
    //         netif->hostname = "lwip";
    //      #endif /* LWIP_NETIF_HOSTNAME */
    // Which seems more useful! We should propose the patch... so the following line actually works!
    netif_set_hostname(&gnetif, WIFI_HOSTNAME);   // Should probably be passed in the contructor...
  #ifdef IP_ADDRESS
    Ethernet.begin(myIP);
  #else
    Ethernet.begin();
  #endif // IP_ADDRESS
#else // All other architectures
    byte mac[6]= { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
    DIAG(F("Ethernet attempting to get MAC address"));
    DCCTimer::getSimulatedMacAddress(mac);
    DIAG(F("Ethernet got MAC address"));
  #ifdef IP_ADDRESS
    Ethernet.begin(mac, myIP);
  #else
    if (Ethernet.begin(mac) == 0)
    {
        DIAG(F("Ethernet.begin FAILED"));
        return;
    }
  #endif // IP_ADDRESS
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      DIAG(F("Ethernet shield not found or W5100"));
    }
#endif // STM32_ETHERNET

    uint32_t startmilli = millis();
    while ((millis() - startmilli) < 5500) { // Loop to give time to check for cable connection
        if (Ethernet.linkStatus() == LinkON)
            break;
        DIAG(F("Ethernet cable connected? Waiting for link (1sec) "));
        delay(1000);
    }
    // Now we either do have link or we have a W5100 where we do not know if we have link.
    // So now run checkLink() which also sets up outboundRing if it does not exist.
    // A false returned means we know we booted without an Ethernet cable, or perhaps there
    // is no W5100 and we should say so
    if (!checkLink())
    {
      #if defined(STM32_ETHERNET)
      DIAG(F("Ethernet cable disconnected!"));
      #else
      DIAG(F("Ethernet cable disconnected, or W5100 hardware not present"));
      #endif
      LCD(4,F("Ethernet DOWN"));
    }
}

/**
 * @brief Cleanup any resources
 * 
 * @return none
 */
EthernetInterface::~EthernetInterface() {
  delete server;
  delete outboundRing;
}

/**
 * @brief Main loop for the EthernetInterface
 * 
 */
void EthernetInterface::loop()
{
    if (!singleton || (!singleton->checkLink()))
      return;
    
    switch (Ethernet.maintain()) {
    case 1:
        //renewed fail
        DIAG(F("Ethernet Error: renewed fail"));
        singleton=NULL;
        return;
    case 3:
        //rebind fail
        DIAG(F("Ethernet Error: rebind fail"));
        singleton=NULL;
        return;
    default:
        //nothing happened
        break;
    }
   singleton->loop2();
}

/**
 * @brief Checks ethernet link cable status and detects when it connects / disconnects
 * 
 * @return true when cable is connected, false otherwise
 */
bool EthernetInterface::checkLink() {
  if (Ethernet.linkStatus() != LinkOFF) { // check for not linkOFF instead of linkON as the W5100 does return LinkUnknown
    //if we are not connected yet, setup a new server
    if(!connected) {
      DIAG(F("Ethernet cable connected"));
      connected=true;
      #ifdef IP_ADDRESS
      #ifndef STM32_ETHERNET
      Ethernet.setLocalIP(myIP);      // for static IP, set it again
      #endif
      #endif
      #if defined (STM32_ETHERNET)
      netif_set_hostname(&gnetif, WIFI_HOSTNAME);   // Should probably be passed in the contructor...
      #ifdef IP_ADDRESS
      Ethernet.begin(myIP);
      #else
      Ethernet.begin();
      #endif // IP_ADDRESS
      #endif // STM32_ETHERNET

      server = new EthernetServer(IP_PORT); // Ethernet Server listening on default port IP_PORT
      server->begin();
      #ifndef IP_ADDRESS
      LCD(4,F("Awaiting DHCP..."));
      IPAddress ip = Ethernet.localIP(); // look what IP was obtained (dynamic or static)
      if (ip[0] == 0) {
        DIAG(F("Awaiting DHCP... ip was %d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
      }
      while (ip[0] == 0) {        // wait until we are given an IP address from the DHCP server
        DIAG(F("Awaiting DHCP... ip was %d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
        ip = Ethernet.localIP(); // look what IP was obtained (dynamic or static)
      }
      #else
      IPAddress ip = Ethernet.localIP(); // look what IP was obtained (dynamic or static)
      #endif
      if (MAX_MSG_SIZE < 20) {
        LCD(4,F("%d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
        LCD(5,F("Port:%d  Eth"), IP_PORT);
      } else {
        LCD(4,F("%d.%d.%d.%d:%d"), ip[0], ip[1], ip[2], ip[3], IP_PORT);
      }
      mdns.begin(Ethernet.localIP(), WIFI_HOSTNAME); // hostname
      mdns.addServiceRecord(WIFI_HOSTNAME "._withrottle", IP_PORT, MDNSServiceTCP);
      // Not sure if we need to run it once, but just in case!
      mdns.run();
      // only create a outboundRing it none exists, this may happen if the cable
      // gets disconnected and connected again
      if(!outboundRing)
      	outboundRing=new RingStream(OUTBOUND_RING_SIZE);
      // Clear out the clients
      for (byte socket = 0; socket < MAX_SOCK_NUM; socket++) {
        clients[socket].inUse = false;
      }
    }
    return true;
  } else { // LinkOFF
    if (connected) {  // Were connected, but no longer without a LINK!
      DIAG(F("Ethernet cable disconnected"));
      connected=false;
      //clean up any clients
      for (byte socket = 0; socket < MAX_SOCK_NUM; socket++) {
        if (clients[socket].inUse && clients[socket].client.connected())
        {
          clients[socket].client.flush();
          clients[socket].client.stop();
        }
      }
      mdns.removeServiceRecord(IP_PORT, MDNSServiceTCP);
      // tear down server
      delete server;
      server = nullptr;
      LCD(4,F("Ethernet DOWN"));
    }
  }
  return false;
}

void EthernetInterface::loop2() {
  if (!outboundRing) { // no idea to call loop2() if we can't handle outgoing data in it
    if (Diag::ETHERNET) DIAG(F("No outboundRing"));
    return;
  }

  // Always do this because we don't want traffic to intefere with being found!
  mdns.run();

    // get client from the server
  #if defined (STM32_ETHERNET)
    // STM32Ethernet doesn't use accept(), just available()
    EthernetClient client = server->available();
  #else
    EthernetClient client = server->accept();
  #endif
    // check for new client
    if (client)
    {
      byte socket;
      bool sockfound = false;
      for (socket = 0; socket < MAX_SOCK_NUM; socket++)
      {
        if (clients[socket].inUse && (client == clients[socket].client))
        {
          sockfound = true;
          if (Diag::ETHERNET)
            DIAG(F("Ethernet: Old client socket %d"), socket);
          break;
        }
      }
      if (!sockfound)
      { // new client
        for (socket = 0; socket < MAX_SOCK_NUM; socket++)
        {
          if (!clients[socket].inUse)
          {
            // On accept() the EthernetServer doesn't track the client anymore
            // so we store it in our client array
            clients[socket].client = client;
            clients[socket].inUse = true;
            if (Diag::ETHERNET)
              DIAG(F("Ethernet: New client socket %d"), socket);
            break;
          }
        }
      }
      if (socket == MAX_SOCK_NUM)
        DIAG(F("new Ethernet OVERFLOW"));
    }

    // check for incoming data from all possible clients
    for (byte socket = 0; socket < MAX_SOCK_NUM; socket++)
    {
      if (clients[socket].inUse)
      {
        if (!clients[socket].client.connected())
        { // stop any clients which disconnect
          CommandDistributor::forget(socket);
          clients[socket].client.flush();
          clients[socket].client.stop();
          clients[socket].inUse = false;
          if (Diag::ETHERNET)
            DIAG(F("Ethernet: disconnect %d "), socket);
          return; // Trick: So that we do not continue in this loop with client that is NULL
        }

        int available = clients[socket].client.available();
        if (available > 0)
        {
          if (Diag::ETHERNET)
            DIAG(F("Ethernet: available socket=%d,avail=%d"), socket, available);
          // read bytes from a client
          int count = clients[socket].client.read(buffer, MAX_ETH_BUFFER);
          buffer[count] = '\0'; // terminate the string properly
          if (Diag::ETHERNET)
            DIAG(F(",count=%d:%e"), socket, buffer);
          // execute with data going directly back
          CommandDistributor::parse(socket, buffer, outboundRing);
          return; // limit the amount of processing that takes place within 1 loop() cycle.
        }
      }
    }

    WiThrottle::loop(outboundRing);
    
    // handle at most 1 outbound transmission 
    int socketOut=outboundRing->read();
    if (socketOut >= MAX_SOCK_NUM) {
      DIAG(F("Ethernet outboundRing socket=%d error"), socketOut);
    } else if (socketOut >= 0) {
      int count=outboundRing->count();
      if (Diag::ETHERNET)
        DIAG(F("Ethernet reply socket=%d, count=:%d"), socketOut,count);
      for(;count>0;count--)  clients[socketOut].client.write(outboundRing->read());
      clients[socketOut].client.flush(); //maybe 
    }
}
#endif
