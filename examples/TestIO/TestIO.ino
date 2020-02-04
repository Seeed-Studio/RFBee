// rfBee selftest
// optimized for least RAM usage
#include "TestIO.h"


#define numPins 5

#define IO_PD_4 4
#define IO_PC_4 18

#define IO_PB_1 9
#define IO_PC_5 19

#define IO_PB_0 8
#define IO_PD_6 6

#define IO_PD_7 7
#define IO_PD_5 5

#define IO_PC_0 14
#define IO_PC_2 16


#define IO_PC_1 15
#define IO_ADC_7 7
#define IO_PC_3 17


int TestIoPins() {
    byte pin[numPins * 2] = {
        IO_PD_4,
        IO_PB_1,
        IO_PB_0,
        IO_PD_7,
        IO_PC_0,

        IO_PC_4,
        IO_PC_5,
        IO_PD_6,
        IO_PD_5,
        IO_PC_2,
    };
    byte i = 0;

    pinMode(IO_PC_1, OUTPUT); //used for lighting the led
    digitalWrite(IO_PC_1, LOW);

    pinMode(IO_PC_3, OUTPUT);
    digitalWrite(IO_PC_3, HIGH);
    if (analogRead(IO_ADC_7) < 500) {
        return ERR;
    }

    for (i = 0; i < numPins; i++) {
        pinMode(pin[i], INPUT);
        pinMode(pin[i + numPins], OUTPUT);
        digitalWrite(pin[i + numPins], HIGH);
    }
    for (i = 0; i < numPins; i++) {
        if (digitalRead(pin[i]) != HIGH) {
            return ERR;
        }
    }

    //if all io is ok, light the led
    digitalWrite(IO_PC_1, HIGH);

    return OK;
}

int TestIO() {
    int result = TestIoPins();
    if (result == OK) {
        Config.set(CONFIG_HW_VERSION, HARDWAREVERSION); // write the hardware version to eeprom
        Serial.println("IO ok");
    } else {
        Serial.println("IO error");
    }
    return result;
}
