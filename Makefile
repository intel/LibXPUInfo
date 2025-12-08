# Makefile for a C++ command-line application (2025 best practices)

# -----------------------------
# Configuration
# -----------------------------
CXX      := g++                  # or clang++
CXXFLAGS := -std=c++17 -I. -I/usr/local/cuda/include -DENABLE_PER_LOGICAL_CPUID_ISA_DETECTION=0 -DXPUINFO_USE_NVML=1
# -H : show include path
#-fno-omit-frame-pointer 
# -Wall -Wextra -Wpedantic -Werror
#CPPFLAGS := -MMD -MP             # auto dependency generation
LDFLAGS  := -L/usr/local/cuda/lib64 -L/usr/local/cuda/lib64/stubs # e.g. -L/usr/local/lib
LDLIBS   := -lnvidia-ml             # e.g. -lpthread -lboost_program_options

# Build type (debug by default)
BUILD    := debug
ifeq ($(BUILD),release)
    CXXFLAGS += -O3 -DNDEBUG
else
    CXXFLAGS += -O0 -g #-fsanitize=address,undefined
	CXXFLAGS += -DHYBRID_DETECT_TRACE_ENABLED_VOLUME=0
    #LDFLAGS  += -fsanitize=address,undefined
endif

# -----------------------------
# Files and directories
# -----------------------------
TARGET   := $(BUILD)/testLibXPUInfo                # name of final executable
SRC_DIR  := .
OBJ_DIR  := obj/$(BUILD)
BIN_DIR  := bin

#SOURCES  := $(wildcard $(SRC_DIR)/*.cpp $(SRC_DIR)/**/*.cpp)
SOURCES  := LibXPUInfo.cpp LibXPUInfo_Util.cpp LibXPUInfo_NVML.cpp DebugStream.cpp TestLibXPUInfo/TestLibXPUInfo.cpp

# LibXPUInfo_JSON.cpp \
# LibXPUInfo_NVML.cpp \
# LibXPUInfo_Util.cpp
#LibXPUInfo_DXCore.cpp \
#LibXPUInfo_IPC.cpp \
#LibXPUInfo_IntelDeviceInfoDX11.cpp \
#LibXPUInfo_L0.cpp \
#LibXPUInfo_OpenCL.cpp \
#LibXPUInfo_SetupAPI.cpp \
#LibXPUInfo_TelemetryTracker.cpp \
#LibXPUInfo_WMI.cpp
#DebugStream.cpp

#OBJECTS  := $(SOURCES:.cpp=.o)

OBJECTS  := $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)
#DEPENDS  := $(OBJECTS:.o=.d)

# Create directories if they don't exist
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR) 2>/dev/null)

# -----------------------------
# Main targets
# -----------------------------
.PHONY: all clean run debug release

all: $(BIN_DIR)/$(TARGET)

# Link the executable
$(BIN_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@
	@echo "Build finished: $@"

# Compile object files
#%.o: %.cpp
#	@echo $(CXX) $(CXXFLAGS) -c $< -o $@
#	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Include auto-generated dependencies
# -include $(DEPENDS)

# -----------------------------
# Convenience targets
# -----------------------------
debug:
	$(MAKE) BUILD=debug all

release:
	$(MAKE) BUILD=release all

run: debug
	./$(BIN_DIR)/$(TARGET)

clean:
	rm -rf obj bin *.o
	@echo "Clean complete"

# Optional: show help
help:
	@echo "Targets:"
	@echo "  all     - build debug version (default)"
	@echo "  debug   - explicitly build debug version"
	@echo "  release - build optimized release version"
	@echo "  run     - build debug and execute"
	@echo "  clean   - remove build artifacts"