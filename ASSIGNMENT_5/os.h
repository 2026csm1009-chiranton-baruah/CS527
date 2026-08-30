#ifndef OS_H
#define OS_H

/*
 * ============================================================
 * Operating System Interface
 * ============================================================
 *
 * The OS layer is responsible for:
 *
 *   - OS initialization
 *   - process/task creation
 *   - process loading
 *   - processor assignment
 *   - ready/waiting queues
 *   - round-robin scheduling
 *   - shell interaction
 *   - process cleanup
 *
 * The implementation is contained in os.c.
 */


/*
 * ============================================================
 * Start OS with interactive shell
 * ============================================================
 *
 * Initializes the processors and memory system, then starts
 * the non-blocking shell/scheduler loop.
 *
 * The OS continues until:
 *
 *   1. the user enters "exit", and
 *   2. all active processes have finished.
 *
 * A hard scheduler-round limit is also present in os.c as a
 * final safety mechanism against an unexpected infinite loop
 * inside the OS itself.
 */
void os_run(void);


/*
 * ============================================================
 * Start OS with an initial program
 * ============================================================
 *
 * program:
 *
 *     source program to compile and load at startup.
 *
 * After the initial program is loaded, the interactive shell
 * remains available for additional programs.
 */
void os_run_with_program(const char *program);


/*
 * ============================================================
 * Shut down OS
 * ============================================================
 *
 * Releases processors, process memory, task resources and
 * other OS-managed resources.
 *
 * This function is implemented by os.c and is called after
 * the scheduler terminates.
 */
void shutdown_os(void);

#endif
