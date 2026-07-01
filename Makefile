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
# Auto-generate header dependency files (.d) so that modifying any
# .h triggers a rebuild of the affected .o without 'make clean'.
CFLAGS += -MMD -MP

# -----------------------------------------------------------------------------
# Platform detection
# Note: _WIN32 / __linux__ are automatically defined by GCC.
#       common.h uses them to define ST_PLATFORM_WINDOWS / LINUX.
#       PLATFORM here only controls which backend source directory to compile.
# -----------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    PLATFORM := win
    EXE      := .exe
    LDFLAGS  := -lkernel32 -luser32 -lgdi32 -ld2d1 -ldwrite -lole32 -luuid
else
    PLATFORM := linux
    EXE      :=
    LDFLAGS  := -lpthread -lm
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
# TEMP GUARD: limited to UC00/UC01 while use_case_15+ test strategy is updated.
# Extend by appending entries; restore full scan with the commented line below.
# UC_FILES := $(wildcard $(UC_DIR)/use_case_*.c)
UC_FILES   := $(UC_DIR)/use_case_00.c \
              $(UC_DIR)/use_case_01.c

SRC_OBJS   := $(patsubst src/%.c,          $(BUILD)/s_%.o, $(SRC_FILES))
PLAT_OBJS  := $(patsubst $(PLATFORM)/%.c,  $(BUILD)/p_%.o, $(PLAT_FILES))
ALL_OBJS   := $(SRC_OBJS) $(PLAT_OBJS)

# Library objects = everything except main.o (used by test programs)
LIB_OBJS   := $(filter-out $(BUILD)/s_main.o, $(ALL_OBJS))

# Special build of main.c with -DST_TESTING: compiles all funcions
# but excludes the real main() symbol, so a test executable that needs
# to drive the full app lifecycle can supply its own main() without a 
# symbol collision.
TEST_MAIN_OBJ := $(BUILD)/s_main_testing.o

# Final targets
TARGET     := $(RELEASE)/st4ever$(EXE)
UC_TARGETS := $(patsubst $(UC_DIR)/%.c, $(TDIR)/%$(EXE), $(UC_FILES))

# -----------------------------------------------------------------------------
# Manual test directory (same use_case_NN.c compiled with -DST_MANUAL_TEST)
# UC4: make manual runs interactive visual validation tests.
#
# Optional: make manual UC=04_2  →  build + run only use_case_04_2
#           make manual           →  build + run all (default)
# -----------------------------------------------------------------------------
MDIR           := manual
MANUAL_TARGETS := $(patsubst $(UC_DIR)/%.c, $(MDIR)/%$(EXE), $(UC_FILES))

ifdef UC
MANUAL_RUN := $(MDIR)/use_case_$(UC)$(EXE)
else
MANUAL_RUN := $(MANUAL_TARGETS)
endif

# -----------------------------------------------------------------------------
# Top-level targets
# -----------------------------------------------------------------------------
.PHONY: all run tests manual clean dd

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

# src/main.c, recompiled with -DST_TESTING -> build/s_main_testing.o
# Excludes the real main() symbol so test_app_main.c can supply its own.
$(TEST_MAIN_OBJ): src/main.c
	$(CC) $(CFLAGS) -DST_TEST_FWK -c $< -o $@

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
$(TDIR)/%$(EXE): $(UC_DIR)/%.c $(LIB_OBJS) $(TEST_MAIN_OBJ)
	$(CC) $(CFLAGS) $< $(TEST_MAIN_OBJ) $(LIB_OBJS) -o $@ $(LDFLAGS)
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
	    if [ $$STATUS -ne 0 ]; then FAILS=$$((FAILS + 1)); break; fi; \
	done; \
	echo ""; \
	echo "================================================================"; \
	echo " Results: $$FAILS test program(s) reported failure (stopped at first)"; \
	echo "================================================================"; \
	echo ""; \
	exit $$FAILS

# Manual test binaries: same use_case_NN.c with -DST_MANUAL_TEST.
# Run from project root; answer y/n for each visual/interactive question.
$(MDIR)/%$(EXE): $(UC_DIR)/%.c $(LIB_OBJS)
	mkdir -p $(MDIR)
	$(CC) $(CFLAGS) -DST_MANUAL_TEST $< $(LIB_OBJS) -o $@ $(LDFLAGS)
	@echo "  --> $@"

manual: all $(MANUAL_RUN)
	@echo ""
	@echo "================================================================"
	@echo " ST4Ever - Manual Validation Tests"
	@echo " Answer y/n for each visual/interactive test."
	@echo " (must be run from the project root directory)"
ifdef UC
	@echo " Running UC=$(UC) only  (make manual for all)"
endif
	@echo "================================================================"
	@FAILS=0; \
	for t in $(MANUAL_RUN); do \
	    echo ""; \
	    echo "  Running: $$t"; \
	    echo "  --------"; \
	    $$t; STATUS=$$?; \
	    if [ $$STATUS -ne 0 ]; then FAILS=$$((FAILS + 1)); fi; \
	done; \
	echo ""; \
	echo "================================================================"; \
	echo " Results: $$FAILS manual test program(s) reported failure"; \
	echo "================================================================"; \
	echo ""; \
	exit $$FAILS

# -----------------------------------------------------------------------------
# Include auto-generated header dependency files (produced by -MMD -MP).
# The leading '-' suppresses errors on first build when .d files don't
# exist yet.
-include $(ALL_OBJS:.o=.d)

# -----------------------------------------------------------------------------
# Detailed Description document (auto-generated, not versioned)
# Usage:
#   make dd               - all modules
#   make dd MODS=trace,CPU - selected modules only
#   make dd DEPTH=5       - deeper call-tree
# -----------------------------------------------------------------------------
DD_ARGS :=
ifdef MODS
DD_ARGS += --mods $(MODS)
endif
ifdef DEPTH
DD_ARGS += --depth $(DEPTH)
endif

dd:
	python tools/gen_dd.py --src src --plat $(PLATFORM) \
	    --out "4 - DD.md" $(DD_ARGS)

# -----------------------------------------------------------------------------
clean:
	rm -rf $(BUILD) $(RELEASE) $(TDIR) $(MDIR)
	rm -f out.txt st4ever_trace.log uc44_*.txt
