CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Wno-unused-parameter
CFLAGS += -include src/common.h
CFLAGS += -fno-pie
LDFLAGS = -no-pie
LIB_LDFLAGS = -Wl,-rpath="$(CURDIR)" -L. -lgbit
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
GBIT_DIR ?= build/gbit
SHELL_HACK := $(shell mkdir -p $(GBIT_DIR))

BIN = build/jitboy
GBIT_BIN = build/gbit.out
OBJS = core.o gbz80.o lcd.o memory.o emit.o interrupt.o optimize.o audio.o save.o
OBJS := $(addprefix $(OUT)/, $(OBJS))
GBIT_OBJS = gbit/gbit.o gbit/test_cpu.o
GBIT_OBJS := $(addprefix $(OUT)/, $(GBIT_OBJS))
JITBOY_OBJS = main.o
JITBOY_OBJS := $(addprefix $(OUT)/, $(JITBOY_OBJS))

GBIT_LIB = libgbit.so
GBIT_LIB_OBJS = tester.o inputstate.o ref_cpu.o disassembler.o
GBIT_LIB_OBJS := $(addprefix $(GBIT_DIR)/, $(GBIT_LIB_OBJS))

deps := $(OBJS:%.o=%.o.d)
deps += $(GBIT_OBJS:%.o=%.d)
deps += $(JITBOY_OBJS:%.o=%.o.d)
deps += $(GBIT_LIB_OBJS:%.o=%.d)

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

gbit: CFLAGS += -O3 -DGBIT
gbit: LDFLAGS += -O3
gbit: $(GBIT_LIB) $(GBIT_BIN) 

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

$(GBIT_LIB): $(GBIT_LIB_OBJS)
	$(CC) $^ -o $@ -shared

$(BIN): $(OBJS) $(JITBOY_OBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

$(GBIT_BIN): $(GBIT_LIB) $(OBJS) $(GBIT_OBJS) 
	$(CC) $^ $(LDFLAGS) -o $@ $(LIB_LDFLAGS) $(LIBS)

$(GBIT_DIR)/%.o: gbit/lib/%.c
	$(CC) -O2 -Wall -Wextra -g -MMD -fPIC -c -o $@ $<

$(GBIT_DIR)/%.o: gbit/src/%.c
	$(CC) $(CFLAGS) -MMD -fPIC -I. -c -o $@ $<

$(OUT)/%.o: src/%.c
	$(CC) -o $@ -c $(CFLAGS) $< -MMD -MF $@.d

$(OUT)/%.o: $(OUT)/%.c
	$(CC) -o $@ -c $(CFLAGS) $< -MMD -MF $@.d

LuaJIT/src/host/minilua.c:
	git submodule update --init
	touch $@

$(OUT)/minilua: LuaJIT/src/host/minilua.c
	$(CC) -o $@ $^ -lm

$(OUT)/emit.c: src/emit.dasc $(OUT)/minilua src/dasm_macros.inc
	$(OUT)/minilua LuaJIT/dynasm/dynasm.lua $(DYNASMFLAGS) -I src -o $@ $<

clean:
	$(RM) $(BIN) $(GBIT_BIN) $(deps)
	$(RM) $(JITBOY_OBJS) $(GBIT_OBJS) $(OBJS)
	$(RM) $(OUT)/minilua $(OUT)/emit.c
	$(RM) $(GBIT_LIB)
	$(RM) $(GBIT_LIB_OBJS) $(GBIT_LIBDIR)/*.d

-include $(deps)
