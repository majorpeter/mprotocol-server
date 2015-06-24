#include "ProtocolParser.h"
#include "Nodes/RootNode.h"
#include <stdio.h>
#include <string.h>

ProtocolParser::ProtocolParser(AbstractSerialInterface* serialInterface): serialInterface(serialInterface) {

}

void ProtocolParser::listen() {
    serialInterface->listen();
}

void ProtocolParser::receiveString(char* s) {
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
        reportError(ProtocolResult_UnknownFunc);
        return;
    }

    // decode target node
    Node* node = NULL;
    char *p;
    if (s[0] != '/') {
        reportError(ProtocolResult_NodeNotFound);
        return;
    }
    node = RootNode::getInstance();
    s++;
    for (;;) {
        p = strchr(s, '/');
        if (p != NULL) {
            *p ='\0';
            node = node->getChildByName(s);
            if (node == NULL) {
                reportError(ProtocolResult_NodeNotFound);
                return;
            }
            s = p+1;
        } else {
            p = strchr(s, '.');
            if (p != NULL) {

            } else {
                reportError(ProtocolResult_SyntaxError);
                return;
            }
        }
    }
}

void ProtocolParser::reportError(ProtocolResult_t errorCode) {
    char buf[4];
    snprintf(buf, 4, "E%d:", errorCode);

    //TODO ITM
    fputs(buf, stdout);
    fputs(resultToStr(errorCode), stdout);
    fputs("\n", stdout);
}

const char* ProtocolParser::resultToStr(ProtocolResult_t result) {
    switch (result) {
    case ProtocolResult_Ok: return "Ok";
    case ProtocolResult_UnknownFunc: return "Unknown function";
    case ProtocolResult_NodeNotFound: return "Node not found";
    case ProtocolResult_SyntaxError: return "Syntax Error";
    }
    return "Unknown Error code!";
}
