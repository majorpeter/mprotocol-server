/*
 * utils.cpp
 *
 *  Created on: 2017. dec. 29.
 *      Author: peti
 */

#include "utils.h"

#include <stdio.h>
#include <stdint.h>

namespace ProtocolServerUtils {

/**
 * this routine prints the float number with 4 frac. digits precision
 * in case printf("%f") is limited in the current libc
 * @return length of printed string
 */
int printFloat(char* dest, float f) {
    const char* const dest_orig = dest;

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

} /* end namespace ProtocolServerUtils */
