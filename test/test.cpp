/*
 * test.cpp
 *
 *  Created on: 2017. dec. 30.
 *      Author: peti
 */

#include "ProtocolParser.h"
#include "mprotocol-nodes/Node.h"
#include "mprotocol-nodes/RootNode.h"

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

class TestNode: public Node {
public:
    TestNode(): Node("TEST", "Test node") {}
};

class ProtocolTester: private ProtocolParser {
public:
    ProtocolTester(): ProtocolParser(&buffer) {
        testCount = failCount = 0;
    }

    void test(const char* input, const char* expectedResult = "") {
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

    void printResults() {
        std::cout << std::endl;
        if (failCount != 0) {
            std::cout << failCount << " tests failed." << std::endl;
        }
        std::cout << (testCount - failCount) << '/' << testCount << " tests passed." << std::endl << std::endl;
    }
private:
    BufferInterface buffer;
    unsigned int testCount;
    unsigned int failCount;

    void printEscaped(const char* s) {
        while (*s != '\0') {
            if ((('0' <= *s) && (*s <= '9')) ||
                    (('a' <= *s) && (*s <= 'z')) ||
                    (('A' <= *s) && (*s <= 'Z')) ||
                    (*s == '/') || (*s == '{') || (*s == '}') || (*s == '.') || (*s == ' ')) {
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
    RootNode::getInstance()->addChild(new TestNode());

    tester.test("GET /\r\n", "{\nN TEST\n}\n");
    tester.test("GET /TEST", "{\n}\n");

    tester.printResults();

    return 0;
}