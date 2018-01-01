/*
 * MemoryNode.h
 *
 *  Created on: 2018. jan. 1.
 *      Author: peti
 */

#ifndef MODULES_MPROTOCOL_SERVER_TEST_MEMORYNODE_H_
#define MODULES_MPROTOCOL_SERVER_TEST_MEMORYNODE_H_

#include "mprotocol-nodes/Node.h"

class MemoryNode: public Node {
public:
    DECLARE_PROP_INT32_RW(Int);
    DECLARE_PROP_BOOL_RW(Bool);
    MemoryNode();
private:
    int32_t mInt;
    bool mBool;
};

#endif /* MODULES_MPROTOCOL_SERVER_TEST_MEMORYNODE_H_ */
