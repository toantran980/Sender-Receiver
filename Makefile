CC       = gcc
CXX      = g++
CFLAGS   = -Wall -Wextra -g
CXXFLAGS = -Wall -Wextra -g
TARGETS  = sender recv send_c recv_c

.PHONY: all clean

all: $(TARGETS)

# C++ targets
sender: sender.o
	$(CXX) $(CXXFLAGS) $< -o $@

recv: recv.o
	$(CXX) $(CXXFLAGS) $< -o $@

sender.o: sender.cpp msg.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

recv.o: recv.cpp msg.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# C targets
send_c: send_c.o
	$(CC) $(CFLAGS) $< -o $@

recv_c: recv_c.o
	$(CC) $(CFLAGS) $< -o $@

send_c.o: send_c.c msg.h
	$(CC) $(CFLAGS) -c $< -o $@

recv_c.o: recv_c.c msg.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o $(TARGETS)
