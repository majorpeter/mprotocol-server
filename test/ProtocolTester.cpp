/*
 * ProtocolTester.cpp
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#include "ProtocolTester.h"
#include <string.h>
#include <iostream>

ProtocolTester::ProtocolTester(): ProtocolParser(&buffer) {
    testCount = failCount = 0;
}

void ProtocolTester::test(const char* input, const char* expectedResult) {
    testCount++;

    buffer.clear();
    this->receiveBytes((const uint8_t*) input, strlen(input));
    this->handler();

    if (strcmp(buffer.getBuffer(), expectedResult) != 0) {
        failCount++;

        std::cout << "Test failed!" << std::endl;
        std::cout << "\tInput: ";
        printEscaped(input);
        std::cout << std::endl;
        std::cout << "\tActual result: ";
        printEscaped(buffer.getBuffer());
        std::cout << std::endl;
        std::cout << "\tExpected result: ";
        printEscaped(expectedResult);
        std::cout << std::endl;
    }
}

void ProtocolTester::printResults() {
    std::cout << std::endl;
    if (failCount != 0) {
        std::cout << failCount << " tests failed." << std::endl;
    }
    std::cout << (testCount - failCount) << '/' << testCount << " tests passed." << std::endl << std::endl;
}

void ProtocolTester::printEscaped(const char* s) {
    while (*s != '\0') {
        if ((('0' <= *s) && (*s <= '9')) ||
            (('a' <= *s) && (*s <= 'z')) ||
            (('A' <= *s) && (*s <= 'Z'))) {
            fwrite(s, 1, 1, stdout);
        } else {
            switch (*s) {
            case '/':
            case '.':
            case '{':
            case '}':
            case '_':
            case '=':
            case ' ':
                fwrite(s, 1, 1, stdout);
                break;
            case '\r':
                fwrite("\\r", 1, 2, stdout);
                break;
            case '\n':
                fwrite("\\n", 1, 2, stdout);
                break;
            default:
                printf("<0x%02x>", *s);
                break;
            }
        }
        s++;
    }
}
