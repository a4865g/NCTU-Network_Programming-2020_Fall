CXX=g++
CXX_LIB_PARAM=-L /usr/local/lib
CXX_INCLUDE_PARAM=-I /usr/local/include
CXXFLAGS=-pedantic -std=c++14 -pthread -lboost_system $(CXX_LIB_PARAM) $(CXX_INCLUDE_PARAM) -pthread -lboost_filesystem -lboost_system
CXXFLAGSS=-lws2_32 -lwsock32 -std=c++14
.PHONY: all clean

all:part1 part2

part1:
	$(CXX) http_server.cpp -o http_server $(CXXFLAGS)
	$(CXX) console.cpp -o console.cgi $(CXXFLAGS)

part2:
	$(CXX) cgi_server.cpp -o cgi_server $(CXXFLAGSS)

clean:
	rm -f http_server
	rm -f console.cgi
	rm -f cgi_server.exe
