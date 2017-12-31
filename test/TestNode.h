/*
 * TestNode.h
 *
 *  Created on: 2017. dec. 31.
 *      Author: peti
 */

#ifndef MODULES_MPROTOCOL_SERVER_TEST_TESTNODE_H_
#define MODULES_MPROTOCOL_SERVER_TEST_TESTNODE_H_

#include "mprotocol-nodes/Node.h"

class TestNode: public Node {
public:
    DECLARE_PROP_FLOAT32_RO(Pi);
    DECLARE_PROP_METHOD(Void);
    DECLARE_PROP_METHOD(IntErrorMethod);

    TestNode();
};

#endif /* MODULES_MPROTOCOL_SERVER_TEST_TESTNODE_H_ */
