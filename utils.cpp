/*
 * utils.cpp
 *
 *  Created on: 2017. dec. 29.
 *      Author: peti
 */

#include "utils.h"
#include "AbstractSerialInterface.h"
#include "mprotocol-nodes/Node.h"
#include "mprotocol-nodes/RootNode.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

namespace ProtocolServerUtils {

/**
 * this routine prints the float number with 4 frac. digits precision
 * in case printf("%f") is limited in the current libc
 * @return length of printed string
 */
int printFloat(char* dest, float f) {
    const char* const dest_orig = dest;

    if (isnanf(f)) {
        strcpy(dest, "nan");
        return 3;
    }

    // print '-' if negative
    if (f < 0.f) {
        *dest = '-';
        dest++;
        f = -f;
    }

    // print integer part and substract it
    dest += sprintf(dest, "%0d", (int) f);
    f -= (float) (int) f;

    // print decimal point if the fractional part is big enough
    if (f > 0.0001f) {
        *dest = '.';
        dest++;
    }

    // print and substract the first 4 fractional digits
    uint8_t fracDigits = 0;
    while (f > 0.0001f) {
        // get the next digit left from the decimal point
        f *= 10;
        // this is a lot faster than sprintf for 1 digit
        *dest = (char) ('0' + (int) f);
        // clear integer part
        f -= (float) (int) f;

        dest++;
        fracDigits++;
        if (fracDigits == 4) {
            break;
        }
    }
    // always close the string
    *dest = '\0';

    return dest - dest_orig;
}

uint8_t hexCharToInt(char c) {
    if (c < '0') {
        return 0xff;
    }

    if (c <= '9') {
        return c - '0';
    }

    if (c < 'A') {
        return 0xff;
    }
    if (c <= 'F') {
        return c - 'A' + 0x0a;
    }

    if (c < 'a') {
        return 0xff;
    }
    if (c <= 'f') {
        return c - 'a' + 0x0a;
    }

    return 0xff;
}

/**
 * parses hex string
 * @return true if the string was valid
 */
bool binaryStringToByteArray(uint8_t* to, const char* from, uint16_t maxLength) {
    while ((from[0] != '\0') && (maxLength > 0)) {
        uint8_t val0 = hexCharToInt(from[0]);
        uint8_t val1 = hexCharToInt(from[1]);
        if ((val0 == 0xff) || (val1 == 0xff)) {
            return false;
        }
        *to = (val0 << 4) | val1;
        from += 2;
        to++;
        maxLength--;
    }
    
    return true;
}

void printBinaryDataAsHex(AbstractSerialInterface* serialInterface, const uint8_t *data, uint16_t length) {
    while (length > 0) {
        char buf[3];
        sprintf(buf, "%02X", *data);
        serialInterface->writeBytes((uint8_t*) buf, 2);
        data++;
        length--;
    }
}

void printNodePathRecursively(AbstractSerialInterface* serialInterface, const Node* node) {
    const Node* parent = node->getParent();
    if (parent != NULL) {
        printNodePathRecursively(serialInterface, parent);
        if (parent != RootNode::getInstance()) {
            serialInterface->writeBytes((uint8_t*) '/', 1);
        }
    }

    if (node->getName() != NULL) {
        serialInterface->writeString(node->getName());
    }
}

} /* end namespace ProtocolServerUtils */
