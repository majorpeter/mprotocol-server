/*
 * TestNode.cpp
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#include "TestNode.h"

MK_PROP_FLOAT32_RO(TestNode, Pi, "Math PI");
MK_PROP_METHOD(TestNode, Void, "Do nothing");
MK_PROP_METHOD(TestNode, IntErrorMethod, "Returns internal error");

PROP_ARRAY(props) = {
        PROP_ADDRESS(TestNode, Pi),
        PROP_ADDRESS(TestNode, Void),
        PROP_ADDRESS(TestNode, IntErrorMethod)
};

TestNode::TestNode(): Node("TEST", "Test node") {
    NODE_SET_PROPS(props);
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
