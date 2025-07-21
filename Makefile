# Makefile

CXX = clang++
#Add a -g to run in debug mode.
CXXFLAGS = -std=c++20 -Wall -Wextra `pkg-config --cflags glfw3` -I$(VULKAN_SDK)/include
LDFLAGS = `pkg-config --libs glfw3` -lvulkan

TARGET = AcceleRender
SRCS = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

FORMAT_EXTENSIONS := *.cpp *.h
FORMAT_DIR := src
CLANG_FORMAT := clang-format
FORMAT_STYLE := file

format:
	find $(FORMAT_DIR) -type f \( -name "*.cpp" -o -name "*.h" \) -exec $(CLANG_FORMAT) -i -style=$(FORMAT_STYLE) {} +

.PHONY: format

