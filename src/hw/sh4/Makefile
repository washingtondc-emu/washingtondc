HEADERS=sh4.hpp Memory.hpp BaseException.hpp types.hpp Icache.hpp Ocache.hpp
OBJECTS=sh4.o Memory.o main.o Icache.o Ocache.o
CXXFLAGS=-I../../common/ -I ../../../external/boost --std=c++03 -Wall -Werror -g

sh4_stub: $(OBJECTS)
	g++ -o sh4_stub $+

%.o: %.cpp $(HEADERS)
	g++ -c $< $(CXXFLAGS)

clean:
	rm $(OBJECTS)
	rm sh4_stub
