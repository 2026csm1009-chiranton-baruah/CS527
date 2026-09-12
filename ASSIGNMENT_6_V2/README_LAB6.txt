CS527 Lab 6 + Extra Hardware Integration
==========================================

This tree retains the Lab 6 implementation and adds the supplied hardware
modules without removing the Lab 6 "limbs":

Lab 6 retained:
- Source-to-bytecode mapping and per-source-instruction frequency reports.
- Memory-resident page tables in physical frame 0.
- Per-processor PTBR registers.
- Multi-processor OS scheduling and finite time slices.
- Existing compiler, processor, memory, OS, and Makefile test flow.

Extra hardware integrated:
- cache.c/cache.h: direct-mapped L1-I, L1-D, private L2, shared L3,
  with write-back/write-allocate behavior and frame invalidation.
- tlb.c/tlb.h: per-processor 8-entry TLB with LRU-style replacement,
  lookup statistics, and invalidation on page-table/frame changes.
- disk.c/disk.h: 64 KiB disk image with 512-byte blocks and allocation bitmap.
- swap.c/swap.h: disk-backed logical-page slots.
- Memory paging: resident pages are translated through the Lab 6 page table;
  a defined non-resident page is paged in from swap. When physical frames are
  exhausted, a victim page is written to swap before its frame is reused.
  Data pages are preferred as victims so instruction pages remain resident
  whenever possible.
- Processor instruction/data accesses now traverse the cache hierarchy after
  virtual-to-physical translation.
- Cache/TLB/Disk/Swap statistics are printed when the OS shuts down.

Build:
    make

Lab 6 regression test:
    make test

Hardware/paging stress test:
    make hardware-stress

The stress test launches four large processes concurrently, forcing physical
frame pressure and exercising page-out/page-in, cache activity, TLB activity,
and the existing Lab 6 frequency-report path.

Expected result:
    Lab 6 + hardware tests passed (...)
    Hardware paging stress passed

Known compiler warnings are inherited from the Lab 6 tree (unused
save_data_file and strncpy truncation warnings); the build completes
successfully with no errors.


New integer ISA instructions
----------------------------
The integrated compiler and processor now support:

- MOD: register modulo, `x1 = x2 MOD x3` (opcode 0x07).
- MODI: immediate modulo, `x1 = x2 MOD 6` (opcode 0x0D).
- Integer shifts: `<<`, `>>`, `>>>` with register or immediate shift amounts (0x18-0x1D).
- Integer logical operations: `&`, `|`, `^` with register or immediate operands, plus `~` (0x30-0x36).
- Register move: `MV x1 x2`, `MOVE x1 x2`, or `x1 = x2` (opcode 0x0E).

The new ISA test program is `tests/newisa.txt`; `make test` runs it in addition to the existing Lab 6 and hardware tests.
