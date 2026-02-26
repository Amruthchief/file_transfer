# File Transfer Utility Makefile
# Cross-platform build system for Linux, macOS, and Windows (MinGW/Cygwin)

# Compiler
CC := gcc

# Compiler flags
CFLAGS := -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 -Isrc/common
LDFLAGS :=

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Linux)
    PLATFORM := LINUX
    LIBS := -lpthread
    EXE_EXT :=
endif
ifeq ($(UNAME_S),Darwin)
    PLATFORM := MACOS
    LIBS := -lpthread
    EXE_EXT :=
endif
ifeq ($(UNAME_S),Windows)
    PLATFORM := WINDOWS
    LIBS := -lws2_32
    EXE_EXT := .exe
endif
# For MinGW/MSYS on Windows
ifneq (,$(findstring MINGW,$(UNAME_S)))
    PLATFORM := WINDOWS
    LIBS := -lws2_32
    EXE_EXT := .exe
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
    PLATFORM := WINDOWS
    LIBS := -lws2_32
    EXE_EXT := .exe
endif
ifneq (,$(findstring CYGWIN,$(UNAME_S)))
    PLATFORM := WINDOWS
    LIBS := -lws2_32
    EXE_EXT := .exe
endif

# Build mode (default: release)
MODE ?= release
ifeq ($(MODE),debug)
    CFLAGS += -g -O0 -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

# Directories
SRC_DIR := src
COMMON_DIR := $(SRC_DIR)/common
SERVER_DIR := $(SRC_DIR)/server
CLIENT_DIR := $(SRC_DIR)/client
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

# Source files
COMMON_SRCS := $(wildcard $(COMMON_DIR)/*.c)
SERVER_SRCS := $(wildcard $(SERVER_DIR)/*.c)
CLIENT_SRCS := $(wildcard $(CLIENT_DIR)/*.c)

# Object files
COMMON_OBJS := $(patsubst $(COMMON_DIR)/%.c,$(OBJ_DIR)/common/%.o,$(COMMON_SRCS))
SERVER_OBJS := $(patsubst $(SERVER_DIR)/%.c,$(OBJ_DIR)/server/%.o,$(SERVER_SRCS))
CLIENT_OBJS := $(patsubst $(CLIENT_DIR)/%.c,$(OBJ_DIR)/client/%.o,$(CLIENT_SRCS))

# Executables
SERVER_BIN := $(BUILD_DIR)/ftserver$(EXE_EXT)
CLIENT_BIN := $(BUILD_DIR)/ftclient$(EXE_EXT)

# Default target
.PHONY: all
all: $(SERVER_BIN) $(CLIENT_BIN)

# Create directories
$(OBJ_DIR)/common $(OBJ_DIR)/server $(OBJ_DIR)/client:
	mkdir -p $@

# Build server
$(SERVER_BIN): $(COMMON_OBJS) $(SERVER_OBJS) | $(BUILD_DIR)
	@echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Server built successfully: $@"

# Build client
$(CLIENT_BIN): $(COMMON_OBJS) $(CLIENT_OBJS) | $(BUILD_DIR)
	@echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo "Client built successfully: $@"

# Compile common source files
$(OBJ_DIR)/common/%.o: $(COMMON_DIR)/%.c | $(OBJ_DIR)/common
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile server source files
$(OBJ_DIR)/server/%.o: $(SERVER_DIR)/%.c | $(OBJ_DIR)/server
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile client source files
$(OBJ_DIR)/client/%.o: $(CLIENT_DIR)/%.c | $(OBJ_DIR)/client
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Build in debug mode
.PHONY: debug
debug:
	$(MAKE) MODE=debug

# Print build configuration
.PHONY: info
info:
	@echo "=== Build Configuration ==="
	@echo "Platform: $(PLATFORM)"
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "Libs: $(LIBS)"
	@echo "Mode: $(MODE)"
	@echo "=========================="

# Help target
.PHONY: help
help:
	@echo "File Transfer Utility - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build both server and client"
	@echo "  clean         - Remove build artifacts"
	@echo "  debug         - Build with debug symbols"
	@echo "  info          - Show build configuration"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make          - Build release version"
	@echo "  make debug    - Build debug version"
	@echo "  make clean    - Clean build artifacts"
	@echo ""
	@echo "Executables will be in:"
	@echo "  $(SERVER_BIN)"
	@echo "  $(CLIENT_BIN)"
