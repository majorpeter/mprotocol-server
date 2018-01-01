MODULES_DIR = ../..
SERVER_DIR = ..

INC_FOLDERS = $(MODULES_DIR) $(SERVER_DIR)
INC_FLAGS = $(addprefix -I,$(INC_FOLDERS))
CXX_FLAGS = -g3 -Og

SRC := test.cpp \
       ProtocolTester.cpp \
       BufferInterface.cpp \
       TestNode.cpp \
       MemoryNode.cpp \
       $(MODULES_DIR)/mprotocol-nodes/Node.cpp \
       $(MODULES_DIR)/mprotocol-nodes/Property.cpp \
       $(MODULES_DIR)/mprotocol-nodes/RootNode.cpp \
       $(SERVER_DIR)/ProtocolParser.cpp \
       $(SERVER_DIR)/utils.cpp \
       $(SERVER_DIR)/AbstractSerialInterface.cpp \
     
OBJ := $(SRC:%.cpp=%.o)

TARGET = test

$(TARGET): $(OBJ)
	@echo $(OBJ)
	g++ $(OBJ) $(CXX_FLAGS) -o $@

%.o: %.cpp
	g++ $(INC_FLAGS) $(CXX_FLAGS) $< -c -o $@

.PHONY: all clean

all: $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
