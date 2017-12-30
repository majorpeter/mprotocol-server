/*
 * utils.h
 *
 *  Created on: 2017. dec. 29.
 *      Author: peti
 */

#ifndef MODULES_MPROTOCOL_SERVER_UTILS_H_
#define MODULES_MPROTOCOL_SERVER_UTILS_H_

#include <stdint.h>

namespace ProtocolServerUtils {

int printFloat(char* dest, float f);
uint8_t charToByte(char c);
bool binaryStringToArray(const char* from, uint8_t* to);    //TODO max length

} /* end namespace ProtocolServerUtils */


#endif /* MODULES_MPROTOCOL_SERVER_UTILS_H_ */
