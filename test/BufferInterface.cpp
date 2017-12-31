/*
 * BufferInterface.cpp
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#include "BufferInterface.h"
#include <string.h>

BufferInterface::BufferInterface() {
    writeIndex = 0;
}

bool BufferInterface::writeBytes(const uint8_t* bytes, uint16_t length) {
    if (writeIndex + length > bufferLength) {
        return false;
    }

    memcpy(buffer + writeIndex, bytes, length);
    writeIndex += length;
    buffer[writeIndex] = '\0';
    return true;
}

void BufferInterface::clear() {
    writeIndex = 0;
    buffer[0] = '\0';
}
