# Development build with MinGW-w64. Not used for releases: its static C++
# runtime costs about 500 KB, which is why build.bat (MSVC /MT) ships instead.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LDFLAGS  ?= -static -static-libgcc -static-libstdc++
LIBS      = -lsetupapi -lhid

SRC = src/main.cpp src/hid.cpp src/hidpp.cpp src/bt.cpp src/devices.cpp src/text.cpp \
      src/providers/logitech.cpp src/providers/bluetooth.cpp src/providers/generic.cpp \
      src/providers/table.cpp

APP  = build/battery_status.exe
TEST = build/frame_test.exe

ifeq ($(OS),Windows_NT)
MKDIR = if not exist build mkdir build
else
MKDIR = mkdir -p build
endif

all: $(APP)

$(APP): $(SRC)
	@$(MKDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) $(LDFLAGS) $(LIBS)

$(TEST): tests/frame_test.cpp src/hidpp.cpp
	@$(MKDIR)
	$(CXX) $(CXXFLAGS) -Isrc -o $@ tests/frame_test.cpp src/hidpp.cpp $(LDFLAGS)

test: $(TEST)
	@build/frame_test.exe

clean:
	@if exist build rmdir /s /q build

.PHONY: all test clean
