/*
 * AbstractSerialInterface.h
 *
 *  Created on: 2015 j�n. 24
 *      Author: peter.major
 */

#ifndef PROTOCOL_ABSTRACTSERIALINTERFACE_H_
#define PROTOCOL_ABSTRACTSERIALINTERFACE_H_

class AbstractUpLayer;

class AbstractSerialInterface {
protected:
    AbstractUpLayer* uplayer;
public:
    AbstractSerialInterface() {
        uplayer = 0;
    }
    virtual ~AbstractSerialInterface() {}
    void setUpLayer(AbstractUpLayer *ul) {
        uplayer = ul;
    }

    virtual void listen() {};
    virtual bool isOpen() {
    	return false;
    }
    virtual void writeString(const char* bytes) = 0;
};

#endif /* PROTOCOL_ABSTRACTSERIALINTERFACE_H_ */
