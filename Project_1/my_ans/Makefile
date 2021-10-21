CXX=g++
CXXFLAGS=-g -std=c++17 -flto -Os
SRC=$(wildcard *.cpp)
BIN=npshell

all:
	$(CXX) -o $(BIN) $(SRC) $(CXXFLAGS)

clean:
	rm $(BIN)

run:
	./$(BIN)
