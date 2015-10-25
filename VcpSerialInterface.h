/*
 * VcpSerialInterface.h
 *
 *  Created on: 2015. okt. 25.
 *      Author: peti
 */

#ifndef VCPSERIALINTERFACE_H_
#define VCPSERIALINTERFACE_H_

#include "AbstractSerialInterface.h"
#include <stdint.h>

class VcpSerialInterface: public AbstractSerialInterface {
	char *txBuffer;
	uint16_t txPosition;
public:
	VcpSerialInterface();
	virtual ~VcpSerialInterface();

	virtual void listen();
	virtual bool isOpen();
	virtual void handler();
	virtual void writeString(const char* bytes);
	void receiveBytes(const uint8_t* bytes, uint16_t len);
};

#endif /* VCPSERIALINTERFACE_H_ */
