/*
 * test.cpp
 *
 *  Created on: 2017. dec. 30.
 *      Author: peti
 */

#include "ProtocolTester.h"
#include "TestNode.h"

#include "mprotocol-nodes/RootNode.h"

#include <stdio.h>
#include <string.h>

#include <iostream>

class EmptyNode: public Node {
public:
    EmptyNode(): Node("EMPTY", "Empty node") {}
};

int main() {
    ProtocolTester tester;
    RootNode::getInstance()->addChild(new EmptyNode());
    RootNode::getInstance()->addChild(new TestNode());

    tester.test("GET /\r\n", "{\nN EMPTY\nN TEST\n}\n");
    tester.test("GET /EMPTY\r\n", "{\n}\n");
    tester.test("MAN /EMPTY\r\n", "MAN Empty node\n");

    tester.test("GET /NONEXISTENT\r\n", "E3:Node not found\n");

    tester.test("GET /TEST\r\n", "{\nP_FLOAT32 Pi=3.14\nP_METHOD Void\nP_METHOD IntErrorMethod\n}\n");
    tester.test("MAN /TEST\r\n", "MAN Test node\n");

    tester.test("GET /TEST.Pi\n", "P_FLOAT32 Pi=3.14\n");
    tester.test("MAN /TEST.Pi\n", "MAN Math PI\n");

    tester.test("CALL /TEST.Void\n", "E0:Ok\n");
    tester.test("CALL /TEST.IntErrorMethod\n", "E8:Internal error\n");

    tester.test("GET /TEST.NonExistent\r\n", "E4:Property not found\n");
    tester.test("CALL /TEST.NonExistent\r\n", "E4:Property not found\n");

    tester.printResults();

    return 0;
}
