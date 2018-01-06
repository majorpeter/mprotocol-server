#ifndef PROTOCOLPARSER_H_
#define PROTOCOLPARSER_H_

#include "AbstractSerialInterface.h"
#include "AbstractUpLayer.h"
#include "mprotocol-nodes/Property.h"

class Node;

class ProtocolParser: public AbstractUpLayer {
public:
    static const uint16_t MaxLiteralLength = 64;
    static const uint16_t MaxSubscribedNodes = 15;

    ProtocolParser(AbstractSerialInterface* serialInterface);
    virtual ~ProtocolParser();
    void handleSubscriptions();
    static ProtocolParser* getExistingInstance();

    void switchSerialInterface(AbstractSerialInterface* interface);
    AbstractSerialInterface* getInterface();
    void listen();
    virtual bool receiveBytes(const uint8_t* bytes, uint16_t len);
    void handler();

    ProtocolResult_t parseString(char* s, uint16_t length = 0);
    void reportResult(ProtocolResult_t errorCode);
    void writeManual(const Node *node, const Property_t *property);
    static const char* resultToStr(ProtocolResult_t result);
    ProtocolResult_t addNodeToSubscribed(Node *node);
    ProtocolResult_t removeNodeFromSubscribed(Node *node);
private:
    enum class State {
        /// ready to receive next command
        Idle,
        /// the current command is invalid, wait for it to finish
        Invalid,
        /// decode the first few bytes to a valid protocol function
        DecodeFunction,
        /// find the node by path
        FindNode,
        /// find property by name
        FindProperty,
    };
    enum class ProtocolFunction {Unknown, Invalid, GET, SET, CALL, OPEN, CLOSE, MAN};
    enum class PropertyListingPreambleType {Get, Change};

    struct {
        State state;
        ProtocolFunction function;
        ProtocolResult_t result;
        Node* node;

        char rxBuffer[MaxLiteralLength + 1];
        uint16_t rxBufferPosition;
    } stateMachine;

    class BinarySerialInterface: public AbstractSerialInterface {
    public:
        BinarySerialInterface(AbstractSerialInterface* serialInterface,
                const Node* node, const Property_t* prop,
                PropertyListingPreambleType listingType);
        ~BinarySerialInterface();
        bool writeBytes(const uint8_t *data, uint16_t length);
    private:
        AbstractSerialInterface* serialInterface;
        const Node* node;
        const Property_t* prop;
        PropertyListingPreambleType listingType;
        bool hasTransmittedBytes;
    };

    static ProtocolParser* instance;
    AbstractSerialInterface* serialInterface;
    Node* subscribedNodes[MaxSubscribedNodes];
    uint16_t subscribedNodeCount;

    void listNode(Node *node);
    inline void printPropertyListingPreamble(const Node* node, const Property_t *prop, PropertyListingPreambleType type);
    static void printPropertyListingPreamble(AbstractSerialInterface* serialInterface, const Node* node, const Property_t *prop, PropertyListingPreambleType type);
    ProtocolResult_t listProperty(const Node *node, const Property_t *prop);
    ProtocolResult_t printPropertyValue(const Node *node, const Property_t *prop, PropertyListingPreambleType listingType);
    ProtocolResult_t setProperty(Node *node, const Property_t *prop, const char* value);
    ProtocolResult_t getBinaryProperty(const Node *node, const Property_t *prop, char* value);
    static ProtocolFunction decodeFunction(const char* str, uint16_t length);

    void resetStateMachine();
    void receiveByte(char c);
    inline void appendByteToStateMachineRx(char c);
    inline bool isLineEnd(char c);

    void handleReceivedCommands();
};

#endif /* PROTOCOLPARSER_H_ */
