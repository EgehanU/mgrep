CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TARGET   = mgrep
SRCS     = main.cpp searcher.cpp
OBJS     = $(SRCS:.cpp=.o)

# debug build - adds sanitizers, slower but catches bugs
# usage: make debug
DEBUG_FLAGS = -g -O0 -fsanitize=address,thread -fno-omit-frame-pointer

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

debug:
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(TARGET)_debug $(SRCS)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET)_debug
