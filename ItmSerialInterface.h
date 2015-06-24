/*
 * ItmSerialInterface.h
 *
 *  Created on: 2015 jún. 24
 *      Author: peter.major
 */

#ifndef PROTOCOL_ITMSERIALINTERFACE_H_
#define PROTOCOL_ITMSERIALINTERFACE_H_

#include "AbstractSerialInterface.h"
#include <stdint.h>

class ItmSerialInterface: public AbstractSerialInterface {
public:
    ItmSerialInterface();
    virtual ~ItmSerialInterface() {}

    virtual void listen();
    virtual void writeString(const char* bytes);
};

#endif /* PROTOCOL_ITMSERIALINTERFACE_H_ */
