/*
 * VcpSerialInterface.h
 *
 *  Created on: 2015. okt. 25.
 *      Author: peti
 */

#ifndef VCPSERIALINTERFACE_H_
#define VCPSERIALINTERFACE_H_

#include "AbstractSerialInterface.h"

class VcpSerialInterface: public AbstractSerialInterface {
private:
	uint8_t *txBuffer;
	uint16_t txPosition;
	uint16_t txOverrunCount;
	VcpSerialInterface();
public:
	static VcpSerialInterface* getInstance();
	virtual ~VcpSerialInterface();

	virtual void listen();
	virtual bool isOpen();
	virtual void handler();
	virtual bool writeBytes(const uint8_t* bytes, uint16_t length);
	bool receiveBytes(const uint8_t* bytes, uint16_t len);
};

#endif /* VCPSERIALINTERFACE_H_ */
