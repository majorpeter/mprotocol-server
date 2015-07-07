#include "ItmSerialInterface.h"
#include "AbstractUpLayer.h"
#include <stdio.h>

ItmSerialInterface::ItmSerialInterface() {}

void ItmSerialInterface::listen() {
    char buffer[256];
    while (1) {
        //TODO IO multiplexing
        gets(buffer);
        uplayer->receiveString(buffer);
    }
}

void ItmSerialInterface::writeString(const char* bytes) {
    fputs(bytes, stdout);
}
