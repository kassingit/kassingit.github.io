GTKMM_CF := $(shell pkg-config gtkmm-4.0 --cflags)
GTKMM_LD := $(shell pkg-config gtkmm-4.0 --libs)
CURL_CF  := $(shell pkg-config libcurl --cflags)
CURL_LD  := $(shell pkg-config libcurl --libs)

ONNXRUNTIME_DIR := /home/kelly/Bureau/Personnal_Projects/ESP32_Monitor/onnxruntime-linux-x64-1.23.0
ONNX_CF := -I$(ONNXRUNTIME_DIR)/include
ONNX_LD := -L$(ONNXRUNTIME_DIR)/lib -lonnxruntime -Wl,-rpath,$(ONNXRUNTIME_DIR)/lib

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Iinclude $(GTKMM_CF) $(CURL_CF) $(ONNX_CF)
LDFLAGS  := $(GTKMM_LD) $(CURL_LD) $(ONNX_LD)

SRC := main.cpp $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)
TARGET := esp32_monitor

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean