#ifndef PROTOCOLPARSER_H_
#define PROTOCOLPARSER_H_

#include "AbstractSerialInterface.h"
#include "AbstractUpLayer.h"
#include "Nodes/Property.h"

class Node;

typedef enum {
    ProtocolResult_Ok,
    ProtocolResult_UnknownFunc,
    ProtocolResult_NodeNotFound,
    ProtocolResult_PropertyNotFound,
    ProtocolResult_SyntaxError,
    ProtocolResult_InvalidFunc,
} ProtocolResult_t;

class ProtocolParser: public AbstractUpLayer {
    AbstractSerialInterface* serialInterface;

    void listNode(Node *node);
    void getProperty(Node *node, const Property_t *prop);
    void setProperty(Node *node, const Property_t *prop, const char* value);
public:
    ProtocolParser(AbstractSerialInterface* serialInterface);
    virtual ~ProtocolParser() {}
    void listen();
    virtual void receiveString(char* s);
    ProtocolResult_t parseString(char* s);
    void reportError(ProtocolResult_t errorCode);
    static const char* resultToStr(ProtocolResult_t result);
};

#endif /* PROTOCOLPARSER_H_ */
