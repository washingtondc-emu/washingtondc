HEADERS=sh4asm.hpp
OBJECTS=sh4asm.o
CXXFLAGS=-I../../common/ -I ../../../external/ -I ../../hw/sh4/ \
--std=c++03 -Wall -Werror -g

sh4asm: main.o libsh4asm.a
	g++ -o sh4asm main.o -lsh4asm -L. -g

libsh4asm.a: $(OBJECTS)
	ar rcs libsh4asm.a $(OBJECTS)

%.o: %.cpp $(HEADERS)
	$(CXX) -c $< $(CXXFLAGS)

clean:
	for obj in $(OBJECTS) ; do if [ -f $$obj ] ; then rm $$obj ; fi ; done
	if [ -f libsh4asm.a ] ; then rm libsh4asm.a ; fi
	if [ -f main.o ]; then rm main.o ; fi
