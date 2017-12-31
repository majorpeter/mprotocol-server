/*
 * test.cpp
 *
 *  Created on: 2017. dec. 30.
 *      Author: peti
 */

#include "ProtocolParser.h"

#include <stdio.h>
#include <string.h>

#include <iostream>

class BufferInterface: public AbstractSerialInterface {
public:
    BufferInterface() {
        writeIndex = 0;
    }

    virtual bool writeBytes(const uint8_t* bytes, uint16_t length) {
        if (writeIndex + length > bufferLength) {
            return false;
        }

        memcpy(buffer + writeIndex, bytes, length);
        writeIndex += length;
        buffer[writeIndex] = '\0';
        return true;
    }

    const char* getBuffer() const {
        return buffer;
    }

    void clear() {
        writeIndex = 0;
        buffer[0] = '\0';
    }
private:
    static const uint16_t bufferLength = 1024;
    uint16_t writeIndex;
    char buffer[bufferLength];
};

class ProtocolTester: private ProtocolParser {
public:
    ProtocolTester(): ProtocolParser(&buffer) {
    }

    void test(const char* input, const char* expectedResult) {
        buffer.clear();
        this->receiveBytes((const uint8_t*) input, strlen(input));
        this->handler();

        if (strcmp(buffer.getBuffer(), expectedResult) != 0) {
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
private:
    BufferInterface buffer;

    void printEscaped(const char* s) {
        while (*s != '\0') {
            if ((('0' <= *s) && (*s <= '9')) ||
                    (('a' <= *s) && (*s <= 'z')) ||
                    (('A' <= *s) && (*s <= 'Z')) ||
                    (*s == '/') || (*s == '{') || (*s == '}') || (*s == '.')) {
                fwrite(s, 1, 1, stdout);
            } else {
                printf("<0x%02x>", *s);
            }
            s++;
        }
    }
};

int main() {
    ProtocolTester tester;

    tester.test("GET /\r\n", "{\n}\n");
    std::cout << "Done." << std::endl;

    return 0;
}
