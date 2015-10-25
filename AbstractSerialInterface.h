/*
 * AbstractSerialInterface.h
 *
 *  Created on: 2015 j�n. 24
 *      Author: peter.major
 */

#ifndef PROTOCOL_ABSTRACTSERIALINTERFACE_H_
#define PROTOCOL_ABSTRACTSERIALINTERFACE_H_

#include <stdint.h>

class AbstractUpLayer;

class AbstractSerialInterface {
protected:
    AbstractUpLayer* uplayer;
public:
    AbstractSerialInterface();
    virtual ~AbstractSerialInterface() {}
    void setUpLayer(AbstractUpLayer *ul);

    virtual void handler() {};
    virtual void listen() {};
    virtual bool isOpen() {
    	return false;
    }

    // returns true on success
    virtual bool writeBytes(const uint8_t* bytes, uint16_t length) = 0;
    // returns true on success
    virtual bool writeString(const char* str);
};

#endif /* PROTOCOL_ABSTRACTSERIALINTERFACE_H_ */
