# jitboy

A Game Boy emulator with dynamic recompilation (JIT) for x86-64.

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
"dynarec" or called just-in-time (JIT) compiler - is used in many emulators
because of its potentially higher speed.

The instructions are translated mostly dynamically at runtime, since
static analysis is difficult - e.g. by tracking all possible execution
paths from a known entry jump point. Self-modifying code and jumping to
addresses calculated at runtime often make a fallback to interpretation or
dynamic translation at runtime necessary in the case of static translation.

The emulator `jitboy` carries out a dynamic translation of the processor
instructions. All other interfaces (graphics, sound, memory) are additionally
emulated by interpreting the address space.

## Game Boy Specification

| Component    | Detail                                                 |
|------------- |--------------------------------------------------------|
| CPU          | 8-bit (Similar to the Z80 processor)                   |
| Clock Speed  | 4.194304MHz (4.295454MHz for SGB, max. 8.4MHz for CGB) |
| Work RAM     | 8K Byte (32K Byte for CGB)                             |
| Video RAM    | 8K Byte (16K Byte for CGB)                             |
| Screen Size  | 2.6"                                                   |
| Resolution   | 160x144 (20x18 tiles)                                  |
| Max sprites  | Max 40 per screen, 10 per line                         |
| Sprite sizes | 8x8 or 8x16                                            |
| Palettes     | 1x4 BG, 2x3 OBJ (for CGB: 8x4 BG, 8x3 OBJ)             |
| Colors       | 4 grayshades (32768 colors for CGB)                    |
| Horiz Sync   | 9198 KHz (9420 KHz for SGB)                            |
| Vert Sync    | 59.73 Hz (61.17 Hz for SGB)                            |
| Sound        | 4 channels with stereo sound                           |
| Power        | DC6V 0.7W (DC3V 0.7W for GB Pocket, DC3V 0.6W for CGB) |


## CPU

The main processor of Game Boy is a Sharp LR35902, a mix between the Z80 and
the Intel 8080 that runs at 4.19 MHz. It is usually called as "GBZ80", however,
it is not a Z80 compatible processor, nor a 8080 compatible processor.

The Z80 is an 8-bit microprocessor, meaning that each operation is natively
performed on a single byte. The instruction set does have some 16-bit
operations but these are just executed as multiple cycles of 8-bit logic.
The Z80 has a 16-bit wide address bus, which logically represents a 64K memory
map. Data is transferred to the CPU over an 8-bit wide data bus but this is
irrelevant to simulating the system at state machine level. The Z80 and the
Intel 8080 that it derives from have 256 I/O ports for accessing external
peripherals but the Game Boy CPU has none - favouring memory mapped I/O (MMIO)
instead.

| Type           | CPU Speed | NOP Instruction |
|----------------|-----------|-----------------|
| Machine Cycles | 1.05MHz   | 1 cycle         |
| Clock Cycles   | 4.19MHz   | 4 cycles        |

Notice, 1 Machine Cycle = 4 clock cycles.

### Registers

The Intel 8080 and Game Boy CPU have six 8-bit general purpose registers, an
accumulator, flags, stack pointer and program counter. 16-bit access is also
provided to each general purpose register and the accumulator and flags registers
in sequential pairs. Additionally, the Z80 has two more 16-bit index registers,
an alternative set of each general purpose, accumulator and flags registers and
a few more bits and pieces.

The Game Boy CPU has one bank of general purpose 8-bit registers: `A`, `B`, `C`,
`D`, `E`, `F`, `H` and `L`.

While the CPU only has 8 bit registers, there are instructions that allow the
game to read and write 16 bits (i.e. 2 bytes) at the same time. These registers
are refered to as `AF` ("a" and "f" combined), `BC` ("b" and "c" combined),
`DE` ("d" and "e" combinded), and finally `hl` ("h" and "l" combined).

| Register | Size                | Purpose                           |
|----------|---------------------|-----------------------------------|
| AF       | 16-bit or two 8-bit | Accumulator (A) and flag bits (F) |
| BC       | 16-bit or two 8-bit | Data/address                      |
| DE       | 16-bit or two 8-bit | Data/address                      |
| HL       | 16-bit or two 8-bit | Accumulator/address               |
| SP       | 16-bit              | Stack pointer                     |
| PC       | 16-bit              | Program counter                   |

The Z80 defines alternative/banked versions of `AF`, `BC`, `DE` and `HL` that are
accessed via the exchange opcodes and also has some more specialized registers.

| Register | Size                | Purpose                        |
|----------|---------------------|--------------------------------|
| IX       | 16-bit or two 8-bit | Displacement offset base       |
| IY       | 16-bit or two 8-bit | Displacement offset base       |
| I        | 8-bit               | Interrupt vector base register |
| R        | 8-bit               | DRAM refresh counter           |

The flags register is a single byte that contains a bit-mask set according to
the last result. Notice that the Game Boy flags register only uses the most
significant 4-bits and does not implement the sign or parity/overflow flag.
The least significant bits of the Game Boy flags register are always 0.

| 8080/Z80 Bit | Game Boy Bit | Name            |
|--------------|--------------|-----------------|
| 0            | 4            | C: Carry        |
| 1            | 6            | N: Subtract     |
| 2            | -            | Parity/Overflow |
| 3            | -            | Undocumented    |
| 4            | 5            | H: Half Carry   |
| 5            | -            | Undocumented    |
| 6            | 7            | Z: Zero         |
| 7            | -            | Sign            |

### Core

A CPU runs on a fetch-decode-execute cycle, called the machine cycle or m-cycle.
The CPU will initially fetch a byte, whose location in the address space is pointed
to by the program counter register (PC), decode it as an instruction (opcode) and
execute it, or contextually use it as a literal for a previous cycle. Opcodes not
related to absolute program flow, such as jumps or calls, will end a cycle by
incrementing the program counter to point at the next byte in the address space.
Opcode length is variable and whilst some operations run in a single cycle, others
require multiple fetch-decode-execute cycles to run.
Here is an example of running three simple opcodes on a Z80:

![Example fetch-decode-execute on the Z80](/assets/cpu.svg)

We are not really concerned with this low level cycle as software cannot control
it, but we do need to keep track of how many have occurred so that we have a mechanism
to match (read: approximate) platform timing. Our higher level cycle will be based on
a concept of an operation, which can be represented by one or more opcodes and optional
literals.

Each operation cycle will:
1. Fetch the next opcode.
2. Decode the fetched opcode.
3. Fetch any extra data required to resolve the operation including extra opcodes
   and literals.
4. Record all m-cycles consumed in the operation so that we can block later to
   adjust our timings.
5. Execute the opcode.

### Instructions

Instruction length can be 1 to 4 bytes long depending on the specific instruction.
Opcodes can be seen as 9 bits long, and will be encoded into 1 or 2 bytes. If the
first byte is `0xCB`, then the second byte would be one of the high 256 opcodes,
otherwise, the first byte is one of the low 256 opcodes.

For example, if the first byte is `0x43`, then the opcode of this instruction is
`0x043`; if the first byte is `0xCB` and the next byte is the `0x43`, then the
opcode of this instruction is `0x143`.

After the opcode, there can be a optional immediate, 8-bit or 16-bit long, gives
the total length of 1 to 4 bytes.

### Execution Timing

The processor runs at either 4 MiHz (4194304 Hz = 2^12 Hz) or 8 MiHz (Double Speed
Mode on GBC). The instruction execution time is always dividable by 4, ranging from
4 cycles to 20 cycles. Ususally a clock cycle at 4 MiHz is called a T-cycle.
4 T-cycles combined together is called a M-cycle (1 MiHz). So, one instruction could
take 1 to 5 M-cycles to execute.

The processor can do one memory read or memory write in one M-cycle, since the
instruction itself needed be fetched, the execution speed can never be faster than
the speed it can read the instruction. For example, a 3 byte instruction needs at
least 3 M-cycles (12 T-cycles) to execute. If the instruction involves memory read
or write, the processor would have to spend more M-cycles just to access the memory.

The processor is also only capable of doing 1 8-bit ALU operation each M-cycle,
if the instruction need to do 16-bit ALU operation, additional 1 M-cycle may be
needed to complete the operation.

The processor also has a prefetch queue with the length of 1 byte.

## Interrupts

The Game Boy provides a total of five different interrupts:
* `VBLANK`
  - The `VBLANK` interrupt is displayed after each image displayed and marks the
    beginning of the VBLANK phase in which the video memory can be freely accessed
    for 4560 clock cycles.
* `STAT`
  - The `STAT` register (memory address `0xFF41`) changes between three states with
    each displayed image line and to a fourth during the `VBLANK` phase. The `STAT`
    interrupt can be triggered when these states change. Which state transitions are
    affected can be selected.
* `Timer`
  - The timer interrupt is triggered when the timer register (`0xFF05`) overflows.
    The rate at which the timer register is incremented can be selected so that the
    timer interrupt occurs at a selectable rate of 16Hz, 64Hz, 256Hz or 1kHz.
* Serial
  - The serial transfer interrupt is triggered when a serial transfer is completed.
* Joypad
  - Every time one of the eight buttons is pressed, the joypad interrupt is triggered.

If an interrupt occurs, it becomes pending and a bit is set in the interrupt flag
register (`0xFF0F`). The interrupt enable register (`0xFFFF`) can be used to select
which interrupts are active. The interrupt master enable flag can also all turn off
interrupts. It can be manipulated with the instructions `DI` (Disable Interrupts),
`EI` (Enable Interrupts) or `RETI` (Return from Interrupt).

If an interrupt is pending, the corresponding bit in the Interrupt Enable Register
and the Interrupt Master Enable flag are set, a handler function with a fixed start
address between `0x40` (`VBLANK`) and `0x60` (Joypad) is called and further interrupts
are prevented during the treatment using the Interrupt Master Enable .

## Memory and Memory Mapped I/O Devices

The relationship between CPU, memory management unit (MMU), memory and memory
mapped I/O (MMIO) devices looks something like the following.

![Simple diagram of the Game Boy MMU](/assets/mmu.svg)

An MMU should support reading and writing data in various lengths across the
entire address space, whilst abstracting away the hardware that is physically
attached to each location in the space.

We can implement an MMU in a platform agnostic way by introducing a concept of
segments. A segment has a location and length so that the MMU can correctly position
it in address space and will provide implementation specific data access operations.
For example, most Game Boy cartridges have a microcontroller acting as a memory bank
controller (MBC) over multiple banks of read only memory (ROM). Read requests for data
in an MBC address space will be forwarded to a configured page of ROM, whereas write
requests will change which page is configured. For this reason we really need
different interfaces for readable and writeable segments.

### Memory Map

16-bit addressing to ROM, RAM, and I/O registers.

| Address   | Usage                                                        |
|-----------|--------------------------------------------------------------|
| 0000-3FFF | 16KB ROM Bank 00 (in cartridge, fixed at bank 00)            |
| 4000-7FFF | 16KB ROM Bank 01..NN (in cartridge, switchable bank number)  |
| 8000-9FFF | 8KB Video RAM (VRAM) (switchable bank 0-1 in CGB Mode)       |
| A000-BFFF | 8KB External RAM (in cartridge, switchable bank, if any)     |
| C000-CFFF | 4KB Work RAM Bank 0 (WRAM)                                   |
| D000-DFFF | 4KB Work RAM Bank 1 (WRAM) (switchable bank 1-7 in CGB Mode) |
| E000-FDFF | Same as C000-DDFF (ECHO) (typically not used)                |
| FE00-FE9F | Sprite Attribute Table (OAM)                                 |
| FEA0-FEFF | Not Usable                                                   |
| FF00-FF7F | I/O Ports                                                    |
| FF80-FFFE | High RAM (HRAM)                                              |
| FFFF      | Interrupt Enable Register                                    |

The addresses between `0x8000` and `0x9FFF` form the video RAM. It contains 8 × 8 pixel
tiles of 16 bytes each, as well as foreground and background tile maps.

The cartridge RAM is displayed between `0xA000` and `0xBFFF`. Depending on the MBC,
several banks can be swapped. In some game cartridges, this memory is supplied by
a battery and can therefore hold a game status even when the Game Boy is switched off.

This is followed by 8kB of internal RAM (`0xC000` - `0xDFFF`), which is almost
completely mirrored a second time in the address range `0xE000` - `0xFDFF`. However,
these addresses are typically not used. The addresses `0xFE00` to `0xFE9F` contain the
OAM memory. It contains the position, the graphic to be displayed, the grayscale
palette used and the flags of all 40 sprites. The OAM memory can be simultaneously
overwritten via DMA transfer.

The hardware IO is controlled via the address range `0xFF00` to `0xFF7F`. It contains
registers for controlling timers, serial transfers, DMA transfers, sound output and
the map area to be displayed. This is followed by a further 127 bytes of main memory
(`0xFF80` - `0xFFFE`), which can be read and written at any time. Since all other
memory can neither be read nor written during a DMA transfer, a jump must be made to
this memory area during such a transfer.

The interrupt enable register occupies the highest address 0xFFFF.

### Jump Vectors in First ROM Bank

The following addresses are supposed to be used as jump vectors:
* 0000,0008,0010,0018,0020,0028,0030,0038 for RST commands
* 0040,0048,0050,0058,0060 for Interrupts

However, the memory may be used for any other purpose in case that your program
does not use any (or only some) `RST` commands or Interrupts.
RST commands are 1-byte opcodes that work similiar to `CALL` opcodes, except that
the destination address is fixed.

### Internal RAM Echo

The addresses E000-FE00 appear to access the internal RAM the same as C000-DE00.
(i.e. If you write a byte to address E000 it will appear at C000 and E000. Similarly,
writing a byte to C000 will appear at C000 and E000.)

### Cartridge Header in First ROM Bank

The memory at 0100-014F contains the cartridge header.
This area contains information about the program, its entry point, checksums,
information about the used MBC chip, the ROM and RAM sizes, etc. Most of the
bytes in this area are required to be specified correctly.

### External Memory and Hardware

The areas from 0000-7FFF and A000-BFFF may be used to connect external hardware.
The first area is typically used to address ROM (read only, of course), cartridges
with Memory Bank Controllers (MBCs) are additionally using this area to output data
(write only) to the MBC chip.

The second area is often used to address external RAM, or to address other external
hardware (Real Time Clock, etc). External memory is often battery buffered, and may
hold saved game positions and high scrore tables (etc.) even when the Game Boy is
turned of, or when the cartridge is removed.

## JIT Compilation

For the emulation of the Game Boy hardware on conventional PCs (x86-64
architecture) a JIT compiling emulation core was implemented.
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
* The emulated Game Boy address space must be consecutive: the change of ROM or RAM
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

## Graphics

The pixels of the Game Boy display cannot be addressed individually, rather
whole tiles of 8 × 8 pixels each are displayed. In addition to a foreground
and background map (called `WIN` and `BG`) that contain the indices of the tiles
to be displayed, up to 40 sprites can be freely positioned on the display.

The image is built up line by line from top to bottom. The line currently being
processed can be read out via register `LY` (0xFF44) and via the `STAT` Register
(`0xFF41`) whether access to the graphics memory is currently possible.

The size of the foreground and background map is 32 by 32 tiles, so that only
a section is visible on the display. Via the register `SCX` (Scroll X, `0xFF43`),
`SCY` (Scroll Y, `0xFF42`), `WX` (Window X, `0xFF4B`) and `WY` (Window Y, `0xFF4A`),
the area to be displayed can be selected. By changing the visible area while the
image is being built up, wave effects can be created on the display.

![Determination of the brightness value of a background pixel](/assets/background-tiles.svg)

The above figure shows an example of how the color of a background pixel comes about:
First of all, the tile indices for the currently drawn image line are determined from
the Background Tile Map; this can be selected from either `0x9800` or `0x9C00`.
The tile data table is indexed from `0x8000` or `0x8800` via this index. The brightness
value of the x-th pixel of the y-th tile line can then be built up from the x-th bit of
the 2 * y-th and 2 * y + 1-th bytes. The structure for a foreground pixel is analogous.
When displaying sprites, the OAM memory is used instead of a tilemap: It contains
a 4-byte structure for each of the 40 sprites, which contains the tile index and some
flags in addition to the screen position. These flags can be used to mirror the sprite,
display it behind the background or with a different grayscale palette.

Since the graphics output of the Game Boy takes place via special control registers
as well as defined memory areas for tilemaps, the emulator must interpret these
memory areas and generate the corresponding image pixel by pixel. It is not enough
to interpret the memory once at the beginning of the `VBLANK` period and to output
the image, since many games use the display timing to create graphic effects.
If these are to be displayed correctly, the image must also be generated line by line
in the emulator.

After each executed instruction block, the `LY` (approx. Every 450 clock cycles) and
the `STAT` register (approx. Every 80, 180, 190 clock cycles) are updated in the course
of interrupt handling. If the `LY` register is incremented, the next image line can be
drawn. After 144 lines have been drawn, at the beginning of the `VBLANK` period, the
generated image can finally be passed on to the rendering thread for display. A separate
rendering thread relieves the main thread of slow updating of the image texture and
its display and halves the runtime of the main thread per frame.
> With each processed line, the `STAT` register runs through three modes of different duration.

The start of the `VBLANK` period is also used to limit the speed: If less than 1/60 s has
passed since the last `VBLANK`, there is a correspondingly long wait before the execution
is continued.


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

Runtime options:
* `-O` specifies the optimization levels. Typically, you can use `-O 3`

To enable extra debugging information, you can rebuild the emulator.
```shell
make clean debug
```

Then, the verbose messages will be dumped when `jitboy` loads and runs the given ROM file.
Meanwhile, the files whose name starts with `/tmp/jitcode` will be generated along with JIT
compilation. You can disassemble them by the command.
```shell
objdump -D -b binary -mi386 -Mx86-64 /tmp/jitcode?
```

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
* Only works for GNU/Linux


## Reference

* [DMG-01: How to Emulate a Game Boy](https://blog.ryanlevick.com/DMG-01/public/book/)
* [Emulation of Nintendo Game Boy (DMG-01)](https://raw.githubusercontent.com/Baekalfen/PyBoy/master/PyBoy.pdf)


## License

`jitboy` is licensed under the MIT License.
