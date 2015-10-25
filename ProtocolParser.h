#ifndef PROTOCOLPARSER_H_
#define PROTOCOLPARSER_H_

#include "AbstractSerialInterface.h"
#include "AbstractUpLayer.h"
#include "Nodes/Property.h"

class Node;

class ProtocolParser: public AbstractUpLayer {
    AbstractSerialInterface* serialInterface;
    char *rxBuffer;
    uint16_t rxPosition;

    void listNode(Node *node);
    ProtocolResult_t getProperty(Node *node, const Property_t *prop, char* value);
    ProtocolResult_t setProperty(Node *node, const Property_t *prop, const char* value);
public:
    ProtocolParser(AbstractSerialInterface* serialInterface);
    virtual ~ProtocolParser() {}
    void listen();
    virtual void receiveBytes(const uint8_t* bytes, uint16_t len);
    void handler();
    ProtocolResult_t parseString(char* s);
    void reportError(ProtocolResult_t errorCode);
    static const char* resultToStr(ProtocolResult_t result);
};

#endif /* PROTOCOLPARSER_H_ */
