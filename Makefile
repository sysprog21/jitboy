CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Wno-unused-parameter
CFLAGS += -include src/common.h
CFLAGS += -fno-pie
LDFLAGS = -no-pie
LIBS = -lm

# SDL2
CFLAGS += `sdl2-config --cflags`
LIBS += `sdl2-config --libs`

# Control the build verbosity
ifeq ("$(VERBOSE)","1")
    Q :=
    VECHO = @true
else
    Q := @
    VECHO = @printf
endif

OUT ?= build
SHELL_HACK := $(shell mkdir -p $(OUT))

BIN = build/jitboy
INSTR_TEST_BIN = build/instruction-test
OBJS = core.o gbz80.o lcd.o memory.o emit.o interrupt.o optimize.o audio.o save.o

JITBOY_OBJS = main.o
JITBOY_OBJS += $(OBJS)
JITBOY_OBJS := $(addprefix $(OUT)/, $(JITBOY_OBJS))

INSTR_TEST_OBJS = instr_test.o tester.o inputstate.o ref_cpu.o disassembler.o
INSTR_TEST_OBJS += $(OBJS)
INSTR_TEST_OBJS := $(addprefix instr-test-, $(INSTR_TEST_OBJS))
INSTR_TEST_OBJS := $(addprefix $(OUT)/, $(INSTR_TEST_OBJS))

INSTR_TEST_LIB_C := gbit/lib/tester.c gbit/lib/inputstate.c \
	gbit/lib/ref_cpu.c gbit/lib/disassembler.c

deps += $(JITBOY_OBJS:%.o=%.o.d)
deps += $(INSTR_TEST_OBJS:%.o=%.o.d)

GIT_HOOKS := .git/hooks/applied

all: CFLAGS += -O3
all: LDFLAGS += -O3
all: $(BIN) $(GIT_HOOKS)

sanitizer: CFLAGS += -Og -g -fsanitize=thread
sanitizer: LDFLAGS += -fsanitize=thread
sanitizer: $(BIN)

debug: CFLAGS += -g -D DEBUG
debug: DYNASMFLAGS += -D DEBUG
debug: $(BIN)

check: CFLAGS += -O3 -DINSTRUCTION_TEST -I.
check: LDFLAGS += -O3
check: INSTR_TEST_PREFIX = instr-test-
check: $(INSTR_TEST_BIN) 
	$(INSTR_TEST_BIN) 

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

$(BIN): $(JITBOY_OBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

$(INSTR_TEST_BIN): $(INSTR_TEST_LIB_OBJS) $(INSTR_TEST_OBJS)
	$(CC) $^ $(LDFLAGS) -o $@ $(LIBS)

$(OUT)/instr-test-%.o: gbit/lib/%.c
	$(CC) -o $@ -c $(CFLAGS) $< -MMD -MF $@.d

$(OUT)/instr-test-%.o: tests/%.c
	$(CC) -o $@ -c $(CFLAGS) $< -MMD -MF $@.d

.SECONDEXPANSION:
$(OUT)/%.o: $$(subst $$(INSTR_TEST_PREFIX),,src/%.c)
	$(CC) -o $@ -c $(CFLAGS) $< -MMD -MF $@.d

$(OUT)/%.o: $$(subst $$(INSTR_TEST_PREFIX),,$(OUT)/%.c)
	$(CC) -o $@ -c $(CFLAGS) $< -MMD -MF $@.d

$(INSTR_TEST_LIB_C):
	git submodule update --init
	touch $@

LuaJIT/src/host/minilua.c:
	git submodule update --init
	touch $@

$(OUT)/minilua: LuaJIT/src/host/minilua.c
	$(CC) -o $@ $^ -lm

$(OUT)/emit.c: src/emit.dasc $(OUT)/minilua src/dasm_macros.inc
	$(OUT)/minilua LuaJIT/dynasm/dynasm.lua $(DYNASMFLAGS) -I src -o $@ $<

clean:
	$(RM) $(BIN) $(INSTR_TEST_BIN) $(deps)
	$(RM) $(JITBOY_OBJS) $(INSTR_TEST_OBJS)
	$(RM) $(OUT)/minilua $(OUT)/emit.c

-include $(deps)
