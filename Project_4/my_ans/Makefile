CXX=g++
CXX_LIB_PARAM=-L /usr/local/lib
CXX_INCLUDE_PARAM=-I /usr/local/include
CXXFLAGS=-pedantic -std=c++14 -pthread -lboost_system $(CXX_LIB_PARAM) $(CXX_INCLUDE_PARAM) -pthread -lboost_filesystem -lboost_system
CXXFLAGSS=-lws2_32 -lwsock32 -std=c++14
.PHONY: all clean

all:
	$(CXX) socks_server.cpp -o socks_server $(CXXFLAGS)
	$(CXX) console.cpp -o hw4.cgi $(CXXFLAGS)

clean:
	rm -f socks_server
	rm -f hw4.cgi
