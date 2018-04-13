#ifndef TESTIO_H
#define TESTIO_H

#include "globals.h"
#include "Config.h"

#define HARDWAREVERSION 11  // 1.1 , version number needs to fit in byte (0~255) to be able to store it into config, this should only be changed by the manufacturer!

int TestIO();

#endif

