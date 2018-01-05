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
    DECLARE_PROP_BINARY_RW(Binary);
    DECLARE_PROP_BINARY_SEGMENTED_RW(BinarySeg, MemoryNode);

    MemoryNode();
private:
    int32_t mInt;
    bool mBool;
    char string[10];
    uint32_t binary;

    static const uint16_t binarySegBufferSize = 512;
    uint8_t binarySegBuffer[binarySegBufferSize];
    uint16_t binarySegBufferPos;
};

#endif /* MODULES_MPROTOCOL_SERVER_TEST_MEMORYNODE_H_ */
