//  Firmware for rfBee
//  see www.seeedstudio.com for details and ordering rfBee hardware.

//  Copyright (c) 2010 Hans Klunder <hans.klunder (at) bigfoot.com>
//  Author: Hans Klunder, based on the original Rfbee v1.0 firmware by Seeedstudio
//  Version: July 14, 2010
//
//  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
//  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//  See the GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along with this program;
//  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA



#define FIRMWAREVERSION 11 // 1.1  , version number needs to fit in byte (0~255) to be able to store it into config
//#define FACTORY_SELFTEST
//#define DEBUG


#include "debug.h"
#include "globals.h"
#include "Config.h"
#include "CCx.h"
#include "rfBeeSerial.h"

#ifdef FACTORY_SELFTEST
    #include "TestIO.h"  // factory selftest
#endif

#define GDO0 2 // used for polling the RF received data


void setup() {
    if (Config.initialized() != OK) {
        Serial.begin(9600);
        Serial.println("Initializing config");
        #ifdef FACTORY_SELFTEST
        if (TestIO() != OK) {
            return;
        }
        #endif
        Config.reset();
    }
    Config.load_default();
    setUartBaudRate();
    rfBeeInit();
    Serial.println("ok");
}

void loop() {

    byte mode = Config.get(CONFIG_RFBEE_MODE);

    if (Serial.available() > 0) {
        sleepCounter = 1000; // reset the sleep counter
        if (serialMode == SERIALCMDMODE) {
            readSerialCmd();
            return;
        } else {
            txDataFromSerialToRf();
        }
    }

    if (mode != TRANSMIT_MODE) {
        if (digitalRead(GDO0) == HIGH) {
            rxDataFromRfToSerial();
            sleepCounter++; // delay sleep
        } else if (millis() - CCx.lastCalibrate > 60000) { //re-calibrate cc when its idle after 1min
            CCx.Strobe(CCx_SIDLE);
            CCx.Strobe(CCx_SRX);
            CCx.lastCalibrate = millis();
        }
    }

    sleepCounter--;

    // check if we can go to sleep again, going into low power too early will result in lost data in the CCx fifo.
    if ((sleepCounter == 0) && (mode == LOWPOWER_MODE)) {
        DEBUGPRINT("low power on")
        lowPowerOn();
        DEBUGPRINT("woke up")
    }
}


void rfBeeInit() {
    DEBUGPRINT()

    CCx.PowerOnStartUp();
    setCCxConfig();

    serialMode = SERIALDATAMODE;
    sleepCounter = 0;

    attachInterrupt(0, ISRVreceiveData, RISING);  //GD00 is located on pin 2, which results in INT 0

    pinMode(GDO0, INPUT); // used for polling the RF received data

}

// handle interrupt
void ISRVreceiveData() {
    //DEBUGPRINT()
    sleepCounter = 10;
}


