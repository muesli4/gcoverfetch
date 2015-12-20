CXX      = g++

LIBS     = libglyr taglib gtkmm-3.0

CXXFLAGS = '-std=c++11' '-std=gnu++11' '-Wall' `pkg-config --cflags $(LIBS)` -I/usr/include/boost -DBOOST_SYSTEM_NO_DEPRECATED
LDFLAGS  = `pkg-config --libs $(LIBS)` -lboost_filesystem -lboost_system

OBJ = main.o

main: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJ) -o main

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $<

