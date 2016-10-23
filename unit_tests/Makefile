SH4_DIR=../src/hw/sh4
OBJECTS=sh4_test.o
CXXFLAGS=-I../src/common/ -I ../external/ -L../src/hw/sh4/ -lsh4 --std=c++03 \
-Wall -Werror -g

all: sh4_test

test: sh4_test
	./sh4_test

sh4_test: $(OBJECTS) $(HEADERS)
	make -C $(SH4_DIR)
	$(CXX) -o sh4_test $(CXXFLAGS) $+

clean:
	for obj in $(OBJECTS) ; do if [ -f $$obj ] ; then rm $$obj ; fi ; done
