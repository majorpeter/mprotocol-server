#include "ProtocolParser.h"
#include "Nodes/RootNode.h"
#include <stdio.h>
#include <string.h>

ProtocolParser::ProtocolParser(AbstractSerialInterface* serialInterface): serialInterface(serialInterface) {}

void ProtocolParser::listen() {
    serialInterface->listen();
}

void ProtocolParser::receiveString(char* s) {
    ProtocolResult_t result = parseString(s);
    if (result != ProtocolResult_Ok) {
        reportError(result);
    }
}

ProtocolResult_t ProtocolParser::parseString(char *s) {
    // decode protocol function
    enum {UNKNOWN, GET, SET, CALL} decodedFunc = UNKNOWN;

    if (strncmp("GET ", s, 4) == 0) {
        decodedFunc = GET;
        s += 4;
    } else if (strncmp("SET ", s, 4) == 0) {
        decodedFunc = SET;
        s += 4;
    } else if (strncmp("CALL ", s, 5) == 0) {
        decodedFunc = CALL;
        s += 5;
    }

    if (decodedFunc == UNKNOWN) {
        return ProtocolResult_UnknownFunc;
    }

    // decode target node
    Node* node = NULL;
    char *p;
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
    if (node == NULL) {
        return ProtocolResult_NodeNotFound;
    }

    if (s[0] == '\0') {
        if (decodedFunc == GET) {
            listNode(node);
            return ProtocolResult_Ok;
        } else {
            return ProtocolResult_InvalidFunc;
        }
    }

    p = strchr(s, '.');
    if (p == NULL) {
        if (decodedFunc == GET) {
            node = node->getChildByName(s);
            if (node != NULL) {
                listNode(node);
                return ProtocolResult_Ok;
            }
        }
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
    default:
    case GET:
        if (prop->type == PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        getProperty(node, prop);
        return ProtocolResult_Ok;
    case SET:
        if (prop->accessLevel != PropAccessLevel_ReadWrite) {
            return ProtocolResult_InvalidFunc;
        }
        setProperty(node, prop, s);
        return ProtocolResult_Ok;
    case CALL:
        if (prop->type != PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        setProperty(node, prop, s);
        return ProtocolResult_Ok;
    }


    //return ProtocolResult_UnknownFunc;
}

void ProtocolParser::reportError(ProtocolResult_t errorCode) {
    char buf[4];
    snprintf(buf, 4, "E%d:", errorCode);

    serialInterface->writeString(buf);
    serialInterface->writeString(resultToStr(errorCode));
    serialInterface->writeString("\n");
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
        serialInterface->writeString("P_");
        serialInterface->writeString(Property_TypeToStr(props[i]->type));
        serialInterface->writeString(" ");
        serialInterface->writeString(props[i]->name);
        if (props[i]->accessLevel != PropAccessLevel_Invokable) {
            serialInterface->writeString("=");
            getProperty(node, props[i]);
        }
        serialInterface->writeString("\n");
    }
    serialInterface->writeString("}\n");
}

void ProtocolParser::getProperty(Node *node, const Property_t *prop) {
    node = (Node*)((int)node - prop->nodeOffset);
    char buffer[256];
    switch (prop->type) {
    case PropertyType_Bool:
        prop->boolGet(node, (bool*)buffer);
        buffer[0] = buffer[0] ? '1' : '0';
        buffer[1] = '\0';
        break;
    case PropertyType_Int32:
        prop->intGet(node, (int32_t*)buffer);
        sprintf(buffer, "%ld", *(int32_t*)buffer);
        break;
    case PropertyType_Uint32:
        prop->uintGet(node, (uint32_t*)buffer);
        sprintf(buffer, "%lu", *(uint32_t*)buffer);
        break;
    case PropertyType_String:
        prop->stringGet(node, buffer);
        break;
    default: buffer[0] = '\0';
    }
    serialInterface->writeString(buffer);
}

void ProtocolParser::setProperty(Node *node, const Property_t *prop, const char *value) {
    node = (Node*)((int)node - prop->nodeOffset);
    switch (prop->type) {
    case PropertyType_Bool:
        prop->boolSet(node, value[0] != '0');
        break;
    case PropertyType_Int32:
        int32_t i;
        sscanf(value, "%ld", &i);
        prop->intSet(node, i);
        break;
    case PropertyType_Uint32:
        uint32_t j;
        sscanf(value, "%lu", &j);
        prop->uintSet(node, j);
        break;
    case PropertyType_String:
        prop->stringSet(node, value);
        break;
    case PropertyType_Method:
        prop->methodInvoke(node, value);
        break;
    }
}

const char* ProtocolParser::resultToStr(ProtocolResult_t result) {
    switch (result) {
    case ProtocolResult_Ok: return "Ok";
    case ProtocolResult_UnknownFunc: return "Unknown function";
    case ProtocolResult_NodeNotFound: return "Node not found";
    case ProtocolResult_PropertyNotFound: return "Property not found";
    case ProtocolResult_SyntaxError: return "Syntax Error";
    case ProtocolResult_InvalidFunc: return "Invalid function";
    }
    return "Unknown Error code!";
}
