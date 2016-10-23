HEADERS=sh4.hpp Memory.hpp BaseException.hpp types.hpp Icache.hpp Ocache.hpp
OBJECTS=sh4.o Memory.o Icache.o Ocache.o
CXXFLAGS=-I../../common/ -I ../../../external/ --std=c++03 -Wall -Werror -g

libsh4.a: $(OBJECTS)
	ar rcs libsh4.a $(OBJECTS)

%.o: %.cpp $(HEADERS)
	$(CXX) -c $< $(CXXFLAGS)

clean:
	for obj in $(OBJECTS) ; do if [ -f $$obj ] ; then rm $$obj ; fi ; done
	if [ -f libsh4.a ] ; then rm libsh4.a ; fi
