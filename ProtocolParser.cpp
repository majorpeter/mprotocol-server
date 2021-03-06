#include "ProtocolParser.h"
#include "utils.h"
#include "mprotocol-nodes/RootNode.h"
//TODO integrate log #include "Log/Log.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

ProtocolParser* ProtocolParser::instance = NULL;

ProtocolParser::ProtocolParser(AbstractSerialInterface* serialInterface): serialInterface(serialInterface) {
    subscribedNodeCount = 0;
    resetStateMachine();

    if (instance != NULL) {
        exit(1);    // kill application if invoked twice
    }
    instance = this;
    serialInterface->setUpLayer(this);
}

ProtocolParser::~ProtocolParser() {
    instance = NULL;
}

/**
 * this function returns the single instance if it exists
 * @note returns NULL if it does not exist
 */
ProtocolParser* ProtocolParser::getExistingInstance() {
    return instance;
}

void ProtocolParser::switchSerialInterface(AbstractSerialInterface* interface) {
    if (this->serialInterface != NULL) {
        // disable RX from previous interface
        this->serialInterface->setUpLayer(NULL);
    }

    resetStateMachine();

    // connect to new interface
    this->serialInterface = interface;
    interface->setUpLayer(this);
}

AbstractSerialInterface* ProtocolParser::getInterface() {
    return this->serialInterface;
}

void ProtocolParser::listen() {
    serialInterface->listen();
}

bool ProtocolParser::receiveBytes(const uint8_t* bytes, uint16_t len) {
    while (len > 0) {
        receiveByte((char) *bytes);
        bytes++;
        len--;
    }
    return true;
}

/**
 * handle change notifications @subscriptions
 * @note sends CHG messages about changes
 */
void ProtocolParser::handleSubscriptions() {
    for (uint16_t i = 0; i < subscribedNodeCount; i++) {
        uint32_t mask = subscribedNodes[i]->getAndClearPropChangeMask();
        uint8_t propIndex = 0;

        while (mask != 0x00) {
            // is LSB a changed property
            if (mask & 0x01) {
                printPropertyValue(subscribedNodes[i], subscribedNodes[i]->getProperties()[propIndex], PropertyListingPreambleType::Change);
            }

            mask = mask >> 1;
            propIndex++;
        }
        // write all CHG's from current node
        serialInterface->handler();
    }
}

void ProtocolParser::handler() {
    this->handleSubscriptions();
    serialInterface->handler();
}

ProtocolParser::ProtocolFunction ProtocolParser::decodeFunction(const char* str, uint16_t length) {
    if (length < 4) {
        return ProtocolFunction::Unknown;
    }

    if (strncmp("GET ", str, 4) == 0) {
        return ProtocolFunction::GET;
    } else if (strncmp("SET ", str, 4) == 0) {
        return ProtocolFunction::SET;
    } else if (strncmp("MAN ", str, 4) == 0) {
        return ProtocolFunction::MAN;
    }

    if (length < 5) {
        return ProtocolFunction::Unknown;
    }

    else if (strncmp("CALL ", str, 5) == 0) {
        return ProtocolFunction::CALL;
    } else if (strncmp("OPEN ", str, 5) == 0) {
        return ProtocolFunction::OPEN;
    }

    if (length < 6) {
        return ProtocolFunction::Unknown;
    }

    if (strncmp("CLOSE ", str, 6) == 0) {
        return ProtocolFunction::CLOSE;
    }

    /// no matter what bytes come next, this line is certainly invalid
    return ProtocolFunction::Invalid;
}

void ProtocolParser::resetStateMachine() {
    stateMachine.state = State::Idle;
    stateMachine.function = ProtocolFunction::Unknown;
    stateMachine.result = ProtocolResult_Ok;
    stateMachine.node = NULL;
    stateMachine.prop = NULL;
    stateMachine.rxBufferPosition = 0;
}

void ProtocolParser::receiveByte(char c) {
    switch (stateMachine.state) {
    case State::Idle:
        if (!isLineEnd(c)) {
            appendByteToStateMachineRx(c);
            stateMachine.state = State::DecodeFunction;
        }
        break;
    case State::Invalid:
        if (isLineEnd(c)) {
            reportResult(stateMachine.result);
            resetStateMachine();
        }
        break;
    case State::DecodeFunction:
        appendByteToStateMachineRx(c);
        if (c == ' ') {
            //TODO not append space
            stateMachine.function = decodeFunction(stateMachine.rxBuffer, stateMachine.rxBufferPosition);
            if (stateMachine.function != ProtocolFunction::Unknown) {
                stateMachine.state = State::FindNode;
                stateMachine.rxBufferPosition = 0;
            } else {
                stateMachine.state = State::Invalid;
                stateMachine.result = ProtocolResult_InvalidFunc;
            }
        }
        break;
    case State::FindNode:
        if (c == '/') {
            if (stateMachine.node == NULL) {
                stateMachine.node = RootNode::getInstance();
            } else {
                appendByteToStateMachineRx('\0');
                stateMachine.node = stateMachine.node->getChildByName(stateMachine.rxBuffer);
            }
        } else if (stateMachine.node == NULL) {
            // all node paths start with '/'
            stateMachine.state == State::Invalid;
            stateMachine.result = ProtocolResult_SyntaxError;
        } else if (c == '.') {
            appendByteToStateMachineRx('\0');
            stateMachine.node = stateMachine.node->getChildByName(stateMachine.rxBuffer);
            if (stateMachine.node != NULL) {
                stateMachine.state = State::FindProperty;
                stateMachine.rxBufferPosition = 0;
            } else {
                stateMachine.state = State::Invalid;
                stateMachine.result = ProtocolResult_NodeNotFound;
            }
        } else if (!isLineEnd(c)) {
            appendByteToStateMachineRx(c);
        } else {
            // isLineEnd
            switch (stateMachine.function) {
            case ProtocolFunction::GET:
            case ProtocolFunction::MAN:
            case ProtocolFunction::OPEN:
            case ProtocolFunction::CLOSE:
                // select node if command ended with a node name
                if (stateMachine.rxBufferPosition != 0) {
                    appendByteToStateMachineRx('\0');
                    stateMachine.node = stateMachine.node->getChildByName(stateMachine.rxBuffer);
                }

                if (stateMachine.node != NULL) {
                    switch (stateMachine.function) {
                    case ProtocolFunction::GET:
                        listNode(stateMachine.node);
                        break;
                    case ProtocolFunction::MAN:
                        writeManual(stateMachine.node, NULL);
                        break;
                    case ProtocolFunction::OPEN:
                        reportResult(addNodeToSubscribed(stateMachine.node));
                        break;
                    case ProtocolFunction::CLOSE:
                        reportResult(removeNodeFromSubscribed(stateMachine.node));
                        break;
                    default:
                        break;
                    }
                } else {
                    reportResult(ProtocolResult_NodeNotFound);
                }
                resetStateMachine();
                break;
            default:
                reportResult(ProtocolResult_SyntaxError);
                resetStateMachine();
                break;
            }
        }
        break;
    case State::FindProperty:
        if (!isLineEnd(c)) {
            if (c != '=') {
                appendByteToStateMachineRx(c);
            } else {
                appendByteToStateMachineRx('\0');
                stateMachine.prop = stateMachine.node->getPropertyByName(stateMachine.rxBuffer);
                if (stateMachine.prop != NULL) {
                    stateMachine.state = State::SetCallProperty;
                    stateMachine.rxBufferPosition = 0;
                    if (stateMachine.prop->type == PropertyType_BinarySegmented) {
                        bool result = getBinarySegmentedSetter(stateMachine.node, stateMachine.prop)->startTransaction();
                        if (!result) {
                            stateMachine.state = State::Invalid;
                            stateMachine.result = ProtocolResult_InternalError;
                        }
                    }
                } else {
                    stateMachine.state = State::Invalid;
                    stateMachine.result = ProtocolResult_PropertyNotFound;
                }
            }
        } else {
            appendByteToStateMachineRx('\0');
            stateMachine.prop = stateMachine.node->getPropertyByName(stateMachine.rxBuffer);
            if (stateMachine.prop != NULL) {
                switch (stateMachine.function) {
                case ProtocolFunction::GET:
                    listProperty(stateMachine.node, stateMachine.prop);
                    break;
                case ProtocolFunction::SET:
                    // cannot set value if not present
                    reportResult(ProtocolResult_SyntaxError);
                    break;
                case ProtocolFunction::CALL:
                    reportResult(setProperty(stateMachine.node, stateMachine.prop, ""));
                    break;
                case ProtocolFunction::MAN:
                    writeManual(stateMachine.node, stateMachine.prop);
                    break;
                }
            } else {
                reportResult(ProtocolResult_PropertyNotFound);
            }
            resetStateMachine();
        }
        break;
    case State::SetCallProperty:
        if (!isLineEnd(c)) {
            bool bufferFull = !appendByteToStateMachineRx(c);
            if (bufferFull) {
                if (stateMachine.prop->type != PropertyType_BinarySegmented) {
                    // longer value than MaxLiteralLength is not allowed
                    stateMachine.state = State::Invalid;
                    stateMachine.result = ProtocolResult_InvalidValue;
                } else {
                    if (!ProtocolServerUtils::binaryStringToByteArray(
                            (uint8_t*) stateMachine.rxBuffer,
                            stateMachine.rxBuffer,
                            stateMachine.rxBufferPosition / 2)) {
                        stateMachine.state = State::Invalid;
                        stateMachine.result = ProtocolResult_InvalidValue;
                    } else {
                        bool result = getBinarySegmentedSetter(stateMachine.node, stateMachine.prop)->transmitData((const uint8_t*) stateMachine.rxBuffer, stateMachine.rxBufferPosition / 2);
                        if (!result) {
                            stateMachine.state = State::Invalid;
                            stateMachine.result = ProtocolResult_InvalidValue;
                        } else {
                            stateMachine.rxBufferPosition = 0;
                            // append the byte that did not fit
                            appendByteToStateMachineRx(c);
                        }
                    }
                }
            }
        } else {
            if (stateMachine.prop->type != PropertyType_BinarySegmented) {
                appendByteToStateMachineRx('\0');
                reportResult(setProperty(stateMachine.node, stateMachine.prop, stateMachine.rxBuffer));
            } else {
                if ((stateMachine.rxBufferPosition & 1) || !ProtocolServerUtils::binaryStringToByteArray(
                        (uint8_t*) stateMachine.rxBuffer,
                        stateMachine.rxBuffer,
                        stateMachine.rxBufferPosition / 2)) {
                    stateMachine.state = State::Invalid;
                    stateMachine.result = ProtocolResult_InvalidValue;
                    break;
                } else {
                    bool result = getBinarySegmentedSetter(stateMachine.node, stateMachine.prop)->transmitData((const uint8_t*) stateMachine.rxBuffer, stateMachine.rxBufferPosition / 2);
                    if (!result) {
                        stateMachine.state = State::Invalid;
                        stateMachine.result = ProtocolResult_InvalidValue;
                        break;
                    }
                }

                bool result = getBinarySegmentedSetter(stateMachine.node, stateMachine.prop)->commitTransaction();
                if (result == true) {
                    reportResult(ProtocolResult_Ok);
                } else {
                    getBinarySegmentedSetter(stateMachine.node, stateMachine.prop)->cancelTransaction();
                    reportResult(ProtocolResult_InternalError);
                }
            }
            resetStateMachine();
        }
        break;
    default:
        reportResult(ProtocolResult_InternalError);
        break;
    }
}

/**
 * appends a byte to stateMachine.rxBuffer
 * @param c character to append
 * @return true if the buffer had enough space
 */
bool ProtocolParser::appendByteToStateMachineRx(char c) {
    if (stateMachine.rxBufferPosition < MaxLiteralLength) {
        stateMachine.rxBuffer[stateMachine.rxBufferPosition] = c;
        stateMachine.rxBufferPosition++;
        return true;
    } else if (c == '\0') {
        // always put the terminating NULL
        stateMachine.rxBuffer[MaxLiteralLength] = '\0';
    }
    return false;
}

bool ProtocolParser::isLineEnd(char c) {
    return (c == '\n') || (c == '\r');
}

void ProtocolParser::reportResult(ProtocolResult_t errorCode) {
    char buf[4];
    snprintf(buf, 4, "E%d:", errorCode);

    serialInterface->writeString(buf);
    serialInterface->writeString(resultToStr(errorCode));
    serialInterface->writeString("\n");
}

void ProtocolParser::writeManual(const Node *node, const Property_t *property) {
    serialInterface->writeBytes((uint8_t*) "MAN ", 4);
    //TODO node name maybe?
    if (property != NULL) {
        // property manual string
        serialInterface->writeString(property->description);
    } else {
        // node description
        serialInterface->writeString(node->getDescription());
    }
    serialInterface->writeBytes((uint8_t*) "\n", 1);
}

void ProtocolParser::listNode(Node *node) {
    serialInterface->writeString("{\n");
    Node* n = node->getFirstChild();
    while (n != NULL) {
        serialInterface->writeString("N ");
        serialInterface->writeString(n->getName());
        serialInterface->writeString("\n");
        n = n->getNextSibling();
    }

    const Property_t** props = node->getProperties();
    unsigned propsCount = node->getPropertiesCount();
    for (uint16_t i = 0; i < propsCount; i++) {
        listProperty(node, props[i]);
    }
    serialInterface->writeString("}\n");
}

void ProtocolParser::printPropertyListingPreamble(const Node* node, const Property_t *prop, PropertyListingPreambleType type) {
    printPropertyListingPreamble(serialInterface, node, prop, type);
}

void ProtocolParser::printPropertyListingPreamble(
        AbstractSerialInterface* serialInterface, const Node* node,
        const Property_t *prop, PropertyListingPreambleType type) {
    switch (type) {
    case PropertyListingPreambleType::Get:
        if (prop->accessLevel == PropAccessLevel_ReadWrite) {
            serialInterface->writeBytes((const uint8_t*) "PW_", 3);
        } else {
            serialInterface->writeBytes((const uint8_t*) "P_", 2);
        }
        serialInterface->writeString(Property_TypeToStr(prop->type));
        serialInterface->writeBytes((const uint8_t*) " ", 1);
        serialInterface->writeString(prop->name);
        break;
    case PropertyListingPreambleType::Change:
        serialInterface->writeString("CHG ");
        ProtocolServerUtils::printNodePathRecursively(serialInterface, node);
        serialInterface->writeBytes((uint8_t*) ".", 1);
        serialInterface->writeString(prop->name);
        break;
    }

    if (prop->accessLevel != PropAccessLevel_Invokable) {
        serialInterface->writeBytes((const uint8_t*) "=", 1);
    }
}

ProtocolResult_t ProtocolParser::listProperty(const Node *node, const Property_t *prop) {
    ProtocolResult_t result = ProtocolResult_InternalError;
    if (prop->accessLevel != PropAccessLevel_Invokable) {
        result = printPropertyValue(node, prop, PropertyListingPreambleType::Get);
        if (result != ProtocolResult_Ok) {
            serialInterface->writeString(resultToStr(result));
        }
    } else {
        printPropertyListingPreamble(serialInterface, node, prop, PropertyListingPreambleType::Get);
        serialInterface->writeBytes((const uint8_t*) "\n", 1);
    }
    return result;
}

ProtocolResult_t ProtocolParser::printPropertyValue(const Node *node, const Property_t *prop, PropertyListingPreambleType listingType) {
    node = (Node*)((uintptr_t)node - prop->nodeOffset);
    ProtocolResult_t result = ProtocolResult_InternalError;

    switch (prop->type) {
    case PropertyType_Bool: {
        bool boolValue;
        result = (node->*prop->boolGet)(&boolValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop, listingType);
            serialInterface->writeBytes((const uint8_t*) (boolValue ? "1\n" : "0\n"), 2);
        }
        break;
    }
    case PropertyType_Int32: {
        int32_t intValue;
        result = (node->*prop->intGet)(&intValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop, listingType);
            char buffer[12];
            sprintf(buffer, "%ld", (long int) intValue);
            serialInterface->writeString(buffer);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_Uint32: {
        uint32_t uintValue;
        result = (node->*prop->uintGet)(&uintValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop, listingType);
            char buffer[12];
            sprintf(buffer, "%lu", (long unsigned int) uintValue);
            serialInterface->writeString(buffer);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_Float32: {
        float floatValue;
        result = (node->*prop->floatGet)(&floatValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop, listingType);
            char buffer[20];
            ProtocolServerUtils::printFloat(buffer, floatValue);
            serialInterface->writeString(buffer);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_String: {
        char buffer[64]; //TODO length limitation in stringGet()?
        result = (node->*prop->stringGet)(buffer);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop, listingType);
            serialInterface->writeString(buffer);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_Binary: {
        uint8_t *ptr = NULL;
        uint16_t length = 0;
        result = (node->*prop->binaryGet)((void**) &ptr, &length);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop, listingType);
            ProtocolServerUtils::printBinaryDataAsHex(serialInterface, ptr, length);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_BinarySegmented: {
        BinarySerialInterface binarySerialInterface(serialInterface, node, prop, listingType);
        result = (node->*prop->binarySegmentedGet)(&binarySerialInterface);
        break;
    }
    default:
        break;
    }
    return result;
}

ProtocolResult_t ProtocolParser::setProperty(Node *node, const Property_t *prop, const char *value) {
    node = (Node*)((uintptr_t)node - prop->nodeOffset);
    ProtocolResult_t result = ProtocolResult_InternalError;

    switch (prop->type) {
    case PropertyType_Bool:
        if (strcmp(value, "1") == 0) {
            result = (node->*prop->boolSet)(true);
        } else if (strcmp(value, "0") == 0) {
            result = (node->*prop->boolSet)(false);
        } else {
            result= ProtocolResult_InvalidValue;
        }
        break;
    case PropertyType_Int32: {
        long int i;
        sscanf(value, "%ld", &i);
        result = (node->*prop->intSet)(i);
        break;
    }
    case PropertyType_Uint32:
        unsigned long int j;
        sscanf(value, "%lu", &j);
        result = (node->*prop->uintSet)(j);
        break;
    case PropertyType_Float32: {
        int fint, ffrac;
        float f = 0.f;
        bool isNegative = false;
        if (value[0] == '-') {
            isNegative = true;
            value++;
        }
        int res = sscanf(value, "%d.%d", &fint, &ffrac);
        if (res == 2) {
            int fracdivision = 1;
            const char *p = strchr(value, '.') + 1;    // cannot be NULL, because sscanf found it
            while (*p != '\0') {
                fracdivision *= 10;
                p++;
            }
            f = (float) fint + (float) ffrac / (float) fracdivision;
        } else if (res == 1) {
            f = (float) fint;
        } else
            break;
        if (isNegative) {
            f = 0.f - f;
        }
        result = (node->*prop->floatSet)(f);
        break;
    }
    case PropertyType_String:
        result = (node->*prop->stringSet)(value);
        break;
    case PropertyType_Method:
        result = (node->*prop->methodInvoke)(value);
        break;
    case PropertyType_Binary: {
        size_t length = strlen(value);
        if (length & 0x01) {
            return ProtocolResult_InvalidValue;
        }
        uint8_t data[length / 2];
        if (!ProtocolServerUtils::binaryStringToByteArray(data, value, length / 2)) {
            return  ProtocolResult_InvalidValue;
        }
        result = (node->*prop->binarySet)(data, length / 2);
        break;
    }
    case PropertyType_BinarySegmented: {
        size_t length = strlen(value);
        if (length & 0x01) {
            return ProtocolResult_InvalidValue;
        }
        uint8_t data[length / 2];
        if (!ProtocolServerUtils::binaryStringToByteArray(data, value, length / 2)) {
            return  ProtocolResult_InvalidValue;
        }

        AbstractPacketInterface* packetInterface = (AbstractPacketInterface*) ((uintptr_t) node + (uintptr_t) prop->binarySegmentedSet);
        if (!packetInterface->startTransaction()) {
            return ProtocolResult_InternalError;
        }
        if (!packetInterface->transmitData(data, length / 2)) {
            packetInterface->cancelTransaction();
            return ProtocolResult_InvalidValue;
        }
        if (!packetInterface->commitTransaction()) {
            packetInterface->cancelTransaction();
            return ProtocolResult_InternalError;
        }
        result = ProtocolResult_Ok;
        break;
    }
    }
    return result;
}

ProtocolResult_t ProtocolParser::getBinaryProperty(const Node *node, const Property_t *prop, char* value) {
    uint8_t *ptr = NULL;
    uint16_t length = 0;
    ProtocolResult_t result = (node->*prop->binaryGet)((void**) &ptr, &length);
    if (result == ProtocolResult_Ok) {
        while (length > 0) {
            sprintf(value, "%02X", *ptr);
            value += 2;
            ptr++;
            length--;
        }
    }
    return result;
}

const char* ProtocolParser::resultToStr(ProtocolResult_t result) {
    switch (result) {
    case ProtocolResult_Ok: return "Ok";
    case ProtocolResult_Failed: return "Failed";
    case ProtocolResult_UnknownFunc: return "Unknown function";
    case ProtocolResult_NodeNotFound: return "Node not found";
    case ProtocolResult_PropertyNotFound: return "Property not found";
    case ProtocolResult_SyntaxError: return "Syntax Error";
    case ProtocolResult_InvalidFunc: return "Invalid function";
    case ProtocolResult_InvalidValue: return "Invalid value";
    case ProtocolResult_InternalError: return "Internal error";
    }
    return "Unknown Error code!";
}

/**
 * subscribe to async change messages for node
 * @return ProtocolResult_Ok, if added to array, ProtocolResult_Failed if array already exists or array full
 */
ProtocolResult_t ProtocolParser::addNodeToSubscribed(Node *node) {
    if (subscribedNodeCount == MaxSubscribedNodes) {
        return ProtocolResult_Failed;
    }

    for (uint16_t i = 0; i < subscribedNodeCount; i++) {
        if (subscribedNodes[i] == node ) {
            return ProtocolResult_Failed;
        }
    }

    subscribedNodes[subscribedNodeCount] = node;
    subscribedNodeCount++;

    return ProtocolResult_Ok;
}

/**
 * unsubscribe from async change messages for node
 * @return ProtocolResult_Ok, if removed from array, ProtocolResult_Failed if not found in array
 */
ProtocolResult_t ProtocolParser::removeNodeFromSubscribed(Node *node) {
    ProtocolResult_t result = ProtocolResult_Failed;
    uint16_t i;
    for (i = 0; i < subscribedNodeCount; i++) {
        if (subscribedNodes[i] == node ) {
            result = ProtocolResult_Ok;
            break;
        }
    }

    if (result == ProtocolResult_Ok) {
        subscribedNodeCount--;
        for (; i < subscribedNodeCount; i++) {
            subscribedNodes[i] = subscribedNodes[i+1];
        }
    }
    return result;
}

ProtocolParser::BinarySerialInterface::BinarySerialInterface(
        AbstractSerialInterface* serialInterface, const Node* node,
        const Property_t* prop, PropertyListingPreambleType listingType) :
        serialInterface(serialInterface), node(node), prop(prop), listingType(listingType) {
    hasTransmittedBytes = false;
}

ProtocolParser::BinarySerialInterface::~BinarySerialInterface() {
    if (hasTransmittedBytes) {
        serialInterface->writeBytes((const uint8_t*) "\n", 1);
    }
}

bool ProtocolParser::BinarySerialInterface::writeBytes(const uint8_t *data, uint16_t length) {
    if (!hasTransmittedBytes) {
        // if the transaction is started, result must be Ok
        ProtocolParser::printPropertyListingPreamble(serialInterface, node, prop, listingType);
        hasTransmittedBytes = true;
    }

    ProtocolServerUtils::printBinaryDataAsHex(serialInterface, data, length);
    return true;
}
