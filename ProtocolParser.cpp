#include "ProtocolParser.h"
#include "Nodes/RootNode.h"
#include <cstdio>
#include <cstring>

#define RX_BUFFER_SIZE 256
#define MAX_SUBSCRIBED_NODES 15

ProtocolParser* ProtocolParser::instance = NULL;

ProtocolParser::ProtocolParser(AbstractSerialInterface* serialInterface): serialInterface(serialInterface) {
	rxBuffer = new char[RX_BUFFER_SIZE];
	rxPosition = 0;

	subscribedNodes = new Node*[MAX_SUBSCRIBED_NODES];
	subscribedNodeCount = 0;

	//TODO check if it is null now
	instance = this;
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

void ProtocolParser::listen() {
    serialInterface->listen();
}

bool ProtocolParser::receiveBytes(const uint8_t* bytes, uint16_t len) {
	if (rxPosition + len > RX_BUFFER_SIZE) {
		return false;
	}
	memcpy(rxBuffer + rxPosition, bytes, len);
	rxPosition += len;
	return true;
}

void ProtocolParser::handler() {
	// handle change notifications @subscriptions
	for (uint16_t i = 0; i < subscribedNodeCount; i++) {
		uint32_t mask = subscribedNodes[i]->getAndClearPropChangeMask();
		uint8_t propIndex = 0;

		while (mask != 0) {
			// is LSB a changed property
			if (mask & 0x01) {
				serialInterface->writeString("CHG ");
				serialInterface->writeString(subscribedNodes[i]->getName());
				serialInterface->writeBytes((uint8_t*) ".", 1);
				serialInterface->writeString(subscribedNodes[i]->getProperties()[propIndex]->name);
				serialInterface->writeBytes((uint8_t*) "\n", 1);
				//TODO finish this
			}

			mask = mask >> 1;
			propIndex++;
		}
	}

	char *nl = (char*) memchr(rxBuffer, '\n', rxPosition);
	if (nl == NULL) {
		nl = (char*) memchr(rxBuffer, '\r', rxPosition);
		if (nl == NULL) {
			return;
		}
	}

	// change new line char to null (terminate string)
	*nl = '\0';
	rxPosition = 0; //TODO allow multiple commands in 1 handler
	ProtocolResult_t result = parseString(rxBuffer);
	if (result != ProtocolResult_Ok) {
		reportError(result);
	}

	serialInterface->handler();
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
            } else {
                return ProtocolResult_NodeNotFound;
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

    ProtocolResult_t result;
    switch (decodedFunc) {
    default:
    case GET:
        if (prop->type == PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        char buffer[256];
        result = this->getProperty(node, prop, buffer);
        if (result == ProtocolResult_Ok) {
            serialInterface->writeString(prop->name);
            serialInterface->writeString("=");
            serialInterface->writeString(buffer);
            serialInterface->writeString("\n");
        } else {
            serialInterface->writeString(resultToStr(result));
        }
        return result;
    case SET:
        if (prop->accessLevel != PropAccessLevel_ReadWrite) {
            return ProtocolResult_InvalidFunc;
        }
        result = setProperty(node, prop, s);
        serialInterface->writeString(resultToStr(result));
        return result;
    case CALL:
        if (prop->type != PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        result = setProperty(node, prop, s);
        serialInterface->writeString(resultToStr(result));
        return result;
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
        char buffer[256];
        strcpy(buffer, "P_");   // maybe optimize?
        strcat(buffer, Property_TypeToStr(props[i]->type));
        strcat(buffer, " ");
        strcat(buffer, props[i]->name);
        if (props[i]->accessLevel != PropAccessLevel_Invokable) {
            strcat(buffer, "=");

            ProtocolResult_t result = getProperty(node, props[i], buffer + strlen(buffer));
            if (result != ProtocolResult_Ok) {
                serialInterface->writeString(resultToStr(result));
                continue;
            }
        }
        strcat(buffer, "\n");
        serialInterface->writeString(buffer);
    }
    serialInterface->writeString("}\n");
}

ProtocolResult_t ProtocolParser::getProperty(Node *node, const Property_t *prop, char* value) {
    node = (Node*)((int)node - prop->nodeOffset);
    ProtocolResult_t result = ProtocolResult_InternalError;

    switch (prop->type) {
    case PropertyType_Bool:
        result = prop->boolGet(node, (bool*)value);
        if (result == ProtocolResult_Ok) {
            value[0] = value[0] ? '1' : '0';
            value[1] = '\0';
        }
        break;
    case PropertyType_Int32:
        result = prop->intGet(node, (int32_t*)value);
        if (result == ProtocolResult_Ok) {
            sprintf(value, "%ld", *(int32_t*)value);
        }
        break;
    case PropertyType_Uint32:
        result = prop->uintGet(node, (uint32_t*)value);
        if (result == ProtocolResult_Ok) {
            sprintf(value, "%lu", *(uint32_t*)value);
        }
        break;
    case PropertyType_Float32:
        result = prop->floatGet(node, (float32_t*)value);
        if (result == ProtocolResult_Ok) {
            /*https://answers.launchpad.net/gcc-arm-embedded/+question/245658
            http://www1.coocox.org/forum/topic.php?id=346*/
#warning fix float32!!
            sprintf(value, "a%lfb", /**(float32_t*)value*/ 3.14f);
        }
        break;
    case PropertyType_String:
        result = prop->stringGet(node, value);
        break;
    default: value[0] = '\0';
    }
    return result;
}

ProtocolResult_t ProtocolParser::setProperty(Node *node, const Property_t *prop, const char *value) {
    node = (Node*)((int)node - prop->nodeOffset);
    ProtocolResult_t result = ProtocolResult_InternalError;

    switch (prop->type) {
    case PropertyType_Bool:
        result = prop->boolSet(node, value[0] != '0');
        break;
    case PropertyType_Int32:
        int32_t i;
        sscanf(value, "%ld", &i);
        result = prop->intSet(node, i);
        break;
    case PropertyType_Uint32:
        uint32_t j;
        sscanf(value, "%lu", &j);
        result = prop->uintSet(node, j);
        break;
    case PropertyType_Float32:
        float32_t f;
        sscanf(value, "%f", &f);
        result = prop->floatSet(node, f);
        break;
    case PropertyType_String:
        result = prop->stringSet(node, value);
        break;
    case PropertyType_Method:
        result = prop->methodInvoke(node, value);
        break;
    }
    return result;
}

const char* ProtocolParser::resultToStr(ProtocolResult_t result) {
    switch (result) {
    case ProtocolResult_Ok: return "Ok";
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
 * @return true if added to array, false if array already full
 */
bool ProtocolParser::subscribeToNode(Node *node) {
	if (this->subscribedNodeCount == MAX_SUBSCRIBED_NODES) {
		return false;
	}

	this->subscribedNodes[this->subscribedNodeCount] = node;
	this->subscribedNodeCount++;

	return true;
}
