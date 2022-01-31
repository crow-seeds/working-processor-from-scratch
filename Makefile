CXX = g++
CXXFLAGS= -g -Wall -std=c++11 
OPTFLAGS= -O3

EXE_NAME=processor
SRCS := main.cpp processor.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(EXE_NAME)

$(EXE_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	$(RM) $(EXE_NAME) $(OBJS)