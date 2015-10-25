#include "ItmSerialInterface.h"
#include "AbstractUpLayer.h"
#include <stdio.h>
#include <string.h>

ItmSerialInterface::ItmSerialInterface() {}

void ItmSerialInterface::listen() {
    char buffer[256];
    while (1) {
        //ITM cannot do IO multiplexing, so this endless loop is the only way it works...
        gets(buffer);
        uplayer->receiveBytes((uint8_t*) buffer, (uint16_t) strlen(buffer));
    }
}

bool ItmSerialInterface::writeString(const char* bytes) {
    fputs(bytes, stdout);
    return true;
}
