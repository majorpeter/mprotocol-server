/*
 * BufferInterface.h
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#ifndef MODULES_MPROTOCOL_SERVER_TEST_BUFFERINTERFACE_H_
#define MODULES_MPROTOCOL_SERVER_TEST_BUFFERINTERFACE_H_

#include "AbstractSerialInterface.h"

class BufferInterface: public AbstractSerialInterface {
public:
    BufferInterface();

    virtual bool writeBytes(const uint8_t* bytes, uint16_t length);

    const char* getBuffer() const {
        return buffer;
    }

    void clear();
private:
    static const uint16_t bufferLength = 1024;
    uint16_t writeIndex;
    char buffer[bufferLength];
};

#endif /* MODULES_MPROTOCOL_SERVER_TEST_BUFFERINTERFACE_H_ */
