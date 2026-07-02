# Makefile for sercon - Serial Ports Console Utility

# set colors if in TTY mode
ifneq ($(and $(MAKE_TERMOUT),$(MAKE_TERMERR)),)
	COLOR_RESET := \033[0m
	COLOR_BOLD := \033[1m
	COLOR_ITALIC := \033[3m
	COLOR_UNDERLINE := \033[4m
	COLOR_ERROR := \033[31m
	COLOR_SUCCESS := \033[32m
	COLOR_WARNING := \033[33m
	COLOR_INFO := \033[34m
endif

# versioning: get the short git commit hash if available, otherwise use 'unknown'
GIT_COMMIT_HASH=$(shell git rev-parse --short HEAD 2>/dev/null || echo 'unknown')
VERSION ?= 1.3.0
FULL_VERSION := $(VERSION).$(GIT_COMMIT_HASH)

CC := cc
CFLAGS := -Wall -Wextra -Wno-unused-result -Wno-unused-parameter -MMD -MP -DVERSION=\"$(FULL_VERSION)\" -D_GNU_SOURCE
TREE_FLAGS := --charset=utf8 -F -C --dirsfirst --noreport
TARGET := sercon
SRC_DIRS := src
INCLUDE_DIRS :=
LIB_DIRS :=
SRCS := $(wildcard $(SRC_DIRS)/*.c)
LIBS := -lreadline -lm
ARTIFACTS_ROOT_DIR ?= $(shell pwd)
BUILD_ROOT := $(ARTIFACTS_ROOT_DIR)/build
DIST_DIR := $(ARTIFACTS_ROOT_DIR)/dist
DOXYGEN_ARTIFACTS_DIR := $(ARTIFACTS_ROOT_DIR)/doxygen
SUPPORTED_TARGETS := $(sort $(shell cat $(lastword $(MAKEFILE_LIST)) | grep -Eo '^[[:alnum:]]+:' | tr -d ':'))

BUILD ?= debug
ARCH ?= $(shell uname -m | tr '[:upper:]' '[:lower:]')
PLATFORM ?= $(shell uname -s | tr '[:upper:]' '[:lower:]')

# installation-related variables
PREFIX ?= /usr/local
MAN_SECTION = 1
MAN_DIR = $(PREFIX)/share/man/man$(MAN_SECTION)
MAN_PAGE = $(TARGET).$(MAN_SECTION)
README = README.md

# protect against accidentally setting ARTIFACTS_ROOT_DIR to the root directory, which could
# lead to disastrous consequences if the Makefile is run with a 'cleanall' target.
ifeq ($(ARTIFACTS_ROOT_DIR),/)
$(error ARTIFACTS_ROOT_DIR cannot be set to the root directory (/)!)
endif

# after we've established the platform and architecture, we can define the
# build directory based on those variables. This allows us to have separate
# build directories for different configurations, which is useful for managing
# multiple builds without conflicts.
# under this root directories we will have a separate debug and release directories.
BUILD_PLAT_ROOT_DIR := $(BUILD_ROOT)/$(FULL_VERSION)/$(PLATFORM)/$(ARCH)

# select the appropriate include and library directories based on the platform and architecture.
ifeq ($(PLATFORM), darwin)
	TREE_FLAGS += --condense
	OPENER := open
ifneq (,$(filter $(ARCH), arm64 aarch64))
# darwin/arm64
	INCLUDE_DIRS += -I/opt/homebrew/opt/readline/include -I/opt/homebrew/opt/sqlite/include
	LIB_DIRS += -L/opt/homebrew/opt/readline/lib -L/opt/homebrew/lib -L/opt/homebrew/opt/sqlite/lib
else ifeq ($(ARCH), x86_64)
# darwin/x86_64
	INCLUDE_DIRS += -I/usr/local/opt/readline/include -I/usr/local/opt/sqlite/include
	LIB_DIRS += -L/usr/local/opt/readline/lib -L/usr/local/opt/sqlite/lib
else
$(error Invalid darwin architecture: $(ARCH). Use \'arm64\' or \'x86_64\'.)
endif # darwin architecture check
else ifeq ($(PLATFORM), linux)
# linux/all architectures
	INCLUDE_DIRS += -I/usr/local/opt/readline/include -I/usr/local/opt/sqlite/include
	LIB_DIRS += -L/usr/local/opt/readline/lib -L/usr/local/opt/sqlite/lib
	OPENER := xdg-open
else
$(error Unsupported platform: $(PLATFORM). Use \'darwin\' or \'linux\'.)
endif

# debug/release adaptations
ifeq ($(BUILD), debug)
	CFLAGS += -O0 -g -D_DEBUG_ -DDEBUG -fsanitize=address
	LDFLAGS += -fsanitize=address
	BUILD_DIR := $(BUILD_PLAT_ROOT_DIR)/debug
else ifeq ($(BUILD), release)
	CFLAGS += -O3 -DNDEBUG
	BUILD_DIR := $(BUILD_PLAT_ROOT_DIR)/release
else
$(error Invalid build type: $(BUILD). Use \'debug\' or \'release\'.)
endif

TARGET_PATH = $(BUILD_DIR)/$(TARGET)

# compose a list of object files based on source files
OBJS = $(foreach src, $(SRCS), $(patsubst $(SRC_DIRS)/%.c, $(BUILD_DIR)/%.o, $(src)))
# compose a list of dependency files based on source files
DEPS = $(OBJS:.o=.d)

# default entry point
all: $(TARGET_PATH) summary

# LINK
$(TARGET_PATH): $(OBJS)
	$(CC) $(LIB_DIRS) $(LDFLAGS) -o $(TARGET_PATH) $(OBJS) $(LIBS)

# print a summary of the build, including the size of the target executable.
summary: SIZE = $(shell wc -c < $(TARGET_PATH) | tr -d ' ')
summary:
	@printf "$(COLOR_SUCCESS)** Build successful: $(TARGET_PATH) ($(SIZE) bytes)$(COLOR_RESET)\n"

# COMPILE
$(BUILD_DIR)/%.o: $(SRC_DIRS)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) -c $< -o $@

# create build directory if it still doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# run arguments are taken from the command line arguments, excluding leading supported targets.
run: RUN_ARGS := $(filter-out $(SUPPORTED_TARGETS), $(MAKECMDGOALS))
run: all
	@$(BUILD_DIR)/$(TARGET) $(RUN_ARGS)

# show man page (pre-installation)
man:
	@$(OPENER) ./$(MAN_PAGE)

# open the README file in the default browser
readme:
	@$(OPENER) $(README)

doxygen:
	@rm -rf $(DOXYGEN_ARTIFACTS_DIR)/$(FULL_VERSION)
	@mkdir -p $(DOXYGEN_ARTIFACTS_DIR)/$(FULL_VERSION)
	@doxygen -g
	@sed -E -i '' 's|^INPUT.+|INPUT = src|g' Doxyfile
	@sed -E -i '' 's|^GENERATE_HTML.+|GENERATE_HTML = YES|g' Doxyfile
	@sed -E -i '' 's|^SEARCHENGINE.+|SEARCHENGINE = YES|g' Doxyfile
	@sed -E -i '' 's|^OUTPUT_DIRECTORY.+|OUTPUT_DIRECTORY = "$(DOXYGEN_ARTIFACTS_DIR)/$(FULL_VERSION)"|g' Doxyfile
	@doxygen Doxyfile
	@$(OPENER) $(DOXYGEN_ARTIFACTS_DIR)/$(FULL_VERSION)/html/index.html
	@rm -rf Doxyfile*

# open the GitHub repository in the default browser
github:
	@$(OPENER) "https://github.com/airlobster/sercon/tree/main"

# override BUILD_DIR to ensure it builds the release version before packaging
package: BUILD_DIR := $(BUILD_PLAT_ROOT_DIR)/release
package: PKG_NAME := $(TARGET)-$(FULL_VERSION)-$(ARCH).tar.gz
package:
	$(MAKE) BUILD=release all
	mkdir -p $(DIST_DIR)
	tar -czvf $(DIST_DIR)/$(PKG_NAME)  $(MAN_PAGE) $(README) -C $(BUILD_DIR) $(TARGET)
	@printf "$(COLOR_SUCCESS)** Package created: $(DIST_DIR)/$(PKG_NAME)$(COLOR_RESET)\n"

# override BUILD_DIR to ensure it builds the release version before installing
install: BUILD_DIR := $(BUILD_PLAT_ROOT_DIR)/release
install:
	$(MAKE) BUILD=release all
	install -m 755 $(BUILD_DIR)/$(TARGET) $(PREFIX)/bin/$(TARGET)
	mkdir -p $(MAN_DIR)
	install -m 644 $(MAN_PAGE) $(MAN_DIR)/$(MAN_PAGE)
	@printf "$(COLOR_SUCCESS)** Installed $(TARGET) to $(PREFIX)/bin and man page to $(MAN_DIR)$(COLOR_RESET)\n"

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(MAN_DIR)/$(MAN_PAGE)
	@printf "$(COLOR_SUCCESS)** Uninstalled $(TARGET) and its man page from $(PREFIX)$(COLOR_RESET)\n"

clean:
	rm -rf $(BUILD_DIR) $(DOXYGEN_ARTIFACTS_DIR)/$(FULL_VERSION)

cleanall:
	rm -rf $(BUILD_ROOT) $(DIST_DIR) $(DOXYGEN_ARTIFACTS_DIR) Doxyfile*

test:
	@./tests/test_linuxes

help:
	@printf "$(COLOR_BOLD)Usage:$(COLOR_RESET)\n"
	@printf "  $(COLOR_ITALIC)make [target] [BUILD=debug|release] [ARCH=arm64|x86_64] [VERSION=x.y.z]$(COLOR_RESET)\n"
	@printf "\n"
	@printf "$(COLOR_BOLD)Targets:$(COLOR_RESET)\n"
	@printf "  $(COLOR_INFO)all$(COLOR_RESET)       - Build the project (default)\n"
	@printf "  $(COLOR_INFO)clean$(COLOR_RESET)     - Remove build artifacts for the current build type\n"
	@printf "  $(COLOR_INFO)cleanall$(COLOR_RESET)  - Remove all build artifacts and the target executable\n"
	@printf "  $(COLOR_INFO)install$(COLOR_RESET)   - Install the target executable and man page\n"
	@printf "  $(COLOR_INFO)uninstall$(COLOR_RESET) - Uninstall the target executable and man page\n"
	@printf "  $(COLOR_INFO)package$(COLOR_RESET)   - Create a tar.gz package of the build output\n"
	@printf "  $(COLOR_INFO)run$(COLOR_RESET)       - Build and run the target executable with optional arguments (after --)\n"
	@printf "  $(COLOR_INFO)man$(COLOR_RESET)       - Display the man page\n"
	@printf "  $(COLOR_INFO)github$(COLOR_RESET)    - Open the GitHub repository in the default browser\n"
	@printf "  $(COLOR_INFO)readme$(COLOR_RESET)    - Display the README file\n"
	@printf "  $(COLOR_INFO)vars$(COLOR_RESET)      - Display current variable settings\n"
	@printf "  $(COLOR_INFO)test$(COLOR_RESET)      - Run the test suite\n"
	@printf "  $(COLOR_INFO)doxygen$(COLOR_RESET)   - Generate and open Doxygen site\n"
	@printf "  $(COLOR_INFO)tree$(COLOR_RESET)      - Display the artifacts directory's tree\n"
	@printf "\n"
	@printf "$(COLOR_BOLD)Build types:$(COLOR_RESET)\n"
	@printf "  $(COLOR_INFO)debug$(COLOR_RESET)     - Build with debug symbols and no optimizations (default)\n"
	@printf "  $(COLOR_INFO)release$(COLOR_RESET)   - Build with optimizations and no debug symbols\n"
	@printf "\n"
	@printf "$(COLOR_BOLD)Notes:$(COLOR_RESET)\n"
	@printf "  * Assign 'debug' or 'release' to BUILD to specify the build type. Default is 'debug'.$(COLOR_RESET)\n"
	@printf "  * Assign a new path to ARTIFACTS_ROOT_DIR to change where build and dist directories are created.$(COLOR_RESET)\n"
	@printf "    Default is current directory.$(COLOR_RESET)\n"
	@printf "\n"

# (the while loop here is for indenting the whole tree output to make it look nicer in the console)
tree:
	@mkdir -p $(ARTIFACTS_ROOT_DIR)
	@tree $(TREE_FLAGS) -P $(TARGET) -P *.tar.gz $(ARTIFACTS_ROOT_DIR) \
		| while IFS= read line; do printf "   %s\n" "$$line"; done

vars:
	@printf "* $(COLOR_BOLD)Platform$(COLOR_RESET): $(COLOR_INFO)$(PLATFORM)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Architecture$(COLOR_RESET): $(COLOR_INFO)$(ARCH)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Full version$(COLOR_RESET): $(COLOR_INFO)$(FULL_VERSION)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Compiler flags$(COLOR_RESET): $(COLOR_INFO)$(CFLAGS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Include directories$(COLOR_RESET): $(COLOR_INFO)$(INCLUDE_DIRS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Library directories$(COLOR_RESET): $(COLOR_INFO)$(LIB_DIRS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Artifacts root directory$(COLOR_RESET): $(COLOR_INFO)$(ARTIFACTS_ROOT_DIR)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Build directory$(COLOR_RESET): $(COLOR_INFO)$(BUILD_DIR)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Source files$(COLOR_RESET): $(COLOR_INFO)$(SRCS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Object files$(COLOR_RESET): $(COLOR_INFO)$(OBJS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Dependency files$(COLOR_RESET): $(COLOR_INFO)$(DEPS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Libraries$(COLOR_RESET): $(COLOR_INFO)$(LIBS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Target$(COLOR_RESET): $(COLOR_INFO)$(BUILD_DIR)/$(TARGET)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Run arguments$(COLOR_RESET): $(COLOR_INFO)$(RUN_ARGS)$(COLOR_RESET)\n"
	@printf "* $(COLOR_BOLD)Makefile targets$(COLOR_RESET): $(COLOR_INFO)$(SUPPORTED_TARGETS)$(COLOR_RESET)\n"

# fall-back for all unknown targets to allow passing arguments to the 'run' target without causing make to fail.
%:
	@:

.PHONY: all clean cleanall help install uninstall package man vars test run doxygen github readme summary doxygen tree

-include $(DEPS)
