# =============================================================================
# ST4Ever - Makefile
#
# Automatically detects MSYS2/Windows vs Linux.
# All .o files go into build/  (prefixed to avoid name collisions).
# Application executable goes into release/.
# Use-case test executables go into tests/.
#
# Targets:
#   make           Build everything (app + tests)
#   make run       Build and run the application
#   make tests     Build and execute all use case tests
#   make clean     Remove build/, release/, tests/
# =============================================================================

CC     := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c99 -g -D_GNU_SOURCE
CFLAGS += -I./src

# -----------------------------------------------------------------------------
# Platform detection
# -----------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    PLATFORM := win
    EXE      := .exe
    CFLAGS   += -DST_PLATFORM_WINDOWS
    LDFLAGS  := -lkernel32 -luser32 -lgdi32
    # TODO(UC3): add -ld2d1 -ldwrite -lole32 -luuid when Direct2D is active
else
    PLATFORM := linux
    EXE      :=
    CFLAGS   += -DST_PLATFORM_LINUX
    LDFLAGS  := -lpthread
    # TODO(UC3): add -lX11 -lXft -lfontconfig when X11 is active
endif

# -----------------------------------------------------------------------------
# Directory layout
# -----------------------------------------------------------------------------
BUILD   := build
RELEASE := release
TESTS   := tests
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
UC_TARGETS := $(patsubst $(UC_DIR)/%.c, $(TESTS)/%$(EXE), $(UC_FILES))

# -----------------------------------------------------------------------------
# Top-level targets
# -----------------------------------------------------------------------------
.PHONY: all run tests clean

all: $(BUILD) $(RELEASE) $(TESTS) $(TARGET) $(UC_TARGETS)

$(BUILD) $(RELEASE) $(TESTS):
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
$(TESTS)/%$(EXE): $(UC_DIR)/%.c $(LIB_OBJS)
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
	rm -rf $(BUILD) $(RELEASE) $(TESTS)
