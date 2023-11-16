# Variables
CXX=g++
CXXFLAGS=-Wall -Wextra
LDFLAGS= -lpthread
SOURCES= ioctl_thread_pool.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=ioctl_thread_pool

# Targets
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

.PHONY: all clean
