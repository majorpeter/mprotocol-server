#include <Protocol/StdioSerialInterface.h>
#include <Protocol/ProtocolParser.h>
#include <Log/Log.h>
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

        uplayer->receiveBytes((uint8_t*) buffer, strlen(buffer));
        ProtocolParser::getExistingInstance()->handler();	// TODO remove this dirty hack!!
        ProtocolParser::getExistingInstance()->handleSubscriptions();	//TODO this too. :)
        Log::getInstance()->handler();                      //TODO pretty much the same deal
    }
}

void StdioSerialInterface::writeString(const char* bytes) {
    fputs(bytes, stdout);
    fflush(stdout);
}
