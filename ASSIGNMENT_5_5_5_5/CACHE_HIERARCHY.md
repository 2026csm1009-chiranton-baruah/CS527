# Mini OS Cache Hierarchy

## Configuration

The simulator now models a three-level write-back, write-allocate cache hierarchy:

- **L1 Instruction:** 128 B per core, direct-mapped
- **L1 Data:** 128 B per core, direct-mapped
- **L2:** 256 B per core, unified and private, direct-mapped
- **L3:** 512 B total, unified and shared by all cores, direct-mapped
- **Cache line:** 16 B
- **Processors:** 4

That gives each core 512 B of private cache capacity and 512 B of shared L3 capacity.

## Data path

```text
CPU
  |
  +-- L1-I 128 B
  |
  +-- L1-D 128 B
          |
          v
     Private L2 256 B/core
          |
          v
     Shared L3 512 B
          |
          v
    Physical memory[]
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

L1, L2 and L3 are write-back/write-allocate. Dirty data moves downward only when a line is replaced, a frame is being reclaimed, or the hierarchy is explicitly flushed.

## Paging/cache integration

`memory.c` calls the cache hierarchy for process byte reads and writes. Before a physical frame is written to swap or reused, `cache_writeback_frame()` drains dirty cache lines belonging to that frame through the hierarchy and into `memory[]`.

`cache_invalidate_frame()` then removes stale copies from L1, L2 and L3 before the physical frame is reused.

## Statistics

The simulator reports separate hit/miss statistics for:

- L1 instruction cache per core
- L1 data cache per core
- Private L2 per core
- Shared unified L3

This makes it possible to observe whether an L1 miss is satisfied by the private L2, the shared L3, or physical memory.
