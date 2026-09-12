from pathlib import Path

src = Path("memory.c")
dst = Path("memory.c")

text = src.read_text()

# Clean up the indentation in the already-applied demand-zero page-in change.
text = text.replace(
"""        if (swap_read_page(proc_id,
                           logical_page,
                           &memory[frame * PAGESIZE]) != 0) {
        /* Roll back the replacement instead of calling freePage() on a frame
         * that has not yet been committed to the incoming page. */
        if (valid_proc(victim_proc) && valid_logical_page(victim_page)) {
            if (swap_read_page(victim_proc,
                               victim_page,
                               &memory[frame * PAGESIZE]) == 0) {
                pageTable[victim_proc][victim_page] = frame;
                pageDirty[victim_proc][victim_page] = 0;
                frameOwnerProc[frame] = victim_proc;
                frameOwnerPage[frame] = victim_page;
                frameStamp[frame] = nextStamp++;
                tlb_insert(victim_proc, victim_page, frame);
                return -1;
            }
        }

        /* No victim to restore: release the reserved frame. */
        freePage(frame);
        return -1;
        }
""",
"""        if (swap_read_page(proc_id,
                           logical_page,
                           &memory[frame * PAGESIZE]) != 0) {
            /* Roll back the replacement instead of calling freePage() on a frame
             * that has not yet been committed to the incoming page. */
            if (valid_proc(victim_proc) && valid_logical_page(victim_page)) {
                if (swap_read_page(victim_proc,
                                   victim_page,
                                   &memory[frame * PAGESIZE]) == 0) {
                    pageTable[victim_proc][victim_page] = frame;
                    pageDirty[victim_proc][victim_page] = 0;
                    frameOwnerProc[frame] = victim_proc;
                    frameOwnerPage[frame] = victim_page;
                    frameStamp[frame] = nextStamp++;
                    tlb_insert(victim_proc, victim_page, frame);
                    return -1;
                }
            }

            /* No victim to restore: release the reserved frame. */
            freePage(frame);
            return -1;
        }
"""
)

dst.write_text(text)
print(f"Created corrected {dst}")
