#ifndef PROTOCOLPARSER_H_
#define PROTOCOLPARSER_H_

#include "AbstractSerialInterface.h"
#include "AbstractUpLayer.h"
#include "Nodes/Property.h"

class Node;

class ProtocolParser: public AbstractUpLayer {
	static ProtocolParser* instance;
    AbstractSerialInterface* serialInterface;
    char *rxBuffer;
    int rxPosition;
    Node **subscribedNodes;
    uint16_t subscribedNodeCount;

    void listNode(Node *node);
    ProtocolResult_t listProperty(const Node *node, const Property_t *prop);
    ProtocolResult_t getProperty(const Node *node, const Property_t *prop, char* value);
    ProtocolResult_t setProperty(Node *node, const Property_t *prop, const char* value);
    ProtocolResult_t getBinaryProperty(const Node *node, const Property_t *prop, char* value);

    void handleSubscriptions();
    void handleReceivedCommands();
public:
    ProtocolParser(AbstractSerialInterface* serialInterface);
    virtual ~ProtocolParser();
    static ProtocolParser* getExistingInstance();
    void listen();
    virtual bool receiveBytes(const uint8_t* bytes, uint16_t len);
    void handler();
    ProtocolResult_t parseString(char* s);
    void reportResult(ProtocolResult_t errorCode);
    void writeManual(const Node *node, const Property_t *property);
    static const char* resultToStr(ProtocolResult_t result);
    ProtocolResult_t addNodeToSubscribed(Node *node);
    ProtocolResult_t removeNodeFromSubscribed(Node *node);
};

#endif /* PROTOCOLPARSER_H_ */
