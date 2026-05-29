# =============================================================================
# ST4Ever - Makefile
#
# Automatically detects MSYS2/Windows vs Linux.
# All .o files go into build/  (prefixed to avoid name collisions).
# Application executable goes into release/.
# Use-case test executables go into tests/.
#
# Platform detection strategy:
#   PLATFORM variable selects the source directory (win/ or linux/).
#   The C compiler symbols _WIN32 / __linux__ are defined automatically
#   by GCC.  common.h converts them to ST_PLATFORM_WINDOWS/LINUX.
#   Do NOT pass -DST_PLATFORM_* here: common.h already does it, and
#   adding it from the command line triggers a redefinition warning.
#
# Targets:
#   make           Build everything (app + tests)
#   make run       Build and run the application
#   make tests     Execute all use case tests (run from project root)
#   make clean     Remove build/, release/, tests/
# =============================================================================

CC     := gcc
# -std=gnu99: enables ##__VA_ARGS__ extension used by the LOG_* macros
# (standard C99 requires at least one variadic argument; GNU extension
#  allows zero.  GCC/MinGW-W64 always supports this regardless of target.)
CFLAGS := -Wall -Wextra -pedantic -std=gnu99 -g -D_GNU_SOURCE
CFLAGS += -I./src

# -----------------------------------------------------------------------------
# Platform detection
# Note: _WIN32 / __linux__ are automatically defined by GCC.
#       common.h uses them to define ST_PLATFORM_WINDOWS / LINUX.
#       PLATFORM here only controls which backend source directory to compile.
# -----------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    PLATFORM := win
    EXE      := .exe
    LDFLAGS  := -lkernel32 -luser32 -lgdi32
    # TODO(UC3): add -ld2d1 -ldwrite -lole32 -luuid when Direct2D is active
else
    PLATFORM := linux
    EXE      :=
    LDFLAGS  := -lpthread
    # TODO(UC3): add -lX11 -lXft -lfontconfig when X11 is active
endif

# -----------------------------------------------------------------------------
# Directory layout
# -----------------------------------------------------------------------------
BUILD   := build
RELEASE := release
TDIR    := test
UC_DIR  := use_cases

# -----------------------------------------------------------------------------
# Source discovery
# All .c under src/ and under the platform directory are compiled.
# Object files are stored flat in build/ with a prefix to avoid collisions:
#   src/foo.c       -> build/s_foo.o
#   win/bar.c       -> build/p_bar.o
# -----------------------------------------------------------------------------
SRC_FILES  := $(wildcard src/*.c)
PLAT_FILES := $(wildcard $(PLATFORM)/*.c)
UC_FILES   := $(wildcard $(UC_DIR)/use_case_*.c)

SRC_OBJS   := $(patsubst src/%.c,          $(BUILD)/s_%.o, $(SRC_FILES))
PLAT_OBJS  := $(patsubst $(PLATFORM)/%.c,  $(BUILD)/p_%.o, $(PLAT_FILES))
ALL_OBJS   := $(SRC_OBJS) $(PLAT_OBJS)

# Library objects = everything except main.o (used by test programs)
LIB_OBJS   := $(filter-out $(BUILD)/s_main.o, $(ALL_OBJS))

# Final targets
TARGET     := $(RELEASE)/st4ever$(EXE)
UC_TARGETS := $(patsubst $(UC_DIR)/%.c, $(TDIR)/%$(EXE), $(UC_FILES))

# -----------------------------------------------------------------------------
# Top-level targets
# -----------------------------------------------------------------------------
.PHONY: all run tests clean

all: $(BUILD) $(RELEASE) $(TDIR) $(TARGET) $(UC_TARGETS)

$(BUILD) $(RELEASE) $(TDIR):
	mkdir -p $@

# -----------------------------------------------------------------------------
# Compile rules
# -----------------------------------------------------------------------------

# src/*.c  ->  build/s_*.o
$(BUILD)/s_%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# win/*.c or linux/*.c  ->  build/p_*.o
$(BUILD)/p_%.o: $(PLATFORM)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------------------------------------------------------
# Link rules
# -----------------------------------------------------------------------------

# Main application
$(TARGET): $(ALL_OBJS)
	$(CC) $(ALL_OBJS) -o $@ $(LDFLAGS)
	@echo "  --> $(TARGET)"

# Each use_case_NN.c linked against the library objects (no main.o)
# NOTE: run 'make tests' from the project root - test binaries use
#       relative paths (e.g. use_cases/UC01/hello.prg).
$(TDIR)/%$(EXE): $(UC_DIR)/%.c $(LIB_OBJS)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS)
	@echo "  --> $@"

# -----------------------------------------------------------------------------
# Run / test helpers
# -----------------------------------------------------------------------------
run: all
	$(TARGET)

tests: all
	@echo ""
	@echo "================================================================"
	@echo " ST4Ever - Use Case Tests"
	@echo " (must be run from the project root directory)"
	@echo "================================================================"
	@FAILS=0; \
	for t in $(UC_TARGETS); do \
	    echo ""; \
	    echo "  Running: $$t"; \
	    echo "  --------"; \
	    $$t; STATUS=$$?; \
	    if [ $$STATUS -ne 0 ]; then FAILS=$$((FAILS + 1)); fi; \
	done; \
	echo ""; \
	echo "================================================================"; \
	echo " Results: $$FAILS test program(s) reported failure"; \
	echo "================================================================"; \
	echo ""; \
	exit $$FAILS

# -----------------------------------------------------------------------------
clean:
	rm -rf $(BUILD) $(RELEASE) $(TDIR)
