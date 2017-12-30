/*
 * utils.h
 *
 *  Created on: 2017. dec. 29.
 *      Author: peti
 */

#ifndef MODULES_MPROTOCOL_SERVER_UTILS_H_
#define MODULES_MPROTOCOL_SERVER_UTILS_H_

#include <stdint.h>

class AbstractSerialInterface;

namespace ProtocolServerUtils {

int printFloat(char* dest, float f);
uint8_t hexCharToInt(char c);
bool binaryStringToByteArray(uint8_t* to, const char* from, uint16_t maxLength);
void printBinaryDataAsHex(AbstractSerialInterface* serialInterface, const uint8_t *data, uint16_t length);

} /* end namespace ProtocolServerUtils */


#endif /* MODULES_MPROTOCOL_SERVER_UTILS_H_ */
