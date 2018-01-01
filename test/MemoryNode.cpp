/*
 * MemoryNode.cpp
 *
 *  Created on: 2018. jan. 1.
 *      Author: peti
 */

#include "MemoryNode.h"

MK_PROP_INT32_RW(MemoryNode, Int, "Integer");
MK_PROP_BOOL_RW(MemoryNode, Bool, "Boolean");

PROP_ARRAY(props) = {
        PROP_ADDRESS(MemoryNode, Int),
        PROP_ADDRESS(MemoryNode, Bool),
};

MemoryNode::MemoryNode(): Node("MEMORY", "A node that stores information") {
    NODE_SET_PROPS(props);

    mInt = 0;
    mBool = false;
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
