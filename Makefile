# Makefile

CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra `pkg-config --cflags glfw3` -I$(VULKAN_SDK)/include
LDFLAGS = `pkg-config --libs glfw3` -lvulkan

TARGET = AcceleRender
SRCS = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

