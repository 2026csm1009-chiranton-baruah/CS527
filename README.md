# CS527 Mini OS Simulator

A small operating-system and processor simulator implemented in C for the CS527 systems/OS laboratory work.

The project has evolved from the supplied processor/compiler/memory starter design into a multi-process Mini OS with:

- 4 simulated processors
- a byte-addressable 8 KiB physical memory
- 512-byte pages/frames
- per-process logical page tables
- demand paging and page replacement
- disk-backed swap space
- a simulated persistent disk image
- per-processor TLBs
- a three-level cache hierarchy
- a preemptive-style finite time-slice scheduler
- a non-blocking interactive shell
- a two-pass assembly-like compiler
- integer, logical, and vector instructions
- process-private program/data bytecode files
- stress testing with multiple concurrent programs

The implementation is intentionally defensive: loops over simulated hardware structures are bounded, instruction execution is time-sliced, and the OS scheduler has an absolute safety limit so that a pathological user program cannot keep the simulator running forever.

---

## 1. Project Overview

The simulator models the path from a source program down through compilation, instruction execution, virtual memory, translation caching, CPU caches, physical memory, and disk-backed paging.

The high-level flow is:

```text
Source program (.txt)
        |
        v
   compiler.c
        |
        |  four-byte bytecode
        v
  program.byte
        |
        v
      os.c
        |
        +--------------------+
        |                    |
        v                    v
   Task / Scheduler      Process files
        |              program_N.byte
        |              data_N.byte
        v
  processor.c
        |
        v
  Logical address
        |
        v
       TLB
        |
        | miss
        v
   Page table / paging
        |
        v
      Cache
        |
        v
  Physical memory
        |
        | page replacement / eviction
        v
      Swap
        |
        v
    disk.img
```

---

## 2. Main Components

| File | Responsibility |
|---|---|
| `main.c` | Program entry point; starts the Mini OS |
| `os.c` | Process management, shell, queues, scheduling, task lifecycle |
| `os.h` | OS public interface |
| `compiler.c` | Two-pass source-to-bytecode compiler |
| `compiler.h` | Compiler interface |
| `processor.c` | Simulated processor, instruction fetch/decode/execute |
| `processor.h` | Processor state and execution interface |
| `memory.c` | Physical memory, page tables, paging, page replacement |
| `memory.h` | Memory-management interface and constants |
| `tlb.c` | Per-processor translation lookaside buffers |
| `tlb.h` | TLB interface |
| `cache.c` | L1-I, L1-D, private L2 and shared L3 caches |
| `cache.h` | Cache configuration and interface |
| `swap.c` | Per-process disk-backed swap mapping |
| `swap.h` | Swap interface |
| `disk.c` | Simulated disk image and disk block allocation |
| `disk.h` | Disk interface and configuration |
| `Makefile` | Build and stress-test automation |

---

## 3. Hardware / Simulator Configuration

### Processors

The simulator uses:

```text
NP = 4
```

Each simulated processor has its own architectural state, including:

- Program Counter (`PC`)
- Stack Pointer (`SP`)
- Accumulator (`AC`)
- Instruction Register (`IR`)
- processor status
- base address
- termination state
- integer register file
- vector register file
- buffered process output

A processor is reset whenever a new process is dispatched to it.

### Physical Memory

```text
Physical memory : 8192 bytes = 8 KiB
Page size        : 512 bytes = 0.5 KiB
Physical frames  : 16
Reserved frame   : frame 0
```

Therefore:

```text
8192 / 512 = 16 frames
```

Frame 0 is permanently reserved, leaving the remaining physical frames available for processes.

### Logical Address Space

Each process has 10 logical pages:

```text
Logical pages 0-1 : instruction memory
Logical pages 2-9 : data memory
```

The corresponding logical regions are:

```text
Instruction space : 1024 bytes = 1KiB
Data space        : 4096 bytes = 4 KiB
```

The page table maps:

```text
logical page -> physical frame
```

An unmapped page is represented by `-1`.

---

## 4. Compiler

`compiler.c` implements a two-pass compiler.

### Pass 1

Pass 1:

1. Reads the source file.
2. Removes comments.
3. Identifies labels.
4. Validates labels.
5. Counts the number of generated instructions.
6. Detects statements that expand into multiple bytecode instructions.
7. Enforces the maximum supported program size.

A terminating HALT instruction is appended automatically.

### Pass 2

Pass 2:

1. Reprocesses each source line.
2. Resolves labels.
3. Generates four-byte instructions.
4. Writes hexadecimal bytecode to:

```text
program.byte
```

Each instruction occupies four bytes:

```text
[ opcode ][ destination ][ source 1 ][ source 2 ]
```

The compiler also supports instruction-relative branch offsets using a signed 8-bit offset.

### Source Language

The current compiler supports the implemented integer, logical, and vector instruction families, including:

- integer arithmetic
- immediate integer arithmetic
- integer memory load/store
- register/data movement
- printing
- conditional branches
- unconditional branches
- logical register operations
- logical immediate operations
- unary bitwise NOT
- shifts and shift-immediate operations
- vector arithmetic
- vector memory operations

Comments use `%`.

Labels begin with `.` and contain only alphanumeric characters after the leading dot.

Example:

```text
.L0
    ...
    BAL .L0
```

The compiler validates branch targets and rejects branch offsets outside the signed 8-bit range.

Integer constants accepted by the current source language are unsigned 8-bit values (`0` through `255`). Logical immediate operations therefore use zero-extended 8-bit immediates.

---

## 5. Processor

`processor.c` implements the simulated CPU.

The processor executes at most the number of instructions requested by:

```c
process_instructions(proc_id, instruction_count);
```

The OS currently uses:

```text
TIME_SLICE = 10 instructions
```

for each scheduler dispatch.

This is important for termination safety: even a program containing an infinite branch such as:

```text
.loop
    BAL .loop
```

does not permanently monopolize the simulator. The processor returns after its finite instruction budget.

### Instruction Encoding

The processor fetches four bytes for every instruction and reconstructs the instruction word in big-endian byte order.

The architecture contains:

```text
256 integer registers
256 vector registers
PC
SP
AC
IR
status
```

### Integer Operations

The implemented opcode families include:

```text
01  ADD
02  SUB
03  MUL
04  DIV

05  integer load
06  integer store
07  MOD
08  Print

09  ADD immediate
0A  SUB immediate
0B  MUL immediate
0C  DIV immediate
0D  MOD immediate
0F  load immediate
```

Division by zero is detected and treated as a processor error.

### Logical Operations

The ISA also implements bitwise logical operations:

```text
30  AND
31  OR
32  XOR
33  AND immediate
34  OR immediate
35  XOR immediate
36  NOT
```

The source-language forms are:

```text
x1 = x2 & x3
x1 = x2 | x3
x1 = x2 ^ x3

x1 = x2 & 15
x1 = x2 | 15
x1 = x2 ^ 15

x1 = ~x2
```

Register-to-register logical operations use the values in both source registers. Logical immediate operations use a zero-extended 8-bit immediate. `NOT` is a dedicated unary operation and computes the bitwise complement of its source register.

Logical instructions write their result to both the destination register and the accumulator (`AC`), allowing them to participate directly in the existing branch model.

These operations have been validated with edge cases and with non-trivial register-only algorithms, including an 8-bit parity routine and a population-count routine.

### Branches

The current conditional branch family is:

```text
10  BEQ
11  BNE
12  BGE
13  BLT
14  BGT
15  BLE
```

Unconditional branch:

```text
1E  BAL
```

Branch conditions are evaluated using the accumulator.

### Shift Operations

The processor also implements logical and arithmetic shifts:

```text
18  SLL
19  SRL
1A  SRA
1B  SLL immediate
1C  SRL immediate
1D  SRA immediate
```

The source language supports:

```text
x1 = x2 << x3
x1 = x2 >> x3
x1 = x2 >>> x3
```

and corresponding immediate shift forms.

### Vector Operations

Vector arithmetic includes:

```text
21  vector ADD
22  vector SUB
23  vector MUL

29  vector ADD immediate
2A  vector SUB immediate
2B  vector MUL immediate
```

Vector memory operations are also implemented.

### Print Buffering

Process output is buffered per processor so that multiple processes do not produce badly interleaved output.

The OS flushes a process's buffered output when the process terminates.

---

## 7. ISA Validation

The logical ISA extension has been validated at several levels.

### Compiler Validation

The compiler was checked with GCC syntax/warning analysis and with source programs containing:

- register `AND`, `OR`, and `XOR`
- immediate `AND`, `OR`, and `XOR`
- unary `NOT`
- combinations of logical operations with arithmetic and shifts

The generated bytecode uses the expected logical opcodes (`0x30` through `0x36`).

### Processor Validation

A simulator test covering the logical instructions produced the expected results for:

- ordinary positive values
- zero
- all-ones 8-bit values
- high-bit values
- signed results produced by bitwise operations
- immediate operands
- combinations of logical operations

### Algorithmic Validation

The logical instructions have also been exercised as building blocks for algorithms.

An 8-bit parity routine was tested using:

```text
x = x ^ (x >> 4)
x = x ^ (x >> 2)
x = x ^ (x >> 1)
parity = x & 1
```

The routine correctly produced parity results for multiple inputs, including `0`, `1`, `3`, `7`, `15`, `170`, `171`, and `255`.

A population-count routine was also tested. It repeatedly isolates the least-significant bit with `AND`, accumulates the result, and shifts the input until it becomes zero. The tested inputs produced:

```text
0   -> 0
1   -> 1
7   -> 3
15  -> 4
170 -> 4
255 -> 8
85  -> 4
240 -> 4
129 -> 2
```

The population-count program completed successfully under the full simulator, including its normal TLB and instruction-cache paths.

These tests demonstrate that the logical operations are not only decoded correctly but can be combined with existing arithmetic, shift, branch, and accumulator semantics to implement useful bit-level algorithms.

---

## 8. Operating System

`os.c` provides the operating-system layer.

The OS maintains:

- a process/task table
- processor-to-PID mappings
- a ready queue
- a waiting queue
- task states
- process-private program/data files
- an interactive shell

### Task States

```text
TASK_READY
TASK_RUNNING
TASK_WAITING
TASK_FINISHED
```

### Task Control Block

Each task stores:

```text
PID
processor ID
source filename
private program bytecode filename
private data bytecode filename
task state
```

### Process-Private Files

When a program is compiled, the compiler produces:

```text
program.byte
```

The OS then creates private copies:

```text
program_1.byte
data_1.byte

program_2.byte
data_2.byte

...
```

This prevents concurrently running tasks from overwriting one another's program/data images.

---

## 8. Scheduler

The scheduler uses a finite instruction time slice:

```text
TIME_SLICE = 10
```

Each scheduler round captures the current ready-queue length before processing it.

Therefore, tasks that are requeued after consuming their time slice are not immediately processed again within the same round.

Conceptually:

```text
scheduler_round()
    |
    +-- take current ready queue length
    |
    +-- for each task that was ready at round start
           |
           +-- execute <= 10 instructions
           |
           +-- finished? -> release processor
           |
           +-- still running? -> return to ready queue
```

### Waiting Queue

Only four processors exist, so additional processes enter the waiting queue.

When a running process finishes:

```text
processor becomes free
        |
        v
dispatch_waiting()
        |
        v
next waiting PID receives processor
```

This was exercised successfully by the 16-program stress test.

### Scheduler Safety Limit

The OS also contains:

```c
#define MAX_SCHEDULER_ROUNDS 1000000
```

The outer scheduler loop cannot run forever.

If this limit is reached, the OS reports:

```text
[OS] Scheduler safety limit reached; stopping simulation
```

and performs normal shutdown.

---

## 9. Interactive Shell

The shell uses non-blocking `select()` on standard input.

Supported commands are:

```text
program.txt data.byte
```

Compile and run a program with a data image.

```text
program.txt
```

Compile and run a program with an empty data image.

```text
ps
```

Display the process table.

```text
disk
```

Display simulated disk allocation status.

```text
exit
```

Stop accepting new programs while allowing existing tasks to finish.

EOF is treated as an exit command.

The shell does not block the scheduler while waiting for user input.

---

## 10. Virtual Memory and Paging

`memory.c` implements the process virtual-memory layer.

The page table is:

```text
pageTable[NP][NUM_LOGICAL_PAGES]
```

with:

```text
NP = 4
NUM_LOGICAL_PAGES = 10
```

### Address Translation

For an instruction access:

```text
logical page = address / 512
offset       = address % 512
```

For a data access:

```text
logical page = address / 512 + 2
offset       = address % 512
```

The physical address is:

```text
physical address = physical frame * 512 + offset
```

### TLB First

Translation first checks the TLB:

```text
TLB lookup
    |
    +-- hit  -> physical frame
    |
    +-- miss -> page_in()
```

This avoids repeatedly walking the page table for recently used mappings.

### Demand Paging

If a logical page is not resident:

```text
page_in()
    |
    +-- obtain physical frame
    |
    +-- read page from swap
    |
    +-- establish page-table mapping
    |
    +-- insert mapping into TLB
```

### Page Replacement

Physical frames maintain reverse ownership information:

```text
frameOwnerProc
frameOwnerPage
frameStamp
```

When no free frame is available, a resident page can be selected for replacement.

Dirty pages are written back to their swap backing before eviction.

### Dirty Pages

Data writes mark the corresponding logical page dirty.

When a dirty page is evicted:

```text
RAM
 |
 +-- cache write-back
 |
 +-- swap_write_page()
 |
 v
disk-backed swap slot
```

---

## 11. Disk and Swap

### Simulated Disk

The disk subsystem uses:

```text
Disk size       : 65536 bytes = 64 KiB
Block size      : 512 bytes = 0.5 KiB
Number of blocks: 128
```

The backing file is:

```text
disk.img
```

The disk maintains a block allocation bitmap and persistent metadata.

Block 0 is reserved for disk metadata.

### Swap

Swap is implemented as disk-backed slots.

Each process can have one swap slot for each of its ten logical pages.

The swap layer maintains:

```text
swap_table[NP][SWAP_NUM_LOGICAL_PAGES]
```

Operations include:

- allocate a swap slot
- locate a page's swap block
- test whether a page has backing storage
- read a page from disk
- write a page to disk
- free a swap slot
- release all slots belonging to a process

The memory subsystem therefore has a clean separation:

```text
memory.c
    |
    v
swap.c
    |
    v
disk.c
    |
    v
disk.img
```

---

## 12. TLB

Each simulated processor has its own TLB.

The current design uses:

```text
8 TLB entries per processor
```

A TLB entry contains:

```text
valid
logical page
physical frame
timestamp
```

The timestamp is used to implement an LRU-style replacement policy.

### TLB Invalidation

The TLB supports:

```text
invalidate one process-local page
invalidate every TLB entry referring to a physical frame
flush one processor
flush the complete TLB
```

Frame-based invalidation is important when physical frames are reused for different pages.

### Statistics

The simulator reports:

```text
hits
misses
hit rate
```

per processor and globally.

---

## 13. Cache Hierarchy

The simulator contains a three-level write-back cache hierarchy.

```text
                 +----------------+
                 |    L1-I       |
                 | 128 B / core  |
                 +----------------+
                         |
                         v
                 +----------------+
                 |    L1-D       |
                 | 128 B / core  |
                 +----------------+
                         |
                         v
                 +----------------+
                 | Private L2    |
                 | 256 B / core  |
                 +----------------+
                         |
                         v
                 +----------------+
                 | Shared L3     |
                 |    512 B      |
                 +----------------+
                         |
                         v
                 Physical memory
```

More precisely, instruction fetches use the private L1 instruction cache, data accesses use the private L1 data cache, and misses proceed through the private L2 and shared L3 hierarchy.

### Configuration

```text
Cache line size : 16 bytes

L1-I            : 128 B/core
L1-D            : 128 B/core
Private L2      : 256 B/core
Shared L3       : 512 B
```

All cache levels are direct-mapped.

The cache policy is:

```text
Write-back
Write-allocate
```

### Total Cache Capacity

Across four processors:

```text
4 * (128 + 128 + 256) + 512
= 2560 bytes = 2.5 KiB
```

### Cache / Paging Separation

The cache hierarchy operates on physical memory.

It does not directly access the disk.

Paging remains responsible for:

```text
RAM <-> swap <-> disk
```

while caching handles:

```text
CPU <-> physical memory
```

### Dirty Cache Lines

Before a physical frame is evicted or reused, cache lines belonging to that frame are written back.

This is critical for maintaining correctness when paging and caching interact.

The cache layer provides frame-oriented operations for:

```text
cache_writeback_frame()
cache_invalidate_frame()
```

### Coherence

The cache implementation invalidates conflicting copies in other processors' L1 caches when a write changes a physical cache line.

This prevents stale private-cache copies from surviving a cross-core write.

### Cache Statistics

The simulator reports:

```text
L1 instruction hits/misses
L1 data hits/misses
L2 hits/misses
L3 hits/misses
```

with hit rates.

---

## 14. Memory / Cache / TLB Access Path

A typical instruction fetch follows:

```text
Processor
   |
   v
logical instruction address
   |
   v
TLB lookup
   |
   +---- hit ----------------------+
   |                              |
   +---- miss                     |
          |                       |
          v                       |
       page_in()                  |
          |                       |
          v                       |
      page table                  |
          |                       |
          +-----------+-----------+
                      |
                      v
              physical address
                      |
                      v
                   L1-I
                      |
                 miss |
                      v
                   L2
                      |
                 miss |
                      v
                   L3
                      |
                 miss |
                      v
              physical memory
```

A data access follows the same translation path, but enters the L1 data cache.

If the page is not resident, the memory subsystem brings it in from disk-backed swap before the physical address is returned.

---

## 15. Initialization and Shutdown

### Initialization

The OS initializes:

1. task state
2. processor-to-task mappings
3. ready/waiting queues
4. physical memory
5. page tables
6. simulated disk
7. swap mappings
8. TLBs
9. caches
10. processor state

### Shutdown

The OS:

1. finishes processes still assigned to processors
2. removes process-private files
3. reports TLB statistics
4. reports cache statistics
5. releases process memory
6. releases swap slots
7. finalizes caches/TLBs/swap/disk

The statistics are deliberately printed before the corresponding subsystems are finalized and their counters cleared.

---

## 16. Building

The project uses GCC and C11.

Current compiler flags:

```text
-Wall -Wextra -std=c11 -O2
```

Build everything with:

```bash
make
```

The resulting executable is:

```text
./simulator
```

To remove generated object files, executable, bytecode, disk image and other generated artifacts:

```bash
make clean
```

---

## 17. Makefile Targets

### Build

```bash
make
```

Builds:

```text
simulator
```

from:

```text
main.o
compiler.o
processor.o
memory.o
disk.o
os.o
tlb.o
swap.o
cache.o
```

### Stress Test

The Makefile includes a multi-process stress test:

```bash
make stress-test
```

The stress workload launches 16 programs:

```text
matmul.txt
a3.txt
fib.txt
a4.txt
eitch.txt
mulmul.txt
cross.txt
f2.txt
eitch.txt
cross.txt
matmul.txt
f2.txt
a3.txt
mulmul.txt
a4.txt
fib.txt
```

followed by:

```text
exit
```

This deliberately exceeds the four available processors and therefore exercises the waiting queue and process dispatch logic.

---

## 18. Running the Simulator Manually

After building:

```bash
./simulator
```

The shell starts with:

```text
Mini OS started with 4 processors.
$
```

Example:

```text
a3.txt data.byte
```

The compiler creates `program.byte`, after which the OS creates a process-private bytecode image and loads the process.

Multiple programs can be entered before:

```text
exit
```

After `exit`, the shell stops accepting new processes but existing processes continue until completion.

---

## 19. Example Stress-Test Results

The current 16-program stress test completed successfully.

The tested workload produced the expected results for the programs used in the stress test, including:

### `a3.txt`

```text
11
13
15
17
19
21
23
25
```

### `a4.txt`

```text
-9
-9
-9
-9
-9
-9
-9
-9
```

### `matmul.txt`

```text
125
136
133
162
174
151
279
300
265
```

### `fib.txt`

```text
0
1
1
2
3
5
8
13
21
34
```

### `cross.txt`

```text
-3
6
-3
```

### `f2.txt`

```text
528
```

### `eitch.txt`

```text
1
2
4
7
12
20
33
54
88
143
232
376
609
986
1596
2583
```

`mulmul.txt` completed successfully without producing user-visible output.

The stress run completed all 16 processes without a scheduler hang or `std::bad_alloc`.

---

## 20. Current TLB Test Result

One complete 16-program stress run produced:

```text
proc0: hits=3683, misses=13, hit rate=99.65%
proc1: hits=2620, misses=4,  hit rate=99.85%
proc2: hits=3313, misses=11, hit rate=99.67%
proc3: hits=2734, misses=6,  hit rate=99.78%

total: hits=12350, misses=34, hit rate=99.73%
```

These results demonstrate substantial temporal locality in the process page mappings.

---

## 21. Current Cache Test Result

The same stress run produced:

```text
L1 Instruction Cache
hits   = 9818
misses = 222
hit rate = 97.79%

L1 Data Cache
hits   = 1706
misses = 78
hit rate = 95.63%

Private L2 Cache
hits   = 0
misses = 300

Shared Unified L3 Cache
hits   = 0
misses = 300
```

The zero L2/L3 hit count in this particular stress workload does not imply that those levels are non-functional. Separate controlled cache tests have demonstrated L2 and L3 hits.

The stress workload has substantial process churn, physical-frame reuse and paging activity, so its lower-level cache reuse pattern differs from the dedicated cache tests.

---

## 22. Validation and Testing

Testing has been performed at several levels.

### Compilation

The project is compiled with:

```text
gcc -Wall -Wextra -std=c11 -O2
```

The current `os.c` task-string handling uses bounded `snprintf()` calls, avoiding the GCC `-Wstringop-truncation` warning previously produced by the `strncpy(..., size - 1)` pattern.

### Processor / Infinite-Loop Safety

The processor executes a finite instruction count per call.

The OS also has:

```text
TIME_SLICE = 10
MAX_SCHEDULER_ROUNDS = 1000000
```

so pathological programs cannot create an unbounded scheduler loop.

### Paging

Paging has been exercised under multi-process workloads where physical memory cannot hold all resident process pages simultaneously.

### TLB

TLB hits, misses and invalidation are tracked per simulated processor.

### Cache

The cache hierarchy has been tested independently with workloads designed to exercise:

- L1 hits
- L1 misses
- L2 hits
- L3 hits
- repeated instruction access

### Stress Test

The 16-program stress test exercises:

- four-way processor concurrency
- ready queue
- waiting queue
- process dispatch
- process cleanup
- paging
- swap
- disk allocation
- TLB
- caches
- processor reset/reuse
- compiler-generated bytecode
- multiple instances of the same program

---

## 23. Generated Files

The simulator may generate:

```text
program.byte
program_<pid>.byte
data_<pid>.byte
disk.img
*.o
simulator
```

The process-private program/data files are removed when their tasks finish.

`make clean` removes the main generated build/runtime artifacts.

---

## 24. Design Principles

Several implementation choices are deliberate.

### Bounded Execution

The simulator avoids unbounded execution wherever possible.

Examples:

```text
finite instruction time slices
finite scheduler rounds
finite hardware-structure loops
bounded file reads
bounded input parsing
```

### Separation of Responsibilities

The implementation separates:

```text
Compiler
Processor
OS
Virtual Memory
TLB
Cache
Swap
Disk
```

rather than putting all simulator functionality into one file.

### Process Isolation

Each process receives:

```text
private program bytecode
private data backing
private logical page mappings
private TLB address-space context
```

### Explicit Error Handling

Invalid processor IDs, logical pages, physical frames, memory addresses, registers, branch targets and disk blocks are checked at subsystem boundaries.

---

## 25. Repository Structure

A typical working directory is:

```text
ASSIGNMENT_5_5_5_5/
├── README.md
├── Makefile
│
├── main.c
│
├── os.c
├── os.h
│
├── compiler.c
├── compiler.h
│
├── processor.c
├── processor.h
│
├── memory.c
├── memory.h
│
├── tlb.c
├── tlb.h
│
├── cache.c
├── cache.h
│
├── swap.c
├── swap.h
│
├── disk.c
├── disk.h
│
├── *.txt
│
├── *.byte              # generated
├── program_*.byte      # generated
├── data_*.byte         # generated
├── disk.img            # generated
├── *.o                 # generated
└── simulator           # generated
```

Generated files should generally not be committed unless they are intentionally required as test artifacts.

---

## 26. Limitations / Notes

This is a simulator rather than a real operating system.

In particular:

- processors are simulated in software
- memory is represented by arrays
- the disk is represented by `disk.img`
- page tables are simple in-memory structures
- the TLB is simulated
- caches are simulated
- scheduling is cooperative at the simulator instruction-slice boundary
- the shell uses the host operating system's standard input
- bytecode is generated and consumed entirely within the simulator environment

The project therefore aims to demonstrate operating-system and computer-architecture mechanisms rather than reproduce a production OS kernel.

---

## 27. Academic Context

The design follows the supplied CS527 laboratory architecture:

```text
compiler
   |
processor
   |
memory
   |
operating system
```

and extends that basic structure with:

```text
TLB
paging
swap
disk
multi-process scheduling
multi-processor execution
cache hierarchy
```

The original laboratory specification describes 4 processors, 8192-byte physical memory, 512-byte pages, 10 logical pages per process, four-byte instructions and the separation of compiler, processor, memory and OS components.

---

## 28. ISA Quick Reference

The current instruction encoding is fixed-width:

```text
[ opcode ][ destination ][ source 1 ][ source 2 ]
```

Each instruction is four bytes. The currently documented opcode groups are:

```text
00        HALT
01-0C     integer arithmetic / memory / print / immediate arithmetic
0F        load immediate
10-15     conditional branches
18-1D     shifts
1E        BAL
21-23     vector arithmetic
25-26     vector memory
29-2B     vector immediate arithmetic
30-36     bitwise logical operations
```

The complete dispatch table remains defined by the implementation in `processor.c`; this section summarizes the currently documented instruction families rather than claiming that every opcode value between the ranges above is implemented.

---

## 29. Quick Start

Clone/open the repository and enter the project directory:

```bash
cd ~/CS527/LABTEST_12092026
```

Build:

```bash
make
```

Run:

```bash
./simulator
```

Or run the complete multi-process stress test:

```bash
make stress-test
```

Clean generated artifacts:

```bash
make clean
```
