/* This is an automation example file.
 *  The presence of a file called "myAutomation.h" brings EX-RAIL code into
 *  the command station.
 *  The automation may have multiple concurrent tasks.
 *  A task may 
 *  - Act as a ROUTE setup macro for a user to drive over 
 *  - drive a loco through an AUTOMATION 
 *  - automate some cosmetic part of the layout without any loco.
 *  
 *  At startup, a single task is created to execute the startup sequence.
 *  This task may simply follow a route, or may START  
 *  further tasks (that is.. send a loco out along a route).
 *  
 *  Where the loco id is not known at compile time, a new task 
 *  can be created with the command:
 *  </ START [cab] route> 
 *  
 *  A ROUTE, AUTOMATION or SEQUENCE are internally identical in ExRail terms  
 *  but are just represented differently to a Withrottle user:
 *  ROUTE(n,"name") - as Route_n .. to setup a route through a layout
 *  AUTOMATION(n,"name") as Auto_n .. to send the current loco off along an automated journey
 *  SEQUENCE(n) is not visible to Withrottle.
 *  
 */

#include "EXRAILMacros.h"

ROSTER(5,"A4 William Whitehall","///F3 Shunting")
ROSTER(6,"DB Schenker Class 08","///F3 Shunting")
ROSTER(7,"Duchess of Montrose",
  "/F1 Background/*F2 Whistle 1/*F3 Whistle 2/*F4 Brake/F5 Cylinder Cock/*F6 Wheel Slip/F7 Coal Shoveling/F8 Injector/F9 Safety Valve/*F10 Guards Whistle/*F11 Coupling/*F12 Carrage Brake Squeal/F13 Flange Squeal/F14 Blower/*F15 Slam Doors/F16 Firemans Breakfast/F17 Blowdown/*F18 Coal Pusher/F19 Water Fill/F20 Chuffing Coasting/////F25 Shunting Mode/F26 Creep Mode/F27 Apply Brake/F28 Auto Function Play"
)

// // This is the startup sequence, 
AUTOSTART
POWERON        // turn on track power
DONE     // This just ends the startup thread, leaving 2 others running.


// points
// "Front Right Crossover Outer"
SERVO_TURNOUT(100, 100, 300, 350, Instant, HIDDEN)
SERVO_TURNOUT(101, 101, 300, 360, Instant, "Front Right Reverse Loop")
SERVO_TURNOUT(102, 102, 290, 320, Instant, "Front Right Loop")
SERVO_TURNOUT(103, 103, 350, 300, Instant, "To Engine Shed")
SERVO_TURNOUT(104, 104, 300, 360, Instant, "To Yard")
// "Front Right Crossover Inner"
SERVO_TURNOUT(105, 105, 260, 310, Instant,  HIDDEN)

// "Front Left Crossover Outer"
SERVO_TURNOUT(120, 120, 375, 300, Instant, HIDDEN) 
SERVO_TURNOUT(121, 121, 360, 300, Instant, "Front Left Loop")
// "Back Left Crossover Outer"
SERVO_TURNOUT(122, 122, 230, 300, Instant, HIDDEN)
SERVO_TURNOUT(123, 123, 230, 300, Instant, "Back Left Reverse Loop")

SERVO_TURNOUT(124, 124, 250, 300, Instant, "Goods or Passenger")
SERVO_TURNOUT(125, 125, 260, 320, Instant, "Passenger Sidings")
SERVO_TURNOUT(126, 126, 250, 300, Instant, "Goods Shed")
SERVO_TURNOUT(127, 127, 250, 300, Instant, "Goods Sidings")

// "Front Left Crossover Inner"
SERVO_TURNOUT(128, 128, 260, 310, Instant,  HIDDEN)
// "Back Left Crossover Inner"
SERVO_TURNOUT(129, 129, 260, 310, Instant,  HIDDEN)

VIRTUAL_TURNOUT(130, "Front Right Crossover")
VIRTUAL_TURNOUT(131, "Front Left Crossover")
VIRTUAL_TURNOUT(132, "Back Left Crossover")

// sensors
JMRI_SENSOR(200, 9)
JMRI_SENSOR(220, 7)

ONTHROW(130)
  THROW(100)
  THROW(105)
DONE

ONCLOSE(130)
  CLOSE(100)
  CLOSE(105)
DONE

ONTHROW(131)
  THROW(120)
  THROW(128)
DONE

ONCLOSE(131)
  CLOSE(120)
  CLOSE(128)
DONE

ONTHROW(132)
  THROW(122)
  THROW(129)
DONE

ONCLOSE(132)
  CLOSE(122)
  CLOSE(129)
DONE

// /* SEQUENCE(1) is a simple shuttle between 2 sensors      
//  *  S20 and S21 are sensors on arduino pins 20 and 21 
//  *  S20                    S21                   
//  *  === START->================
//  */
//    SEQUENCE(1) 
//      DELAY(10000)   // wait 10 seconds
//      FON(3)       // Set Loco Function 3, Horn on
//      DELAY(1000)    // wait 1 second
//      FOFF(3)      // Horn off
//      FWD(80)      // Move forward at speed 80
//      AT(21)       // until we hit sensor id 21
//      STOP         // then stop
//      DELAY(5000)    // Wait 5 seconds
//      FON(2)       // ring bell
//      REV(60)      // reverse at speed 60
//      AT(20)       // until we get to S20
//      STOP         // then stop
//      FOFF(2)      // Bell off 
//      FOLLOW(1)    // and follow sequence 1 again
   
// /* SEQUENCE(2) is an automation example for a single loco Y shaped journey
//  *  S31,S32,S33 are sensors, T4 is a turnout
//  *  
//  *  S33                      T4                            S31
//  *  ===-START->=============================================
//  *                          //
//  *  S32                    //
//  *  ======================//
//  *  
//  *  Train runs from START to S31, back to S32, again to S31, Back to start.
//  */
//   SEQUENCE(2)
//    FWD(60)     // go forward at DCC speed 60 
//    AT(31) STOP  // when we get to sensor 31 
//    DELAY(10000)  // wait 10 seconds 
//    THROW(4)    // throw turnout for route to S32
//    REV(45)     // go backwards at speed 45
//    AT(32) STOP  // until we arrive at sensor 32
//    DELAY(5000)   // wait 5 seconds
//    FWD(50)     // go forwards at speed 50
//    AT(31) STOP  // and stop at sensor 31
//    DELAY(5000)   // wait 5 seconds 
//    CLOSE(4)    // set turnout closed
//    REV(50)     // reverse back to S3
//    AT(33) STOP
//    DELAY(20000)  // wait 20 seconds 
//    FOLLOW(2)   // follow sequence 2... ie repeat the process
