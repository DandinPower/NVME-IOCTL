# Variables
CXX=g++
CXXFLAGS=-Wall -Wextra
LDFLAGS=
SOURCES=ioctl_single_thread.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=ioctl_test

# Targets
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

.PHONY: all clean
