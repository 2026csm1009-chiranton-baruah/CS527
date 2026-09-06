#ifndef OS_H
#define OS_H

/*
 * ============================================================
 * Operating System interface
 * ============================================================
 */

/*
 * Start the Mini OS and its interactive shell.
 *
 * The OS initializes the processor set, virtual memory, and
 * disk-backed paging subsystem before entering the scheduler.
 */
void os_run(void);

/*
 * Start the Mini OS with an initial source program.
 *
 * The normal shell remains available after the initial program
 * has been loaded.
 */
void os_run_with_program(const char *program);

#endif
