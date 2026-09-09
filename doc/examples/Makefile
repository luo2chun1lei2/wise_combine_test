CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -fPIC

all: libconn.so

libconn.so: libconn.cpp
	$(CXX) $(CXXFLAGS) -shared libconn.cpp -o libconn.so

clean:
	rm -f libconn.so
