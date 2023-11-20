# Variables
CXX=g++
CXXFLAGS=-Wall -Wextra
INCLUDES= -I src
LDFLAGS=
SOURCES=ioctl_block.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=ioctl_block

# Targets
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

.PHONY: all clean
