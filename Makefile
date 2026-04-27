# Compiler settings
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2

# Linker flags (Required for openpty)
LDFLAGS = -lutil

# Executable names
CLIENT_BIN = client
SERVER_BIN = server

# Object files needed for each executable
CLIENT_OBJS = client.o helper.o packet.o
SERVER_OBJS = server.o helper.o packet.o

# Default target to build everything
.PHONY: all clean
all: $(CLIENT_BIN) $(SERVER_BIN)

# Build the client executable
$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Build the server executable (linking -lutil)
$(SERVER_BIN): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files with header dependencies
client.o: client.cpp helper.h packet.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

server.o: server.cpp helper.h packet.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

helper.o: helper.cpp helper.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

packet.o: packet.cpp packet.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f *.o $(CLIENT_BIN) $(SERVER_BIN)