/*
 * TestNode.cpp
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#include "TestNode.h"
#include <limits.h>

MK_PROP_UINT32_RO(TestNode, UintMax, "Maximal value of uint32_t");
MK_PROP_INT32_RO(TestNode, IntMin, "Minimal value of int32_t");
MK_PROP_FLOAT32_RO(TestNode, Pi, "Math PI");
MK_PROP_METHOD(TestNode, Void, "Do nothing");
MK_PROP_METHOD(TestNode, IntErrorMethod, "Returns internal error");

PROP_ARRAY(props) = {
        PROP_ADDRESS(TestNode, UintMax),
        PROP_ADDRESS(TestNode, IntMin),
        PROP_ADDRESS(TestNode, Pi),
        PROP_ADDRESS(TestNode, Void),
        PROP_ADDRESS(TestNode, IntErrorMethod)
};

TestNode::TestNode(): Node("TEST", "Test node") {
    NODE_SET_PROPS(props);
}

ProtocolResult_t TestNode::getUintMax(uint32_t* dest) const {
    *dest = UINT32_MAX;
    return ProtocolResult_Ok;
}

ProtocolResult_t TestNode::getIntMin(int32_t* dest) const {
    *dest = INT32_MIN;
    return ProtocolResult_Ok;
}

ProtocolResult_t TestNode::getPi(float* dest) const {
    *dest = 3.14f;
    return ProtocolResult_Ok;
}

ProtocolResult_t TestNode::invokeVoid(const char*) {
    return ProtocolResult_Ok;
}

ProtocolResult_t TestNode::invokeIntErrorMethod(const char*) {
    return ProtocolResult_InternalError;
}
