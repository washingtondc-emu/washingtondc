SRC_DIR=../src
SH4_DIR=$(SRC_DIR)/hw/sh4
SH4ASM_DIR = $(SRC_DIR)/tool/sh4asm
OBJECTS=sh4_test.o
CXXFLAGS=-I../src/common/ -I$(SH4_DIR) -I../external/ -I$(SRC_DIR) \
--std=c++03 -Wall -Werror -g
LDFLAGS=-L$(SRC_DIR)/hw/sh4/ -lsh4

all: sh4_test sh4asm_test

test: sh4_test sh4asm_test
	./sh4_test
	./sh4asm_test

sh4_test: sh4_test.cpp $(HEADERS) sh4_lib
	$(CXX) -o sh4_test $(CXXFLAGS) sh4_test.cpp $(LDFLAGS)

sh4asm_test: sh4asm_test.cpp $(HEADERS) sh4asm_lib
	$(CXX) -o sh4asm_test $(CXXFLAGS) sh4asm_test.cpp -L$(SRC_DIR)/tool/sh4asm -lsh4asm

clean:
	for obj in $(OBJECTS) ; do if [ -f $$obj ] ; then rm $$obj ; fi ; done

# pseudo-target to go build libsh4.a in SH4_DIR
sh4_lib:
	make -C $(SH4_DIR)

sh4asm_lib:
	make -C $(SH4ASM_DIR)
