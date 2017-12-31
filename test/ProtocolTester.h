/*
 * ProtocolTester.h
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#ifndef MODULES_MPROTOCOL_SERVER_TEST_PROTOCOLTESTER_H_
#define MODULES_MPROTOCOL_SERVER_TEST_PROTOCOLTESTER_H_

#include "ProtocolParser.h"
#include "BufferInterface.h"

class ProtocolTester: private ProtocolParser {
public:
    ProtocolTester();

    void test(const char* input, const char* expectedResult = "");
    void printResults();
private:
    BufferInterface buffer;
    unsigned int testCount;
    unsigned int failCount;

    void printEscaped(const char* s);
};

#endif /* MODULES_MPROTOCOL_SERVER_TEST_PROTOCOLTESTER_H_ */
