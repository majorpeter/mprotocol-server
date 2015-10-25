/*
 * ItmSerialInterface.h
 *
 *  Created on: 2015 j�n. 24
 *      Author: peter.major
 */

#ifndef PROTOCOL_ITMSERIALINTERFACE_H_
#define PROTOCOL_ITMSERIALINTERFACE_H_

#include "AbstractSerialInterface.h"

class ItmSerialInterface: public AbstractSerialInterface {
public:
    ItmSerialInterface();
    virtual ~ItmSerialInterface() {}

    virtual void listen();
    //TODO implement isOpen();
    virtual bool writeString(const char* bytes);
};

#endif /* PROTOCOL_ITMSERIALINTERFACE_H_ */
