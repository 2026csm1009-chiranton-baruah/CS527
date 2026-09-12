from pathlib import Path

patch_text = """--- memory.c
+++ memory.c
@@ -249,9 +249,6 @@
     if (pageTable[proc_id][logical_page] > 0)
         return pageTable[proc_id][logical_page];
 
-    if (!swap_has_page(proc_id, logical_page))
-        return -1;
-
     int victim_proc = -1;
     int victim_page = -1;
     int frame = allocate_page_frame(&victim_proc, &victim_page);
@@ -259,28 +256,35 @@
     if (frame < 0)
         return -1;
 
-    if (swap_read_page(proc_id,
-                       logical_page,
-                       &memory[frame * PAGESIZE]) != 0) {
-        /* Roll back the replacement instead of calling freePage() on a frame
-         * that has not yet been committed to the incoming page. */
-        if (valid_proc(victim_proc) && valid_logical_page(victim_page)) {
-            if (swap_read_page(victim_proc,
-                               victim_page,
-                               &memory[frame * PAGESIZE]) == 0) {
-                pageTable[victim_proc][victim_page] = frame;
-                pageDirty[victim_proc][victim_page] = 0;
-                frameOwnerProc[frame] = victim_proc;
-                frameOwnerPage[frame] = victim_page;
-                frameStamp[frame] = nextStamp++;
-                tlb_insert(victim_proc, victim_page, frame);
-                return -1;
+    if (swap_has_page(proc_id, logical_page)) {
+        if (swap_read_page(proc_id,
+                           logical_page,
+                           &memory[frame * PAGESIZE]) != 0) {
+            /* Roll back the replacement instead of calling freePage() on a
+             * frame that has not yet been committed to the incoming page. */
+            if (valid_proc(victim_proc) && valid_logical_page(victim_page)) {
+                if (swap_read_page(victim_proc,
+                                   victim_page,
+                                   &memory[frame * PAGESIZE]) == 0) {
+                    pageTable[victim_proc][victim_page] = frame;
+                    pageDirty[victim_proc][victim_page] = 0;
+                    frameOwnerProc[frame] = victim_proc;
+                    frameOwnerPage[frame] = victim_page;
+                    frameStamp[frame] = nextStamp++;
+                    tlb_insert(victim_proc, victim_page, frame);
+                    return -1;
+                }
             }
+
+            /* No victim to restore: release the reserved frame. */
+            freePage(frame);
+            return -1;
         }
-
-        /* No victim to restore: release the reserved frame. */
-        freePage(frame);
-        return -1;
+    } else {
+        /*
+         * First access to a page that has never been backed by swap.
+         * Treat it as a demand-zero page.
+         */
+        memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
     }
 
-    /* Commit the replacement only after the incoming page is safely loaded. */
+    /* Commit the replacement only after the incoming page is safely loaded
+     * or demand-zero initialized. */
     pageTable[proc_id][logical_page] = frame;
     pageDirty[proc_id][logical_page] = 0;
     frameOwnerProc[frame] = proc_id;
"""

path = Path("/home/bumblebee/CS527/LABTEST_120926/page_in_demand_zero.patch")
path.write_text(patch_text)
print(path)
print("Patch lines:", len(patch_text.splitlines()))
