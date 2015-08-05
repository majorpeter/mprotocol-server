#include <Protocol/StdioSerialInterface.h>
#include "AbstractUpLayer.h"
#include <stdio.h>

StdioSerialInterface::StdioSerialInterface() {}

void StdioSerialInterface::listen() {
    char buffer[256];
    while (1) {
        //TODO IO multiplexing
        fgets(buffer, sizeof buffer, stdin);
        if (feof(stdin)) {
        	return;
        }
        uplayer->receiveString(buffer);
    }
}

void StdioSerialInterface::writeString(const char* bytes) {
    fputs(bytes, stdout);
    fflush(stdout);
}
