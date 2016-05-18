
CC       = gcc
CXX      = g++
CXXFLAGS = -std=c++11
LDFLAGS  =
LDLIBS   = -lbcm2835 `pkg-config --libs libzmq`
INCLUDES = -I../vendor/cppzmq
RM       = rm -rf

all : max31865-spi

max31865-spi : max31865-spi.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS) 

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) -o $@ $<

clean :
	$(RM) max31865-spi.o max31865-spi
