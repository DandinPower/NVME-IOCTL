CXX = g++
CXXFLAGS = -std=c++11
LDFLAGS = -lstdc++ -laio
 
SRCS = main.cpp layer_manager.cpp nvme-ioctl.cpp nvme-aio.cpp
OBJS = $(SRCS:.cpp=.o)
 
TARGET = nvme_api
 
all: $(TARGET)
 
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)
 
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
 
clean:
	rm -f $(OBJS) $(TARGET)