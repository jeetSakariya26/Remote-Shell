CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
LDFLAGS  = -lpthread

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

client: client.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f pty_server pty_client