#include "ProtocolParser.h"
#include "utils.h"
#include "mprotocol-nodes/RootNode.h"
//TODO integrate log #include "Log/Log.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifndef SIMULATOR
#define SUBSCRIPTION_HANDLE_PERIOD_MS 100
#else
#define SUBSCRIPTION_HANDLE_PERIOD_MS 1    // send update in each simulation step
#endif

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

/**
 * parse received lines (commands)
 * @note also moves an incomplete command from the buffer to the start position of buffer
 */
void ProtocolParser::handleReceivedCommands() {
    /*int rxLineStart = 0;
    while (1) {
        // find \n or \r at end of line
        char *nl = (char*) memchr(rxBuffer + rxLineStart, '\n', rxPosition - rxLineStart);
        char *cr = (char*) memchr(rxBuffer + rxLineStart, '\r', rxPosition - rxLineStart);
        if (nl == NULL) {
            nl = cr;
            if (nl == NULL) {
                break;
            }
        } else if (cr != NULL) {
            *cr = '\0';
        }

        // skip empty lines
        if (&rxBuffer[rxLineStart] == nl) {
            rxLineStart++;
            continue;
        }

        // change new line char to null (terminate string)
        *nl = '\0';

        // parse current line
        ProtocolResult_t result = parseString(&rxBuffer[rxLineStart], nl - &rxBuffer[rxLineStart]);
        if (result != ProtocolResult_Ok) {
            reportResult(result);
        }

        // update next line start address, the next byte after '\n'
        rxLineStart = (nl - rxBuffer) + 1;
    }

    // move last incomplete command to buffer start (if not already the first bytes)
    //TODO lock buffer for this?
    if ((rxLineStart > 0) && (rxPosition != 0)) {
        if (rxPosition > rxLineStart) {
            rxPosition = rxPosition - rxLineStart;
            memcpy(rxBuffer, &rxBuffer[rxLineStart], rxPosition);
        } else {
            rxPosition = 0; // all lines parsed, buffer is empty
        }
    }

    // if the RX buffer is full, and it could not be processed in this iteration, it won't be processed in the next either...
    if (rxPosition == RX_BUFFER_SIZE) {
        rxPosition = 0;
        reportResult(ProtocolResult_SyntaxError);
    }*/
}

void ProtocolParser::handler() {
    this->handleSubscriptions();
    this->handleReceivedCommands();
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
            appendByteToStateMachineRx(c);
        } else {
            appendByteToStateMachineRx('\0');
            const Property_t* prop = stateMachine.node->getPropertyByName(stateMachine.rxBuffer);
            if (prop != NULL) {
                switch (stateMachine.function) {
                case ProtocolFunction::GET:
                    listProperty(stateMachine.node, prop);
                    break;
                case ProtocolFunction::SET:
                    // cannot set value if not present
                    reportResult(ProtocolResult_SyntaxError);
                    break;
                case ProtocolFunction::CALL:
                    reportResult(setProperty(stateMachine.node, prop, ""));
                    break;
                case ProtocolFunction::MAN:
                    writeManual(stateMachine.node, prop);
                    break;
                }
            } else {
                reportResult(ProtocolResult_PropertyNotFound);
            }
            resetStateMachine();
        }
        break;
    default:
        exit(1);
    }
}

void ProtocolParser::appendByteToStateMachineRx(char c) {
    if (stateMachine.rxBufferPosition < MaxLiteralLength) {
        stateMachine.rxBuffer[stateMachine.rxBufferPosition] = c;
        stateMachine.rxBufferPosition++;
    } else if (c == '\0') {
        // always put the terminating NULL
        stateMachine.rxBuffer[MaxLiteralLength] = '\0';
    }
}

bool ProtocolParser::isLineEnd(char c) {
    return (c == '\n') || (c == '\r');
}

ProtocolResult_t ProtocolParser::parseString(char *s, uint16_t length) {
    /*ProtocolResult_t result = ProtocolResult_InternalError;

    char *p = strchr(s, '\n');
    if (p == NULL) {
        if (length == 0) {
            return result;
        }
    } else {
        length = p - s;
        *p = '\0';
    }

    ProtocolFunction decodedFunc = decodeFunction(s, length);
    if (decodedFunc == ProtocolFunction::Invalid || decodedFunc == ProtocolFunction::Unknown) {
        return ProtocolResult_UnknownFunc;
    }
    // jump after function (must be valid, since decodeFunction() was successful)
    s = strchr(s, ' ') + 1;

    // decode target node
    Node* node = NULL;
    if (s[0] != '/') {
        return ProtocolResult_NodeNotFound;
    }
    node = RootNode::getInstance();
    s++;
    for (;;) {
        p = strchr(s, '/');
        if (p == NULL) {
            break;
        }
        *p ='\0';
        node = node->getChildByName(s);
        if (node == NULL) {
            return ProtocolResult_NodeNotFound;
        }
        s = p+1;
    }

    // there is no property if there is no '.' in string
    if ((s[0] != '\0') && (strchr(s, '.') == NULL)) {
        node = node->getChildByName(s);
        s[0] = '\0';    // end of string, there is no property
    }

    if (node == NULL) {
        return ProtocolResult_NodeNotFound;
    }

    // Node found by name, end of command
    if (s[0] == '\0') {
        switch (decodedFunc) {
        case ProtocolFunction::GET:
            listNode(node);
            return ProtocolResult_Ok;
        case ProtocolFunction::OPEN:
            result = addNodeToSubscribed(node);
            if (result == ProtocolResult_Ok) {
                this->reportResult(ProtocolResult_Ok);
            }
            return result;
        case ProtocolFunction::CLOSE:
            result = removeNodeFromSubscribed(node);
            if (result == ProtocolResult_Ok) {
                this->reportResult(ProtocolResult_Ok);
            }
            return result;
        case ProtocolFunction::MAN:
            this->writeManual(node, NULL);
            return ProtocolResult_Ok;
        default:
            return ProtocolResult_InvalidFunc;
        }
    }

    // check for Property in Node (after '.')
    p = strchr(s, '.');
    if (p == NULL) {
        return ProtocolResult_SyntaxError;
    }
    *p = '\0';
    node = node->getChildByName(s);
    s = p + 1;
    if (node == NULL) {
        return ProtocolResult_NodeNotFound;
    }

    p = strchr(s, '=');
    if (p != NULL) {
        *p = '\0';
    }
    const Property_t *prop = node->getPropertyByName(s);
    s = p + 1;
    if (prop == NULL) {
        return ProtocolResult_PropertyNotFound;
    }

    switch (decodedFunc) {
    case ProtocolFunction::GET:
        if (prop->type == PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        return this->listProperty(node, prop);
    case ProtocolFunction::SET:
        if (prop->accessLevel != PropAccessLevel_ReadWrite) {
            return ProtocolResult_InvalidFunc;
        }
        result = setProperty(node, prop, s);
        if (result == ProtocolResult_Ok) {
            this->reportResult(ProtocolResult_Ok);
        }
        return result;
    case ProtocolFunction::CALL:
        if (prop->type != PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        result = setProperty(node, prop, s);
        if (result == ProtocolResult_Ok) {
            this->reportResult(ProtocolResult_Ok);
        }
        return result;
    case ProtocolFunction::MAN:
        this->writeManual(node, prop);
        return ProtocolResult_Ok;
    default:
        break;
    }


    return ProtocolResult_UnknownFunc;*/
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
