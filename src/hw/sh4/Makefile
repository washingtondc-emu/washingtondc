HEADERS=sh4.hpp Memory.hpp BaseException.hpp types.hpp Icache.hpp Ocache.hpp
OBJECTS=sh4.o Memory.o main.o Icache.o Ocache.o
CXXFLAGS=-I../../common/ -I ../../../external/ --std=c++03 -Wall -Werror -g

sh4_test: $(OBJECTS)
	$(CXX) -o sh4_test $+

%.o: %.cpp $(HEADERS)
	$(CXX) -c $< $(CXXFLAGS)

test: sh4_test
	./sh4_test

clean:
	rm $(OBJECTS)
	rm sh4_stub
