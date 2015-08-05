#ifndef PROTOCOL_STDIOSERIALINTERFACE_H_
#define PROTOCOL_STDIOSERIALINTERFACE_H_

#include "AbstractSerialInterface.h"
#include <stdint.h>

/**
 * this serial interface uses STDIN & STDOUT
 * @note this means semihosting with retarget in the STM
 */

class StdioSerialInterface: public AbstractSerialInterface {
public:
    StdioSerialInterface();
    virtual ~StdioSerialInterface() {}

    virtual void listen();
    virtual void writeString(const char* bytes);
};

#endif /* PROTOCOL_STDIOSERIALINTERFACE_H_ */
