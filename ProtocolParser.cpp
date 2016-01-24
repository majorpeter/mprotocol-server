#include "ProtocolParser.h"
#include "Nodes/RootNode.h"
#include "Log/Log.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <Scheduler/Scheduler.h>

#define RX_BUFFER_SIZE 256
#define MAX_SUBSCRIBED_NODES 15

ProtocolParser* ProtocolParser::instance = NULL;

ProtocolParser::ProtocolParser(AbstractSerialInterface* serialInterface): serialInterface(serialInterface) {
	rxBuffer = new char[RX_BUFFER_SIZE];
	rxPosition = 0;

	subscribedNodes = new Node*[MAX_SUBSCRIBED_NODES];
	subscribedNodeCount = 0;

	if (instance != NULL) {
		exit(1);	// kill application if invoked twice
	}
	instance = this;
	serialInterface->setUpLayer(this);
	Timeout::set(50,ProtocolParser::periodicHandle,TIMEOUT_MODE_NON_BLOCKING|TIMEOUT_MODE_AUTORESTART);
}

void ProtocolParser::periodicHandle() {
	ProtocolParser::getExistingInstance()->handleSubscriptions();
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
	if (rxPosition + len > RX_BUFFER_SIZE) {
		return false;
	}
	memcpy(rxBuffer + rxPosition, bytes, len);
	rxPosition += len;
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

		while (mask != 0) {
			// is LSB a changed property
			if (mask & 0x01) {
				serialInterface->writeString("CHG ");
				subscribedNodes[i]->printPathRecursively(serialInterface);
				serialInterface->writeBytes((uint8_t*) ".", 1);
				serialInterface->writeString(subscribedNodes[i]->getProperties()[propIndex]->name);
				serialInterface->writeBytes((uint8_t*) "=", 1);
				char buffer[256];	//TODO buffer size? maybe a common buffer?
				getProperty(subscribedNodes[i], subscribedNodes[i]->getProperties()[propIndex], buffer);	//TODO print instead
				*serialInterface << buffer;
				serialInterface->writeBytes((uint8_t*) "\n", 1);
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
		ProtocolResult_t result = parseString(&rxBuffer[rxLineStart]);
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
}

void ProtocolParser::handler() {
	//this->handleSubscriptions();
	this->handleReceivedCommands();

	Log::getInstance()->handler(serialInterface);
	serialInterface->handler();
}

ProtocolResult_t ProtocolParser::parseString(char *s) {
    ProtocolResult_t result = ProtocolResult_InternalError;

    // decode protocol function
    enum {UNKNOWN, GET, SET, CALL, OPEN, CLOSE, MAN} decodedFunc = UNKNOWN;

    char *p = strchr(s, '\n');
    if (p != NULL) {
    	*p = '\0';
    }

    if (strncmp("GET ", s, 4) == 0) {
        decodedFunc = GET;
        s += 4;
    } else if (strncmp("SET ", s, 4) == 0) {
        decodedFunc = SET;
        s += 4;
    } else if (strncmp("CALL ", s, 5) == 0) {
        decodedFunc = CALL;
        s += 5;
    } else if (strncmp("OPEN ", s, 5) == 0) {
        decodedFunc = OPEN;
        s += 5;
    } else if (strncmp("CLOSE ", s, 6) == 0) {
        decodedFunc = CLOSE;
        s += 6;
	} else if (strncmp("MAN ", s, 4) == 0) {
		decodedFunc = MAN;
		s += 4;
	}

    if (decodedFunc == UNKNOWN) {
        return ProtocolResult_UnknownFunc;
    }

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
        case GET:
            listNode(node);
            return ProtocolResult_Ok;
        case OPEN:
        	result = addNodeToSubscribed(node);
            if (result == ProtocolResult_Ok) {
                this->reportResult(ProtocolResult_Ok);
            }
            return result;
        case CLOSE:
        	result = removeNodeFromSubscribed(node);
            if (result == ProtocolResult_Ok) {
                this->reportResult(ProtocolResult_Ok);
            }
            return result;
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
        if (result == ProtocolResult_Ok) {
        	this->reportResult(ProtocolResult_Ok);
        }
        return result;
    case CALL:
        if (prop->type != PropertyType_Method) {
            return ProtocolResult_InvalidFunc;
        }
        result = setProperty(node, prop, s);
        if (result == ProtocolResult_Ok) {
        	this->reportResult(ProtocolResult_Ok);
        }
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
		*s = '\0';	// just to make sure

		if (prop->type == PropertyType_Binary) {
			uint8_t *ptr = NULL;
			uint16_t length = 0;
			result = prop->binaryGet(node, (void**) &ptr, &length);
			if (result == ProtocolResult_Ok) {
				serialInterface->writeString(buffer);
				while (length > 0) {
					char buf[3];
					sprintf(buf, "%02X", *ptr);
					serialInterface->writeBytes((uint8_t*) buf, 2);
					ptr++;
					length--;
				}
				serialInterface->writeBytes((uint8_t*) "\n", 1);
			}
			return result;
		}
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
            value[0] = (*(bool*)value) ? '1' : '0';
            value[1] = '\0';
        }
        break;
    case PropertyType_Int32:
        result = prop->intGet(node, (int32_t*)value);
        if (result == ProtocolResult_Ok) {
            sprintf(value, "%ld", (long int) *(int32_t*)value);
        }
        break;
    case PropertyType_Uint32:
        result = prop->uintGet(node, (uint32_t*)value);
        if (result == ProtocolResult_Ok) {
            sprintf(value, "%lu", (long unsigned int) *(uint32_t*)value);
        }
        break;
    case PropertyType_Float32: {
    	float f;
        result = prop->floatGet(node, &f);
        if (result == ProtocolResult_Ok) {
        	/*
        	 * this routine prints the float number with 4 frac. digits precision
        	 * the printf("%f") did not work with this clib.
        	 */

        	// print '-' if negative
        	if (f < 0.f) {
        		*value = '-';
        		value++;
        		f = -f;
        	}

        	// print integer part and substract it
        	value += sprintf(value, "%0d", (int) f);
            f -= (float) (int) f;

            // print decimal point if the fractional part is big enough
            if (f > 0.0001f) {
            	*value = '.';
            	value++;
            }

            // print and substract the first 4 fractional digits
            uint8_t fracDigits = 0;
            while (f > 0.0001f) {
            	// get the next digit left from the decimal point
            	f *= 10;
            	// this is a lot faster than sprintf for 1 digit
            	*value = (char) ('0' + (int) f);
            	// clear integer part
            	f -= (float) (int) f;

            	value++;
            	fracDigits++;
            	if (fracDigits == 4) {
            		break;
            	}
            }
            // always close the string
            *value = '\0';
        }
        break;
    }
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
    node = (Node*)((size_t)node - prop->nodeOffset);
    ProtocolResult_t result = ProtocolResult_InternalError;

    switch (prop->type) {
    case PropertyType_Bool:
        result = prop->boolSet(node, value[0] != '0');
        break;
    case PropertyType_Int32: {
        long int i;
        sscanf(value, "%ld", &i);
        result = prop->intSet(node, i);
        break;
    }
    case PropertyType_Uint32:
        unsigned long int j;
        sscanf(value, "%lu", &j);
        result = prop->uintSet(node, j);
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
        	const char *p = strchr(value, '.') + 1;	// cannot be NULL, because sscanf found it
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
        result = prop->floatSet(node, f);
        break;
    }
    case PropertyType_String:
        result = prop->stringSet(node, value);
        break;
    case PropertyType_Method:
        result = prop->methodInvoke(node, value);
        break;
    case PropertyType_Binary: {
    	size_t length = strlen(value);
    	if (length & 0x01) {
    		return ProtocolResult_InvalidValue;
    	}
    	uint8_t data[length / 2];
    	if (!this->binaryStringToArray(value, data)) {
			return  ProtocolResult_InvalidValue;
    	}
		result = prop->binarySet(node, data, length / 2);
    }
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

uint8_t ProtocolParser::charToByte(char c) {
	if (c < '0') return 0xff;
	if (c <= '9') {
		return c - '0';
	}
	if (c < 'A') {
		return 0xff;
	}
	if (c <= 'F') {
		return c - 'A' + 0x0a;
	}
	return 0xff;
}

/**
 * parses hex string
 * @return true if the string was valid
 */
bool ProtocolParser::binaryStringToArray(const char* from, uint8_t* to) {
	while (from[0] != '\0') {
		uint8_t val0 = charToByte(from[0]);
		uint8_t val1 = charToByte(from[1]);
		if ((val0 == 0xff) || (val1 == 0xff)) {
			return false;
		}
		*to = (val0 << 4) | val1;
		from += 2;
		to++;
	}
	return true;
}
