BUILD_TYPE ?= release

APP_NAME := rtsp-peek

# ----------------------------------------
# COMPILER
# ----------------------------------------
CC 				= gcc

# COMMON_FLAGS = -Wall -Wextra -Werror -Wshadow -Wconversion
COMMON_FLAGS = -Wall -Wextra -pthread
CFLAGS_RELEASE = -O2
CFLAGS_DEBUG = -g -O0
CFLAGS_ASAN = -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g -O0

ifeq ($(BUILD_TYPE),release)
  CFLAGS := $(COMMON_FLAGS) $(CFLAGS_RELEASE)
else ifeq ($(BUILD_TYPE),debug)
  CFLAGS := $(COMMON_FLAGS) $(CFLAGS_DEBUG)
else ifeq ($(BUILD_TYPE),asan)
  CFLAGS := $(COMMON_FLAGS) $(CFLAGS_ASAN)
else
  $(error '$(BUILD_TYPE)' is not a valid BUILD_TYPE. Please use 'release' (default), 'debug' or 'asan')
endif

# ----------------------------------------
# INCLUDES
# ----------------------------------------

# pkg-config libraries
PKG = pkg-config
PKG_PKGS = sdl2 libavformat libavcodec libavutil libswscale
LIBS_INCS = $(shell $(PKG) --cflags $(PKG_PKGS))
LIBS = $(shell $(PKG) --libs $(PKG_PKGS))
LIBS += -lm # math libm

PUBLIC_INCS := $(filter-out %.c %.h,$(addprefix -I,$(wildcard include/*)))
# PUBLIC_INCS += $(filter-out %.c %.h,$(addprefix -I,$(wildcard external/*)))
PRIVATE_INCS := $(filter-out %.c %.h,$(addprefix -I,$(wildcard src/*)))
TESTS_INCS := $(filter-out %.c %.h, $(addprefix -I,$(wildcard tests*)))
EXTERNAL_INCS := $(filter-out %.c %h,$(addprefix -I,$(wildcard external/*)))

# ----------------------------------------
# Dependency generation flags
# ----------------------------------------
DEPFLAGS = -MMD -MP

# ----------------------------------------
# SRCS
# ----------------------------------------
SRC_DIR = src
TESTS_DIR = test

APP_SRCS := $(filter %.c,$(wildcard $(SRC_DIR)/*/*))
APP_MAIN_SRC = src/main.c
TESTS_SRCS := $(filter %.c,$(wildcard $(TESTS_DIR)/*/*))
TESTS_SRCS += $(filter %.c,$(wildcard $(TESTS_DIR)/*))

# ----------------------------------------
# OBJS
# ----------------------------------------
BUILD_DIR := build
BUILD_DIR_APP := $(BUILD_DIR)/$(BUILD_TYPE)
BUILD_DIR_TESTS := $(BUILD_DIR)/$(BUILD_TYPE)/$(TESTS_DIR)

APP_MAIN_OBJ := $(BUILD_DIR_APP)/obj/main.o
APP_OBJS := $(subst obj, $(BUILD_DIR_APP)/obj,$(subst src,obj,$(patsubst %.c,%.o,$(APP_SRCS))))
TESTS_OBJS := $(subst $(TESTS_DIR)/,$(BUILD_DIR_TESTS)/,$(patsubst %.c,%.o,$(TESTS_SRCS)))

# ----------------------------------------
# EXEC
# ----------------------------------------
BIN_DIR = bin/$(BUILD_TYPE)
TARGET := $(BIN_DIR)/$(APP_NAME)
TESTS_TARGET := $(BIN_DIR)/$(APP_NAME)-tests

# ----------------------------------------
# Install paths
# ----------------------------------------
DESTDIR ?= $(HOME)/.local/bin
BIN_INSTALL_DIR := $(DESTDIR)

# ----------------------------------------
# RULES
# ----------------------------------------
all: $(TARGET)

# tests: $(TESTS_TARGET)

$(TARGET): $(APP_MAIN_OBJ) $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LIBS_INCS) $(PUBLIC_INCS) $(EXTERNAL_INCS) $^ -o $@ $(LIBS)

$(APP_MAIN_OBJ): $(APP_MAIN_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(LIBS_INCS) $(PUBLIC_INCS) $(PRIVATE_INCS) $(EXTERNAL_INCS) -c $< -o $@

$(APP_OBJS): $(BUILD_DIR_APP)/obj/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(LIBS_INCS) $(PUBLIC_INCS) $(PRIVATE_INCS) $(EXTERNAL_INCS) -c $< -o $@

# TODO TESTS_TARGET, TESTS_OBJS

# TODO builds with santizers

clean:
	@rm -rf ./$(BUILD_DIR) ./$(BIN_DIR)

run: $(TARGET)
	./$(TARGET)

# ----------------------------------------
# Install / Uninstall
# ----------------------------------------

install: $(TARGET)
	@echo "Installing $(APP_NAME) ($(BUILD_TYPE)) to $(BIN_INSTALL_DIR)..."
	@mkdir -p $(BIN_INSTALL_DIR)
	@install -m 755 $(TARGET) $(BIN_INSTALL_DIR)/$(APP_NAME)
	@echo "Successfully installed."

uninstall:
	@echo "Removing $(APP_NAME) from $(BIN_INSTALL_DIR)..."
	@rm -f $(BIN_INSTALL_DIR)/$(APP_NAME)
	@echo "Successfully uninstalled."

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

.PHONY: all clean run install uninstall check-deps

# -include $(DEPS)
