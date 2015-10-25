#ifndef ABSTRACTUPLAYER_H_
#define ABSTRACTUPLAYER_H_

#include <stdint.h>

class AbstractUpLayer {
public:
    AbstractUpLayer() {}
    virtual ~AbstractUpLayer() {}
    virtual bool receiveBytes(const uint8_t* bytes, uint16_t len) = 0;
};

#endif /* ABSTRACTUPLAYER_H_ */
