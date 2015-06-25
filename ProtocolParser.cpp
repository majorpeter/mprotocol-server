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

    //TODO ITM
    fputs(buf, stdout);
    fputs(resultToStr(errorCode), stdout);
    fputs("\n", stdout);
}

void ProtocolParser::listNode(Node *node) {
    puts("{");
    Node* n = node->getFirstChild();
    while (n != NULL) {
        fputs("N ", stdout);
        fputs(n->getName(), stdout);
        fputs("\n", stdout);
        n = n->getNextSibling();
    }

    const Property_t** props = node->getProperties();
    uint16_t propsCount = node->getPropertiesCount();
    for (uint16_t i = 0; i < propsCount; i++) {
        puts(props[i]->name);
    }
    puts("}");
}

void ProtocolParser::getProperty(Node *node, const Property_t *prop) {
    char buffer[256];
    prop->getter(node, buffer);
    switch (prop->type) {
    case PropertyType_Uint32: sprintf(buffer, "%u", *((unsigned int*)buffer)); break;
    case PropertyType_Int32: sprintf(buffer, "%d", *((int*)buffer)); break;
    case PropertyType_Bool: strcpy(buffer, buffer[0] ? "true" : "false"); break;
    case PropertyType_String: break;
    case PropertyType_Method: strcpy(buffer, "METHOD GETTER CALLED!"); break;
    }
    puts(buffer);
}

void ProtocolParser::setProperty(Node *node, const Property_t *prop, const char *value) {
    char buffer[256] = {0};
    switch (prop->type) {
    case PropertyType_Uint32: sscanf(value, "%u", (unsigned int*)buffer); break;
    case PropertyType_Int32: sscanf(value, "%d", (int*)buffer); break;
    case PropertyType_Bool:
        if ((strcmp(value, "true") == 0) || (strcmp(value, "1") == 0))
            buffer[0] = 1;
        else if ((strcmp(value, "false") == 0) || (strcmp(value, "0") == 0))
            buffer[0] = 0;
        else
            return;
        break;
    case PropertyType_String:
    case PropertyType_Method:
        strcpy(buffer, value);
        break;
    }
    prop->setter(node, buffer);
    if (prop->type == PropertyType_Method) {
        fputs(buffer, stdout);
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
