# Simple Makefile for Automation Daemon

# Default target architecture
TARGET_ARCH ?= aarch64

# Target Android API level
API_LEVEL=24

# --- Toolchain ---
# Ensure ANDROID_NDK_HOME is set
ifeq ($(ANDROID_NDK_HOME),)
$(error ANDROID_NDK_HOME is not set. Please set it to your Android NDK root directory.)
endif

# Auto-detect host OS and architecture for the toolchain path
HOST_OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')
HOST_ARCH := $(shell uname -m)
TOOLCHAIN := $(ANDROID_NDK_HOME)/toolchains/llvm/prebuilt/$(HOST_OS)-$(HOST_ARCH)

# Define compiler based on target architecture
ifeq ($(TARGET_ARCH), aarch64)
    CC := $(TOOLCHAIN)/bin/aarch64-linux-android$(API_LEVEL)-clang
else ifeq ($(TARGET_ARCH), arm)
    CC := $(TOOLCHAIN)/bin/armv7a-linux-androideabi$(API_LEVEL)-clang
else
    $(error "Unsupported TARGET_ARCH: $(TARGET_ARCH). Supported values are 'aarch64' and 'arm'.")
endif

# --- Build Rules ---
TARGET_BIN := autd
SRC_FILES  := source/main.c source/game.c source/utils.c
INCLUDES   := -Iinclude
CFLAGS     := $(INCLUDES) -Wall -O2 -fPIE
LDFLAGS    := -pie

# Default target
all: $(TARGET_BIN)

$(TARGET_BIN): $(SRC_FILES)
	@echo "Compiling for $(TARGET_ARCH)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo "Binary 'autd' created successfully."

# --- Phony Targets ---
.PHONY: all clean

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -f $(TARGET_BIN)
	@echo "Done."

