/*
 * MemoryNode.cpp
 *
 *  Created on: 2018. jan. 1.
 *      Author: peti
 */

#include "MemoryNode.h"

MK_PROP_INT32_RW(MemoryNode, Int, "Integer");

PROP_ARRAY(props) = {
        PROP_ADDRESS(MemoryNode, Int),
};

MemoryNode::MemoryNode(): Node("MEMORY", "A node that stores information") {
    NODE_SET_PROPS(props);

    mInt = 0;
}

ProtocolResult_t MemoryNode::getInt(int32_t* dest) const {
    *dest = mInt;
    return ProtocolResult_Ok;
}

ProtocolResult_t MemoryNode::setInt(int32_t value) {
    mInt = value;
    return ProtocolResult_Ok;
}
