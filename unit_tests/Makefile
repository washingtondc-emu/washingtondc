SRC_DIR=../src
SH4_DIR=$(SRC_DIR)/hw/sh4
OBJECTS=sh4_test.o
CXXFLAGS=-I../src/common/ -I../external/ -I$(SRC_DIR) --std=c++03 -Wall \
-Werror -g
LDFLAGS=-L$(SRC_DIR)/hw/sh4/ -lsh4

all: sh4_test

test: sh4_test
	./sh4_test

sh4_test: $(OBJECTS) $(HEADERS) sh4_lib
	$(CXX) -o sh4_test $(CXXFLAGS) $(OBJECTS) $(LDFLAGS)

clean:
	for obj in $(OBJECTS) ; do if [ -f $$obj ] ; then rm $$obj ; fi ; done

# pseudo-target to go build libsh4.a in SH4_DIR
sh4_lib:
	make -C $(SH4_DIR)
