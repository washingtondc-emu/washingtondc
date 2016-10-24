SRC_DIR=../src
SH4_DIR=$(SRC_DIR)/hw/sh4
OBJECTS=sh4_test.o
CXXFLAGS=-I../src/common/ -I../external/ -I$(SRC_DIR) --std=c++03 -Wall \
-Werror -g
LDFLAGS=-L$(SRC_DIR)/hw/sh4/ -lsh4

all: sh4_test

test: sh4_test
	./sh4_test

sh4_test: $(OBJECTS) $(HEADERS)
	make -C $(SH4_DIR)
	$(CXX) -o sh4_test $(CXXFLAGS) $+ $(LDFLAGS)

clean:
	for obj in $(OBJECTS) ; do if [ -f $$obj ] ; then rm $$obj ; fi ; done
