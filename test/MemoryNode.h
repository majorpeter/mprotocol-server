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
    DECLARE_PROP_STRING_RW(String);
    MemoryNode();
private:
    int32_t mInt;
    bool mBool;
    char string[10];
};

#endif /* MODULES_MPROTOCOL_SERVER_TEST_MEMORYNODE_H_ */
