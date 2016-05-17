
CC       = gcc
CXX      = g++
CFLAGS   = -std=c++11
LDFLAGS  =
LDLIBS   = -lbcm2835
INCLUDES = -I../vendor/cppzmq
RM       = rm -rf

all : spi-max31865

spi-max31865 : spi.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS) 

%.o : %.cpp
	$(CXX) -c $(CFLAGS) $(INCLUDES) -o $@ $<

clean :
	$(RM) spi.o spi-max31865
