/*
 * MemoryNode.cpp
 *
 *  Created on: 2018. jan. 1.
 *      Author: peti
 */

#include "MemoryNode.h"
#include <string.h>

MK_PROP_INT32_RW(MemoryNode, Int, "Integer");
MK_PROP_BOOL_RW(MemoryNode, Bool, "Boolean");
MK_PROP_STRING_RW(MemoryNode, String, "String");
MK_PROP_BINARY_RW(MemoryNode, Binary, "Binary data on 4 bytes");

PROP_ARRAY(props) = {
        PROP_ADDRESS(MemoryNode, Int),
        PROP_ADDRESS(MemoryNode, Bool),
        PROP_ADDRESS(MemoryNode, String),
        PROP_ADDRESS(MemoryNode, Binary)
};

MemoryNode::MemoryNode(): Node("MEMORY", "A node that stores information") {
    NODE_SET_PROPS(props);

    mInt = 0;
    mBool = false;
    string[0] = '\0';
    binary = 0;
}

ProtocolResult_t MemoryNode::getInt(int32_t* dest) const {
    *dest = mInt;
    return ProtocolResult_Ok;
}

ProtocolResult_t MemoryNode::setInt(int32_t value) {
    mInt = value;
    return ProtocolResult_Ok;
}

ProtocolResult_t MemoryNode::getBool(bool* dest) const {
    *dest = mBool;
    return ProtocolResult_Ok;
}
ProtocolResult_t MemoryNode::setBool(bool value) {
    mBool = value;
    return ProtocolResult_Ok;
}

ProtocolResult_t MemoryNode::getString(char* dest) const {
    strcpy(dest, string);
    return ProtocolResult_Ok;
}

ProtocolResult_t MemoryNode::setString(const char* value) {
    if (strlen(value) >= sizeof(string)) {
        return ProtocolResult_InvalidValue;
    }

    strcpy(string, value);
    return ProtocolResult_Ok;
}

ProtocolResult_t MemoryNode::getBinary(const void** dest, uint16_t* length) const {
    *dest = &binary;
    *length = sizeof(binary);
    return ProtocolResult_Ok;
}

ProtocolResult_t MemoryNode::setBinary(const void* value, uint16_t length) {
    if (length != sizeof(binary)) {
        return ProtocolResult_InvalidValue;
    }

    memcpy(&binary, value, length);
    return ProtocolResult_Ok;
}
