/*
 * VcpSerialInterface.cpp
 *
 *  Created on: 2015. okt. 25.
 *      Author: peti
 */

#include "VcpSerialInterface.h"
#include "usbd_cdc_if.h"
#include "AbstractUpLayer.h"
#include <Log/Log.h>

#define TX_BUFFER_SIZE 256

/// CDC device access
extern USBD_HandleTypeDef *hUsbDevice_0;

static int8_t SerialIface_CDC_Receive_FS (uint8_t* Buf, uint32_t *Len);

LOG_TAG(VCP);

VcpSerialInterface::VcpSerialInterface() {
	txBuffer = new uint8_t[TX_BUFFER_SIZE];
	txPosition = 0;
	txOverrunCount = 0;
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
	static const uint16_t PacketSize = 6;	//TODO set 256 after CSW is ready to handle this

	if (txPosition != 0) {
		// first packet start address
		uint8_t* data = txBuffer;
		while(data < &txBuffer[txPosition]) {
			// packet length is number of remaining bytes or max packet size
			uint16_t packetLen = MIN(PacketSize, &txBuffer[txPosition] - data);
			// transmit current packet (!non-blocking call!)
			CDC_Transmit_FS(data, packetLen);

			// get handle to USB CDC device
			USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*) hUsbDevice_0->pClassData;
			// wait for CDC to finish transmission
			while (hcdc->TxState == 1);

			data += packetLen;
		}
		txPosition = 0;
	}

	// create log message that will be sent out on the next call
	if (txOverrunCount > 0) {
		Log::Error(VCP, "TX overrun", txOverrunCount);
		txOverrunCount = 0;
	}
}

bool VcpSerialInterface::writeBytes(const uint8_t* bytes, uint16_t length) {
	if (txPosition + length > TX_BUFFER_SIZE) {
		txOverrunCount += length;
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

	return success ? USBD_OK : USBD_BUSY;
}
