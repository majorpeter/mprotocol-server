#ifndef PROTOCOLPARSER_H_
#define PROTOCOLPARSER_H_

#include "AbstractSerialInterface.h"
#include "AbstractUpLayer.h"

typedef enum {
    ProtocolResult_Ok,
    ProtocolResult_UnknownFunc,
    ProtocolResult_NodeNotFound,
    ProtocolResult_SyntaxError,
} ProtocolResult_t;

class ProtocolParser: public AbstractUpLayer {
    AbstractSerialInterface* serialInterface;
public:
    ProtocolParser(AbstractSerialInterface* serialInterface);
    virtual ~ProtocolParser() {}
    void listen();
    virtual void receiveString(char* s);
    void reportError(ProtocolResult_t errorCode);
    static const char* resultToStr(ProtocolResult_t result);
};

#endif /* PROTOCOLPARSER_H_ */
