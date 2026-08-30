#ifndef OS_H
#define OS_H


/*
 * ============================================================
 * Operating System interface
 * ============================================================
 */


/*
 * Start the Mini OS and its interactive shell.
 */
void os_run(void);


/*
 * Start the Mini OS with an initial program.
 */
void os_run_with_program(const char *program);


#endif
