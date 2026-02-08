CC 				= gcc
CFLAGS_RELEASE 	= -Wall -Wextra -O2
CFLAGS_DEBUG 	= -Wall -Wextra -g -O0
# ----------------------------------------
# pkg-config libraries
# ----------------------------------------
PKG 			= pkg-config
PKG_PKGS 		= sdl2 libavformat libavcodec libavutil libswscale

# ----------------------------------------
# INCS and LIBS flags
# ----------------------------------------
INCS 			= $(shell $(PKG) --cflags $(PKG_PKGS))
LIBS			= $(shell $(PKG) --libs $(PKG_PKGS))
LIBS            += -lm

# ----------------------------------------
# SAN flags
# ----------------------------------------
SAN_FLAGS = -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer

# ----------------------------------------
# PATHS
# ----------------------------------------
SRC_DIR 		= src
BUILD_DIR 		= build
BIN_DIR			= bin

SRCS			= $(wildcard $(SRC_DIR)/*.c)

TARGET_NAME 	= rtsp-peek

BUILD_TYPE 		?= release
# override BUILD_TYPE = $(BUILD_TYPE)

# ----------------------------------------
# adjust flags and dirs per build type
# ----------------------------------------
ifeq ($(BUILD_TYPE),debug)
	CFLAGS 			= $(CFLAGS_DEBUG) $(SAN_FLAGS)
	LDFLAGS         = $(SAN_FLAGS)
	BUILD_SUBDIR 	= debug
else
	CFLAGS 			= $(CFLAGS_RELEASE)
	LDFLAGS         =
	BUILD_SUBDIR 	= release
endif

# ----------------------------------------
# Dynamic paths
# ----------------------------------------
BUILD_DIR_FULL 	= $(BUILD_DIR)/$(BUILD_SUBDIR)
BIN_DIR_FULL 	= $(BIN_DIR)/$(BUILD_SUBDIR)

TARGET 			= $(BIN_DIR_FULL)/$(TARGET_NAME)

OBJS			= $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR_FULL)/%.o,$(SRCS))


# ----------------------------------------
# Default target: release
# ----------------------------------------
.DEFAULT_GOAL = release

# ----------------------------------------
# Build targets
# ----------------------------------------
all: $(TARGET)

# link
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR_FULL)
	$(CC) $(CFLAGS) $(INCS) $^ -o $@ $(LDFLAGS) $(LIBS)

# compile
$(BUILD_DIR_FULL)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR_FULL)
	@echo "Compiling $< [$(BUILD_TYPE)]"
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

# ----------------------------------------
# Convenience build targets
# ----------------------------------------
debug:
	@$(MAKE) BUILD_TYPE=debug all

release:
	@$(MAKE) BUILD_TYPE=release all

# ----------------------------------------
# Run
# ----------------------------------------
run: release
	@echo "Running $(TARGET)..."
	./$(TARGET) $(ARGS)

run-debug:
	@$(MAKE) BUILD_TYPE=debug run-debug-internal

run-debug-internal: $(TARGET)
	@echo "Launching $(TARGET) in gdb..."
	gdb --tui --args ./$(TARGET) $(ARGS)

run-release:
	@$(MAKE) BUILD_TYPE=release run-release-internal

run-release-internal: $(TARGET)
	@echo "Running release binary..."
	./$(TARGET) $(ARGS)

# ----------------------------------------
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean run debug release run-debug run-debug-internal run-release
