#include "StdioSerialInterface.h"
#include "ProtocolParser.h"
#include "AbstractUpLayer.h"
#include <stdio.h>
#include <string.h>

StdioSerialInterface::StdioSerialInterface() {}

void StdioSerialInterface::listen() {
    char buffer[256];
    while (1) {
        //TODO IO multiplexing
        fgets(buffer, sizeof buffer, stdin);
        if (feof(stdin)) {
            return;
        }

        uplayer->receiveBytes((uint8_t*) buffer, strlen(buffer));
        ProtocolParser::getExistingInstance()->handler();    // TODO remove this dirty hack!!
    }
}

bool StdioSerialInterface::writeBytes(const uint8_t* bytes, uint16_t length) {
    size_t retVal = fwrite(bytes, 1, length, stdout);
    fflush(stdout);
    return (retVal == length);
}
