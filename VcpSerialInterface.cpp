/*
 * VcpSerialInterface.cpp
 *
 *  Created on: 2015. okt. 25.
 *      Author: peti
 */

#include "VcpSerialInterface.h"
#include "usbd_cdc_if.h"
#include "AbstractUpLayer.h"

#define TX_BUFFER_SIZE 256

extern USBD_HandleTypeDef *hUsbDevice_0;

static int8_t SerialIface_CDC_Receive_FS (uint8_t* Buf, uint32_t *Len);

VcpSerialInterface::VcpSerialInterface() {
	txBuffer = new char[TX_BUFFER_SIZE];
	txPosition = 0;
}

VcpSerialInterface* VcpSerialInterface::getInstance() {
	static VcpSerialInterface* instance = NULL;
	if (instance == NULL) {
		instance = new VcpSerialInterface();
	}
	return instance;
}

VcpSerialInterface::~VcpSerialInterface() {
	delete[] txBuffer;
}

void VcpSerialInterface::listen() {
	  USBD_Interface_fops_FS.Receive = SerialIface_CDC_Receive_FS;
}

bool VcpSerialInterface::isOpen() {
	return (hUsbDevice_0 != NULL);
}

void VcpSerialInterface::handler() {
	if (txPosition != 0) {
		CDC_Transmit_FS((uint8_t*) txBuffer, txPosition);
		txPosition = 0;
	}
}

bool VcpSerialInterface::writeBytes(const uint8_t* bytes, uint16_t length) {
	if (txPosition + length > TX_BUFFER_SIZE) {
		return false;
	}
	memcpy(txBuffer + txPosition, bytes, length);
	txPosition += length;
	return true;
}

bool VcpSerialInterface::receiveBytes(const uint8_t* bytes, uint16_t len) {
	return (uplayer->receiveBytes(bytes, len));
}

static int8_t SerialIface_CDC_Receive_FS(uint8_t* Buf, uint32_t *Len) {
	USBD_CDC_ReceivePacket(hUsbDevice_0);
	bool success = VcpSerialInterface::getInstance()->receiveBytes(Buf, (uint16_t) *Len);

	fputs("Receive: ", stdout);
	fwrite(Buf, 1, *Len, stdout);
	fputc('\n', stdout);

	return success ? USBD_OK : USBD_BUSY;
}
