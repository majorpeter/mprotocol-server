/*
 * TestNode.cpp
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#include "TestNode.h"

MK_PROP_FLOAT32_RO(TestNode, Pi, "Math PI");

PROP_ARRAY(props) = {
        PROP_ADDRESS(TestNode, Pi)
};

TestNode::TestNode(): Node("TEST", "Test node") {
    NODE_SET_PROPS(props);
}

ProtocolResult_t TestNode::getPi(float* dest) const {
    *dest = 3.14f;
    return ProtocolResult_Ok;
}
