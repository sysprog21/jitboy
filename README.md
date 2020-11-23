# jitboy

A Game Boy emulator with dynamic recompilation (JIT) for x86\_64.

## Overview

Since most of the games published for the Game Boy are only available in
binary form as ROM images, porting to current systems is excluded.
An alternative is the emulation of the Game Boy architecture: a runtime
environment that is as exactly the same as the Game Boy and is able to
execute the unmodified program is provided.  Due to the incompatible
processor architecture, the instruction sequence of the emulated programms
cannot be executed directly: either they are interpreted instruction by
instruction, i.e. the fetch execute cycle is carried out in software, or
it is translated into compatible instructions. The second method - often
"dynarec" or called JITcompiler - is used in many emulators because of
its potentially higher speed.

The instructions are translated mostly dynamically at runtime, since
static analysis is difficult - e.g. by tracking all possible execution
paths from a known entry jump point. Self-modifying code and jumping to
addresses calculated at runtime often make a fallback to interpretation or
dynamic translation at runtime necessary in the case of static translation.

The emulator "jitboy" carries out a dynamic translation of the processor
instructions. All other interfaces (graphics, sound, memory) are additionally
emulated by interpreting the address space.

## JIT Compilation

For the emulation of the Game Boy hardware on conventional PCs (x86-64
architecture) a Just-In-Time (JIT) compiling emulation core was implemented.
Instead of decoding and interpreting individual instructions in a loop, as
with an interpreting emulator, an attempt is made to combine entire blocks
that usually end with a jump instruction (JP, JR, CALL, RST, RET, RETI).
By means of the [DynASM](https://luajit.org/dynasm.html) runtime assembler of
the [LuaJIT](https://luajit.org/) project, x86 instructions corresponding to
the block are generated and executed at the first point in time at which
a memory address is jumped to.

One goal during development was to use the status flags (carry, half-carry /
adjust and zero) of the host architecture for the emulated environment instead
of emulating it. In most cases this is possible without any problems, since
the Z80-like Game Boy CPU [LR35902](https://pastraiser.com/cpu/gameboy/gameboy_opcodes.html)
and the Intel 8080 architecture, which is largely also supported by modern
processors, are very similar. Since the subtract flag of the Game Boy has no
direct equivalent in the x86-64 architecture, it is the only one of the status
flags that has to be emulated.

Jumps are not executed directly, but instead the jump target is saved and
the generated function is exited with `RET`. This allows the runtime environment
to first compile the block at the jump target and perform other parallel tasks,
including interrupt, graphics, input and DMA emulation.

During the compilation of a program block, the number of Game Boy clock cycles
required up to this point is calculated for each possible end over which the
block can be exited, and this sum is added to an instruction counter during
execution. By means of this counter, events that occur on the Game Boy at
certain times, such as timer or `VBLANK` interrupts, can be precisely timed
despite the higher speed of the host platform. Since there may be routines in
the emulated programs that are dependent on a fixed number of executed
instructions in a certain period of time, the timers of the host system cannot
be used without compatibility problems. Due to the block-wise execution, however,
there is also the problem with the emulator presented here that interrupts or
timers are only executed or updated a few clock pulses late - after the next jump.

During the execution of compiled program blocks, the register set of the Game Boy
is mapped directly to registers of the x86-64 architecture. At the end of a block,
the entire Game Boy register set, processor flags and the number of emulated clock
cycles must be saved (struct `gb_state`). The following table shows the register
usage during the execution of translated blocks. The combined registers `AF`, `BC`,
`DE` and `HL` required for the 16-bit instructions of the Game Boy are first put
together in a temporary register and written back after the instruction.

| Game Boy | x86-64   | comment |
|----------|----------|---------|
| A        | r0 (rax) | accumulator |
| F        | -        | generated dynamically from the `FLAGS` register |
| B        | r1 (rcx) | |
| C        | r2 (rdx) | |
| D        | r3 (rbx) | |
| E        | r13      | |
| H        | r5 (rbp) | |
| L        | r6 (rsi) | |
| SP       | r7 (rdi) | |
| PC       | -        | not necessary |
| -        | r8       | base address of Game Boy address space |
| -        | r9       | address of strct `gb_state` |
| -        | r10      | temporary register |
| -        | r11      | temporary register |
| -        | r12      | temporary register |
| -        | r4 (rsp) | host stack pointer |

A second important goal of the implementation was the support of direct read
memory access: to read an address of the Game Boy address space, only one
additional addition of a base pointer should be necessary. This is not possible
for write memory accesses, since write accesses to addresses in the ROM lead to
bank changes by the MBC and some IO registers trigger certain actions such as
DMA transfers or reading the joypad buttons during write accesses. Write access
is therefore replaced by a function call that emulates any necessary side effects.

Direct read access has some important implications:
* Hardly any reading overhead: Compared to the Game Boy, there is hardly any reading
  overhead with the emulation. Since reading memory accesses are often among the most
  frequent instructions, this means a significant increase in efficiency.
* The emulated Gameboy address space must be consecutive: the change of ROM or RAM
  banks requires a lot of additional effort, as the corresponding bank must first
  be mapped into the address space by munmap and mmap.
* Status registers must always be updated: the program sequence must be interrupted
  frequently in order to update special status registers such as the TIMA timer
  (`0xFF05`) or the currently drawn image line LY (`0xFF44`). If this does not
  happen, queues may no longer terminate.

### Exemplary translation of a block

The individual steps for translating and executing a program block should be
illustrated by an example: The following listing shows a block from the game
"Super Mario World".
```
3E 02		LD A, 2
EA 00 20	LD (0x2000), A
E0 FD		LDH (0xFD), A
FA 1D DA	LD A, (0xDA1D)
FE 03		CP A, 3
20 0B		JR NZ, 0xOB
3E FF		LD A, 0xFF
EA 1D DA	LD (0xDA1D), A
CD E8 09	CALL 0x9E8
```

In the first step, instructions are read to the end of the block. Every unconditional
jump (`JP`, `CALL`, `RST`, `RET`, `RETI`), as well as `EI` (Enable Interrupts)
terminate a block. The instructions are stored in a linked list and grouped according
to their type. Various rules for optimization are applied to this instruction list,
and instructions for saving and restoring the status register are inserted. Then the
appropriate x86-64 assembler is generated - the example is translated to the following
code (without optimization):
```
    prologue
    mov A, 2
    write_byte 0x2000, A
    write_byte 0xfffd, A
    mov A, [aMem + 0xda1d]
    cmp A, 3
    save_cc
    restore_cc
    jz >1
    add qword state->inst_count, 17
    return 0x239
1:  mov A, 0xff
    write_byte 0xda1d, A
    dec SP
    dec SP
    and SP, 0xffff
    mov word [aMem + SP], 0x235
    add qword state->inst_count, 28
    mov byte state->return_reason, REASON_CALL
   return 0x9e8
```

Some macros are used for simplification:
* `prologue` saves all necessary registers and restores the Game Boy register contents.
* `return` saves all register contents in the `gb_state` struct, restores the original
  register contents, writes the argument in the result register and exits the function
  with RET.
* `write_byte` calls the function `gb_memory_write`.
* `save_cc` saves the status register on the stack.
* `restore_cc` restores the status register from the stack.

`aMem` designates the register `r8`, which contains the base address of the Game Boy
address space, state the register `r9`, which contains the address of the `gb_state`.
`state->inst_count` counts executed Game Boy clock cycles, `state->trap_reason`
specifies which instruction terminates the block in order to update the backtrace in
the debugger. However, the debugger is not yet implemented.

In the next step, `DynASM` is used to assemble the code and convert it into a
to write a pre-allocated memory area. After this can be carried out using mprotect the
function can be executed. To run again accelerate, the function pointer is stored
indexed via the start address. The memory address returned by the function is the
start address of the next block to be executed.
> If an interrupt occurs, it is instead placed on the Game Boy stack and the start
> address of the interrupt handler is jumped to.

If a memory address within the RAM is jumped to, it must be assumed that the sequence
of instructions has changed during the next execution. Blocks within the RAM are
therefore discarded again after execution. Blocks within the addresses `0xFF80` to
`0xFFFE` are an exception: a jump into this area must be made briefly during DMA
transfers. The routine that waits for the transfer to end does not usually change
during the execution of the program. It is therefore worthwhile to temporarily store
the blocks until there is a write access to this memory area.

### Optimization

After reading an instruction block, some rules for optimization are applied. Loops
interrupt the program flow very frequently with a large number of jumps and thus
cause a very high overhead for saving and restoring the register contents and for
checking for interrupts.  For this reason, most of the implemented optimizations
are for the detection and handling of loops.

The easiest way to recognize loops is by jumping to the start address of the
current block, since a new block is translated from this start address after
the first iteration and the return to the beginning of the loop.

If there is no read or write memory access in the loop body and all interrupts
return with RET or RETI, the entire loop can be executed atomically. In this case
it is irrelevant whether an interrupt is executed before, during or after the loop.
Simple waiting loops that wait a fixed number of clock cycles can be accelerated
in this way: the return to the beginning of the block is carried out directly
without relinquishing control to the runtime environment and checking for
interrupts in the meantime.

Writing memory accesses can also usually be carried out safely. In this case,
however, an interrupt handler can possibly be influenced by the loop and behave
incorrectly due to the additionally executed loop iterations. Reading memory
access, on the other hand, carries a considerable risk: a waiting loop waiting
for an interrupt or timer may no longer terminate. Since read access to timer
and status registers is usually carried out with special instructions
(`LDH A, (a8)` or `LDH A, (C)`), other read instructions are allowed in loops
in higher optimization levels.

The following loop executes a `memset` on a memory area of length `BC` with
end address `HL` and can be executed without interruption with the above
optimizations:
```
32		LD (HL-), A	; Set byte and decrement HL
05		DEC B
20 FC		JR NZ, 0xFC	; jump to the beginning
0D		DEC C
20 F9		JR NZ, 0xF9	; jump to the beginning
```

Other optimizations use pattern matching to search for known and frequent
instruction sequences that can be simplified. The following frequently used
pattern waits until a specific line of the display is drawn.
> A `VBLANK` can also be waited for if the line is > 144.

```
F0 44		LDH A, (0x44)	; read current display line
FE ??		CP A, ??	; compare with a fixed value
20 FA		JR NZ, 0xFA	; jump to the beginning
```

Instead, a modified HALT instruction can be inserted in the emulation, which
waits for the corresponding display line to be drawn instead of an interrupt.

## Build

`jitboy` relies on some 3rd party packages to be fully usable and to
provide you full access to all of its features. You need to have a working
[SDL2 library](https://www.libsdl.org/) on your target system.
* macOS: `brew install sdl2`
* Ubuntu Linux / Debian: `sudo apt install libsdl2-dev`

Build the emulator.
```shell                                                                                                                                                      
make
```

`build/jitboy` is the built executable file, and you can use it to load Game Boy
ROM files.

## Key Controls

| Action            | Keyboard   |
|-------------------|------------|
| A                 | z          |
| B                 | x          |
| Start             | Return     |
| Select            | Backspace  |
| D-Pad             | Arrow Keys |

## Known Issues
* No audio support
* Only works for GNU/Linux with clang
    - `clang-6` and `clang-10` are known to work.
    - gcc can build `jitboy`, but the generated executable file fails to work.

## License

`jitboy` is licensed under the MIT License.
