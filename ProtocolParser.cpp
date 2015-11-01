#include "ProtocolParser.h"
#include "Nodes/RootNode.h"
#include "Log/Log.h"
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
				subscribedNodes[i]->printPathRecursively(serialInterface);
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
		reportResult(result);
	}

	Log::getInstance()->hanlder();
	serialInterface->handler();
}

ProtocolResult_t ProtocolParser::parseString(char *s) {
    // decode protocol function
    enum {UNKNOWN, GET, SET, CALL, MAN} decodedFunc = UNKNOWN;

    if (strncmp("GET ", s, 4) == 0) {
        decodedFunc = GET;
        s += 4;
    } else if (strncmp("SET ", s, 4) == 0) {
        decodedFunc = SET;
        s += 4;
    } else if (strncmp("CALL ", s, 5) == 0) {
        decodedFunc = CALL;
        s += 5;
	} else if (strncmp("MAN ", s, 4) == 0) {
		decodedFunc = MAN;
		s += 4;
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
        return this->listProperty(node, prop);
    case SET:
        if (prop->accessLevel != PropAccessLevel_ReadWrite) {
            return ProtocolResult_InvalidFunc;
        }
        result = setProperty(node, prop, s);
        this->reportResult(result);
        return result;
    case CALL:
        if (prop->type != PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        result = setProperty(node, prop, s);
        this->reportResult(result);
        return result;
    case MAN:
    	this->writeManual(node, prop);
    	return ProtocolResult_Ok;
    }


    //return ProtocolResult_UnknownFunc;
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
	serialInterface->writeString(property->description);
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

ProtocolResult_t ProtocolParser::listProperty(const Node *node, const Property_t *prop) {
	ProtocolResult_t result = ProtocolResult_InternalError;
	char buffer[256];
	char *s = buffer;
	if (prop->accessLevel == PropAccessLevel_ReadWrite) {
		strcpy(s, "PW_");
		s+=3;
	} else {
		strcpy(s, "P_");
		s+=2;
	}
	strcpy(s, Property_TypeToStr(prop->type));
	strcat(s, " ");
	strcat(s, prop->name);
	s += strlen(s);
	if (prop->accessLevel != PropAccessLevel_Invokable) {
		*s = '=';
		s++;

		result = getProperty(node, prop, s);
		if (result != ProtocolResult_Ok) {
			serialInterface->writeString(resultToStr(result));
		}
	}
	strcat(s, "\n");
	serialInterface->writeString(buffer);
	return result;
}

ProtocolResult_t ProtocolParser::getProperty(const Node *node, const Property_t *prop, char* value) {
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
        	/*
        	 * this routine prints the float number with 4 frac. digits precision
        	 * the printf("%f") did not work with this clib.
        	 */
        	float f = *(float32_t*)value;	// make a local copy
        	int pos = 0;					// position in result string

        	// print '-' if negative
        	if (f < 0.f) {
        		value[0] = '-';
        		pos = 1;
        		f = -f;
        	}

        	// print integer part and substract it
        	pos = sprintf(value + pos, "%d", (int) f);
            f -= (float) (int) f;

            // print decimal point if the fractional part is big enough
            if (f > 0.0001f) {
            	value[pos] = '.';
            	pos++;
            }

            // print and substract the first 4 fractional digits
            uint8_t fracDigits = 0;
            while (f > 0.0001f) {
            	// get the next digit left from the decimal point
            	f *= 10;
            	// this is a lot faster than sprintf for 1 digit
            	value[pos] = (char) ('0' + (int) f);
            	// clear integer part
            	f -= (float) (int) f;

            	pos++;
            	fracDigits++;
            	if (fracDigits == 4) {
            		break;
            	}
            }
            // always close the string
            value[pos] = '\0';
        }
        break;
    case PropertyType_String:
        result = prop->stringGet(node, value);
        break;
    case PropertyType_Binary:
    	result = this->getBinaryProperty(node, prop, value);
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
    case PropertyType_Binary:
    	return ProtocolResult_InvalidFunc;	//TODO implement bin setter
    }
    return result;
}

ProtocolResult_t ProtocolParser::getBinaryProperty(const Node *node, const Property_t *prop, char* value) {
	uint8_t *ptr = NULL;
	uint16_t length = 0;
	ProtocolResult_t result = prop->binaryGet(node, (void**) &ptr, &length);
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
