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
# Dependency generation flags
# ----------------------------------------
DEPFLAGS = -MMD -MP


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
	CFLAGS 			= $(CFLAGS_DEBUG)
	LDFLAGS         =
	BUILD_SUBDIR 	= debug

else ifeq ($(BUILD_TYPE),sanitize)
	CFLAGS          = $(CFLAGS_RELEASE) -g $(SAN_FLAGS)
	LDFLAGS         = $(SAN_FLAGS)
	BUILD_SUBDIR    = sanitize

else ifeq ($(BUILD_TYPE),debug-sanitize)
	CFLAGS       = $(CFLAGS_DEBUG) $(SAN_FLAGS)
	LDFLAGS      = $(SAN_FLAGS)
	BUILD_SUBDIR = debug-sanitize

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

DEPS 			= $(OBJS:.o=.d)

# ----------------------------------------
# Install paths
# ----------------------------------------
PREFIX ?= $(HOME)/.local
BIN_INS_DIR     = $(PREFIX)/bin
DESTDIR ?=

# ----------------------------------------
# Default target: release
# ----------------------------------------
.DEFAULT_GOAL = release

# ----------------------------------------
# Build targets
# ----------------------------------------
all: check-deps $(TARGET)

# link
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR_FULL)
	$(CC) $(CFLAGS) $(INCS) $^ -o $@ $(LDFLAGS) $(LIBS)

# compile
$(BUILD_DIR_FULL)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR_FULL)
	@echo "Compiling $< [$(BUILD_TYPE)]"
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCS) -c $< -o $@

# ----------------------------------------
# Convenience build targets
# ----------------------------------------
debug:
	@$(MAKE) BUILD_TYPE=debug all

release:
	@$(MAKE) BUILD_TYPE=release all

sanitize:
	@$(MAKE) BUILD_TYPE=sanitize all

debug-sanitize:
	@$(MAKE) BUILD_TYPE=debug-sanitize all

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

run-debug-sanitize:
	@$(MAKE) BUILD_TYPE=debug-sanitize run-debug-sanitize-internal

run-debug-sanitize-internal: $(TARGET)
	@echo "Launching $(TARGET) in gdb (debug-sanitize)..."
	gdb --tui --args ./$(TARGET) $(ARGS)

run-sanitize:
	@$(MAKE) BUILD_TYPE=sanitize run-release-internal

# ----------------------------------------
# Install / Uninstall
# ----------------------------------------

install: release
	@echo "Installing $(TARGET_NAME) ($(BUILD_SUBDIR)) to $(DESTDIR)$(BIN_INS_DIR)..."
	@mkdir -p $(DESTDIR)$(BIN_INS_DIR)
	@install -m 755 $(TARGET) $(DESTDIR)$(BIN_INS_DIR)/$(TARGET_NAME)
	@echo "Successfully installed."

uninstall:
	@echo "Removing $(TARGET_NAME) from $(BIN_INS_DIR)..."
	@rm -f $(DESTDIR)$(BIN_INS_DIR)/$(TARGET_NAME)
	@echo "Successfully uninstalled."

install-debug:
	@$(MAKE) BUILD_TYPE=debug install

install-release:
	@$(MAKE) BUILD_TYPE=release install

install-sanitize:
	@$(MAKE) BUILD_TYPE=sanitize install

install-debug-sanitize:
	@$(MAKE) BUILD_TYPE=debug-sanitize install

# ----------------------------------------
# Check deps
# ----------------------------------------

check-deps:
	@printf "Checking dependencies...\n"
	@pkg-config --exists sdl2 || (echo "ERROR: SDL2 dev package not found"; exit 1)
	@pkg-config --exists libavcodec || (echo "ERROR: libavcodec dev package not found"; exit 1)
	@pkg-config --exists libavformat || (echo "ERROR: libavformat dev package not found"; exit 1)
	@pkg-config --exists libavutil || (echo "ERROR: libavutil dev package not found"; exit 1)
	@pkg-config --exists libswscale || (echo "ERROR: libswscale dev package not found"; exit 1)
	@echo "All dependencies found."

# ----------------------------------------
# Help
# ----------------------------------------
help:
	@printf "\nUsage:\n"
	@printf "  make [target] [VARIABLE=value]\n\n"

	@printf "Build targets:\n"
	@printf "  make, make release          Build release binary (default)\n"
	@printf "  make debug                  Build debug binary\n"
	@printf "  make sanitize               Build release-like binary with sanitizers (-O2 + sanitizers)\n"
	@printf "  make debug-sanitize         Build debug binary with sanitizers (-O0 + sanitizers)\n\n"

	@printf "Run targets:\n"
	@printf "  make run                    Build & run release binary\n"
	@printf "  make run-debug              Build & run debug binary in gdb\n"
	@printf "  make run-debug-sanitize     Build & run debug-sanitize binary in gdb\n"
	@printf "  make run-sanitize           Build & run sanitize binary\n\n"

	@printf "Install targets:\n"
	@printf "  make install                Install release binary (default)\n"
	@printf "  make install-debug          Install debug binary\n"
	@printf "  make install-sanitize       Install release-like sanitize binary\n"
	@printf "  make install-debug-sanitize Install debug-sanitize binary\n\n"

	@printf "Install location:\n"
	@printf "  Default prefix: %s\n" "$(PREFIX)"
	@printf "  Binary path:   %s/%s\n\n" "$(BIN_INS_DIR)" "$(TARGET_NAME)"

	@printf "Common variables:\n"
	@printf "  BUILD_TYPE=release|debug|sanitize|debug-sanitize   Select build type (default: release)\n"
	@printf "  PREFIX=PATH                                        Install prefix (default: %s)\n" "$(PREFIX)"
	@printf "  DESTDIR=PATH                                       Staging prefix for packaging (default empty)\n"
	@printf "  ARGS=\"...\"                                         Arguments passed to the program\n\n"

	@printf "Examples:\n"
	@printf "  make debug install\n"
	@printf "  make sanitize install\n"
	@printf "  make debug-sanitize install\n"
	@printf "  make install PREFIX=/usr/local\n"
	@printf "  make run ARGS=\"rtsp://example.com/stream\"\n\n"
# ----------------------------------------
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: \
	all clean debug release sanitize debug-sanitize \
	run run-debug run-debug-internal run-debug-sanitize run-debug-sanitize-internal \
	run-release run-release-internal \
	install install-debug install-release install-sanitize install-debug-sanitize \
	uninstall help check-deps

-include $(DEPS)
