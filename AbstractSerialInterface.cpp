/*
 * AbstractSerialInterface.cpp
 *
 *  Created on: 2015. okt. 25.
 *      Author: peti
 */

#include "AbstractSerialInterface.h"
#include <string.h>

AbstractSerialInterface::AbstractSerialInterface() {
    uplayer = 0;
}

void AbstractSerialInterface::setUpLayer(AbstractUpLayer *ul) {
    uplayer = ul;
}

bool AbstractSerialInterface::writeString(const char *str) {
	size_t length = strlen(str);
	return this->writeBytes((uint8_t*) str, length);
}
