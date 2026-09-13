# PTBR implementation for Lab 6 Task 2

This version replaces the simulator's array-based `pageTable[NP][NUM_LOGICAL_PAGES]` with page tables stored directly in reserved physical frame 0.

## Design

- `PTBR[NP]` is added to the processor state.
- `PTBR[p]` contains the physical byte address of the first PTE for the task mapped to processor `p`.
- Each page-table entry is one byte containing a physical frame number.
- Entry value `0` means "not resident" because physical frame 0 is reserved.
- Processor `p` gets the non-overlapping page-table region:
  `PTBR[p] = p * NUM_LOGICAL_PAGES`.
- `getPhysicallAddress()` reaches the page table through `PTBR` (via `page_table_get()`); it no longer indexes a simulator-side page-table array.
- Page-in, page-out, replacement, loader initialization, and process release all update/read the physical-memory page table.
- The existing `pageDirty`, reverse-frame mappings, free-frame list, TLB, swap, and cache mechanisms remain intact; they are not page-table storage.
- `reset()` deliberately does not overwrite PTBR. The memory subsystem owns PTBR lifetime: loading a task installs it, and releasing a task clears it.

## Files changed

- `memory.c`
- `memory.h`
- `processor.c`
- `processor.h`

No other subsystem needs to know how the page table is physically represented.
