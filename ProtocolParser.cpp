#include "ProtocolParser.h"
#include "utils.h"
#include "mprotocol-nodes/RootNode.h"
//TODO integrate log #include "Log/Log.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#define RX_BUFFER_SIZE 256
#define MAX_SUBSCRIBED_NODES 15

#ifndef SIMULATOR
#define SUBSCRIPTION_HANDLE_PERIOD_MS 100
#else
#define SUBSCRIPTION_HANDLE_PERIOD_MS 1    // send update in each simulation step
#endif

ProtocolParser* ProtocolParser::instance = NULL;

ProtocolParser::ProtocolParser(AbstractSerialInterface* serialInterface): serialInterface(serialInterface) {
    rxBuffer = new char[RX_BUFFER_SIZE];
    rxPosition = 0;

    subscribedNodes = new Node*[MAX_SUBSCRIBED_NODES];
    subscribedNodeCount = 0;

    if (instance != NULL) {
        exit(1);    // kill application if invoked twice
    }
    instance = this;
    serialInterface->setUpLayer(this);
}

ProtocolParser::~ProtocolParser() {
    delete[] rxBuffer;
    delete[] subscribedNodes;
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
        this->serialInterface->setUpLayer(NULL);
    }

    rxPosition = 0;
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
    bool result = true;
    if (rxPosition + len > RX_BUFFER_SIZE) {
        result = false;
        len = RX_BUFFER_SIZE - rxPosition;
    }

    if (len == 0) {
        return result;
    }

    memcpy(rxBuffer + rxPosition, bytes, len);
    rxPosition += len;
    return result;
}

/**
 * handle change notifications @subscriptions
 * @note sends CHG messages about changes
 */
void ProtocolParser::handleSubscriptions() {
    for (uint16_t i = 0; i < subscribedNodeCount; i++) {
        uint32_t mask = subscribedNodes[i]->getAndClearPropChangeMask();
        uint8_t propIndex = 0;

        while (mask != 0) {
            // is LSB a changed property
            if (mask & 0x01) {
                /*TODO serialInterface->writeString("CHG ");
                this->printNodePathRecursively(subscribedNodes[i]);
                serialInterface->writeBytes((uint8_t*) ".", 1);
                serialInterface->writeString(subscribedNodes[i]->getProperties()[propIndex]->name);
                serialInterface->writeBytes((uint8_t*) "=", 1);
                char buffer[256];    //TODO buffer size? maybe a common buffer?
                getProperty(subscribedNodes[i], subscribedNodes[i]->getProperties()[propIndex], buffer);    //TODO print instead
                *serialInterface << buffer;
                serialInterface->writeBytes((uint8_t*) "\n", 1);*/
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
    int rxLineStart = 0;
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
    }
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

ProtocolResult_t ProtocolParser::parseString(char *s, uint16_t length) {
    ProtocolResult_t result = ProtocolResult_InternalError;

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


    return ProtocolResult_UnknownFunc;
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

void ProtocolParser::printPropertyListingPreamble(const Node* node, const Property_t *prop) {
    printPropertyListingPreamble(serialInterface, node, prop);
}

void ProtocolParser::printPropertyListingPreamble(AbstractSerialInterface* serialInterface, const Node* /*TODO node AND type*/, const Property_t *prop) {
    if (prop->accessLevel == PropAccessLevel_ReadWrite) {
        serialInterface->writeBytes((const uint8_t*) "PW_", 3);
    } else {
        serialInterface->writeBytes((const uint8_t*) "P_", 2);
    }
    serialInterface->writeString(Property_TypeToStr(prop->type));
    serialInterface->writeBytes((const uint8_t*) " ", 1);
    serialInterface->writeString(prop->name);

    if (prop->accessLevel != PropAccessLevel_Invokable) {
        serialInterface->writeBytes((const uint8_t*) "=", 1);
    }
}

ProtocolResult_t ProtocolParser::listProperty(const Node *node, const Property_t *prop) {
    ProtocolResult_t result = ProtocolResult_InternalError;
    if (prop->accessLevel != PropAccessLevel_Invokable) {
        result = printPropertyValue(node, prop);
        if (result != ProtocolResult_Ok) {
            serialInterface->writeString(resultToStr(result));
        }
    }
    return result;
}

ProtocolResult_t ProtocolParser::printPropertyValue(const Node *node, const Property_t *prop) {
    node = (Node*)((int)node - prop->nodeOffset);
    ProtocolResult_t result = ProtocolResult_InternalError;
    char value[256]; //TODO optimize out

    switch (prop->type) {
    case PropertyType_Bool: {
        bool boolValue;
        result = (node->*prop->boolGet)(&boolValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop);
            serialInterface->writeBytes((const uint8_t*) (boolValue ? "1\n" : "0\n"), 2);
        }
        break;
    }
    case PropertyType_Int32: {
        int32_t intValue;
        result = (node->*prop->intGet)(&intValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop);
            sprintf(value, "%ld", (long int) intValue);
            serialInterface->writeString(value);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_Uint32: {
        uint32_t uintValue;
        result = (node->*prop->uintGet)(&uintValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop);
            sprintf(value, "%lu", (long unsigned int) uintValue);
            serialInterface->writeString(value);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_Float32: {
        float floatValue;
        result = (node->*prop->floatGet)(&floatValue);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop);
            ProtocolServerUtils::printFloat(value, floatValue);
            serialInterface->writeString(value);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_String:
        result = (node->*prop->stringGet)(value);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop);
            serialInterface->writeString(value);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    case PropertyType_Binary: {
        uint8_t *ptr = NULL;
        uint16_t length = 0;
        result = (node->*prop->binaryGet)((void**) &ptr, &length);
        if (result == ProtocolResult_Ok) {
            printPropertyListingPreamble(node, prop);
            ProtocolServerUtils::printBinaryDataAsHex(serialInterface, ptr, length);
            serialInterface->writeBytes((const uint8_t*) "\n", 1);
        }
        break;
    }
    case PropertyType_BinarySegmented: {
        BinaryPacketInterface binaryPacketInterface(serialInterface, node, prop);
        result = (node->*prop->binarySegmentedGet)(&binaryPacketInterface);
        break;
    }
    default:
        value[0] = '\0';
        break;
    }
    return result;
}

ProtocolResult_t ProtocolParser::setProperty(Node *node, const Property_t *prop, const char *value) {
    node = (Node*)((size_t)node - prop->nodeOffset);
    ProtocolResult_t result = ProtocolResult_InternalError;

    switch (prop->type) {
    case PropertyType_Bool:
        result = (node->*prop->boolSet)(value[0] != '0');
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

        AbstractPacketInterface* packetInterface = (AbstractPacketInterface*) ((uint8_t*) node + (uint32_t) prop->binarySegmentedSet);
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
    if (subscribedNodeCount == MAX_SUBSCRIBED_NODES) {
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

void ProtocolParser::printNodePathRecursively(const Node* node) {
    const Node* parent = node->getParent();
    if (parent != NULL) {
        this->printNodePathRecursively(parent);
        if (parent != RootNode::getInstance()) {
            this->serialInterface->writeBytes((uint8_t*) '/', 1);
        }
    }

    if (node->getName() != NULL) {
        this->serialInterface->writeString(node->getName());
    }
}

ProtocolParser::BinaryPacketInterface::BinaryPacketInterface(
        AbstractSerialInterface* serialInterface, const Node* node, const Property_t* prop) :
        serialInterface(serialInterface), node(node), prop(prop) {
}

bool ProtocolParser::BinaryPacketInterface::startTransaction() {
    // if the transaction is started, result must be Ok
    ProtocolParser::printPropertyListingPreamble(serialInterface, node, prop);
    return true;
}

bool ProtocolParser::BinaryPacketInterface::transmitData(const uint8_t *data, uint16_t length) {
    ProtocolServerUtils::printBinaryDataAsHex(serialInterface, data, length);
    return true;
}

bool ProtocolParser::BinaryPacketInterface::commitTransaction() {
    serialInterface->writeBytes((const uint8_t*) "\n", 1);
    return true;
}

void ProtocolParser::BinaryPacketInterface::cancelTransaction() {
    // invalid operation
    exit(1);
}
