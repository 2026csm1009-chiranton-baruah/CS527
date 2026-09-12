#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "os.h"
#include "compiler.h"
#include "memory.h"
#include "processor.h"
#include "cache.h"
#include "tlb.h"
#include "swap.h"
#include "disk.h"


/*
 * ============================================================
 * OS configuration
 * ============================================================
 */

#define MAX_TASKS       128
#define MAX_LINE        256
#define TIME_SLICE      10
#define QUEUE_CAPACITY  MAX_TASKS

/*
 * Absolute safety limit for the OS scheduler.
 *
 * Under normal operation the OS exits long before this value.
 * This exists so that even a pathological state in the simulator
 * cannot keep the OS alive forever.
 */
#define MAX_SCHEDULER_ROUNDS 1000000


/*
 * ============================================================
 * Task states
 * ============================================================
 */

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_FINISHED
} TaskState;


/*
 * ============================================================
 * Task control block
 * ============================================================
 */

typedef struct {
    int pid;

    /*
     * Processor currently assigned to this task.
     *
     * -1 means the task is not currently assigned to a
     * processor.
     */
    int proc_id;

    char source[MAX_LINE];
    char program_file[MAX_LINE];
    char data_file[MAX_LINE];
    char map_file[MAX_LINE];

    TaskState state;
} Task;


/*
 * ============================================================
 * Global OS state
 * ============================================================
 */

static Task tasks[MAX_TASKS];

static int task_count = 0;
static int next_pid = 1;

/*
 * proc_pid[p] = PID currently assigned to processor p.
 *
 * 0 means the processor is free.
 */
static int proc_pid[NP];

/*
 * Ready queue contains PIDs that currently have a processor.
 */
static int ready_queue[QUEUE_CAPACITY];
static int ready_count = 0;

/*
 * Waiting queue contains PIDs that are waiting for a
 * processor to become available.
 */
static int waiting_queue[QUEUE_CAPACITY];
static int waiting_count = 0;

static int shell_exit = 0;


/*
 * ============================================================
 * Queue operations
 * ============================================================
 */

static int queue_push(int *queue,
                      int *count,
                      int value)
{
    if (queue == NULL || count == NULL)
        return 0;

    if (*count >= QUEUE_CAPACITY) {
        fprintf(stderr,
                "[OS] Queue is full; PID %d was rejected\n",
                value);
        return 0;
    }

    queue[*count] = value;
    (*count)++;

    return 1;
}


static int queue_pop(int *queue,
                     int *count)
{
    if (queue == NULL || count == NULL)
        return -1;

    if (*count <= 0)
        return -1;

    int value = queue[0];

    /*
     * Shift the remaining finite queue contents left.
     */
    for (int i = 1; i < *count; i++)
        queue[i - 1] = queue[i];

    (*count)--;

    return value;
}


/*
 * ============================================================
 * Task lookup
 * ============================================================
 */

static Task *find_task(int pid)
{
    for (int i = 0; i < task_count; i++) {

        if (tasks[i].pid == pid)
            return &tasks[i];
    }

    return NULL;
}


/*
 * ============================================================
 * Processor lookup
 * ============================================================
 */

static int find_free_processor(void)
{
    for (int p = 0; p < NP; p++) {

        if (proc_pid[p] == 0)
            return p;
    }

    return -1;
}


/*
 * ============================================================
 * File-copy helper
 * ============================================================
 *
 * Used to give every process its own program/data bytecode
 * files.
 *
 * Returns:
 *
 *     0  success
 *    -1  failure
 * ============================================================
 */

static int copy_file(const char *source,
                     const char *destination)
{
    if (source == NULL || destination == NULL)
        return -1;

    FILE *src = fopen(source, "rb");

    if (src == NULL)
        return -1;

    FILE *dst = fopen(destination, "wb");

    if (dst == NULL) {
        fclose(src);
        return -1;
    }

    char buffer[4096];

    size_t bytes_read;

    /*
     * fread() terminates at EOF. Each iteration transfers a
     * finite 4096-byte block.
     */
    while ((bytes_read =
            fread(buffer, 1, sizeof(buffer), src)) > 0) {

        size_t bytes_written =
            fwrite(buffer, 1, bytes_read, dst);

        if (bytes_written != bytes_read) {

            fclose(src);
            fclose(dst);

            return -1;
        }
    }

    if (ferror(src)) {

        fclose(src);
        fclose(dst);

        return -1;
    }

    fclose(src);
    fclose(dst);

    return 0;
}


/*
 * ============================================================
 * Create a task
 * ============================================================
 *
 * The shell has already produced program.byte and data.byte.
 *
 * Each task receives private copies so that one task's program
 * and data do not overwrite another task's files.
 * ============================================================
 */

static int create_task(const char *program_source,
                       const char *data_source)
{
    if (program_source == NULL)
        return -1;

    if (task_count >= MAX_TASKS) {

        fprintf(stderr,
                "[OS] Maximum task count reached\n");

        return -1;
    }

    if (next_pid <= 0) {

        fprintf(stderr,
                "[OS] PID space exhausted\n");

        return -1;
    }

    int pid = next_pid++;

    char program_file[MAX_LINE];
    char data_file[MAX_LINE];
    char map_file[MAX_LINE];

    snprintf(program_file,
             sizeof(program_file),
             "program_%d.byte",
             pid);

    snprintf(data_file,
             sizeof(data_file),
             "data_%d.byte",
             pid);

    snprintf(map_file,
             sizeof(map_file),
             "program_%d.map",
             pid);

    /*
     * The compiler generates program.byte and program.map.
     */
    if (copy_file("program.byte",
                  program_file) != 0) {

        fprintf(stderr,
                "[OS] Could not create %s\n",
                program_file);

        return -1;
    }

    if (copy_file("program.map",
                  map_file) != 0) {

        fprintf(stderr,
                "[OS] Could not create %s\n",
                map_file);
        remove(program_file);
        return -1;
    }

    /*
     * data.byte is optional according to the command-line
     * handling. If supplied, copy it.
     */
    if (data_source != NULL &&
        data_source[0] != '\0') {

        if (copy_file(data_source,
                      data_file) != 0) {

            fprintf(stderr,
                    "[OS] Could not copy data file %s\n",
                    data_source);

            remove(program_file);
            remove(map_file);

            return -1;
        }

    } else {

        /*
         * Create an empty data file.
         */
        FILE *fp = fopen(data_file, "wb");

        if (fp == NULL) {

            remove(program_file);
            remove(map_file);

            return -1;
        }

        fclose(fp);
    }


    Task *task = &tasks[task_count];

    memset(task, 0, sizeof(*task));

    task->pid = pid;
    task->proc_id = -1;

    /*
     * snprintf() provides bounded copies without the compiler's
     * -Wstringop-truncation warning that strncpy() can trigger when
     * the source may be exactly as large as the destination.
     */
    snprintf(task->source,
             sizeof(task->source),
             "%s",
             program_source);

    snprintf(task->program_file,
             sizeof(task->program_file),
             "%s",
             program_file);

    snprintf(task->data_file,
             sizeof(task->data_file),
             "%s",
             data_file);

    snprintf(task->map_file,
             sizeof(task->map_file),
             "%s",
             map_file);


    /*
     * Try to obtain a processor immediately.
     */
    int processor = find_free_processor();

    if (processor >= 0) {

        /*
         * Load the process into its logical/physical memory
         * mappings.
         */
        if (load_process_memory(processor,
                                task->program_file,
                                task->data_file) != 0) {

            fprintf(stderr,
                    "[OS] Failed to load PID %d into memory\n",
                    pid);

            remove(task->program_file);
            remove(task->data_file);
            remove(task->map_file);

            return -1;
        }

        reset(processor);

        task->proc_id = processor;
        task->state = TASK_READY;

        proc_pid[processor] = pid;

        task_count++;

        if (!queue_push(ready_queue,
                        &ready_count,
                        pid)) {

            unload_process_memory(processor);

            proc_pid[processor] = 0;
            task->proc_id = -1;

            remove(task->program_file);
            remove(task->data_file);
            remove(task->map_file);

            return -1;
        }

        printf("[OS] PID %d loaded on processor %d\n",
               pid,
               processor);

    } else {

        /*
         * All processors are busy.
         *
         * The task therefore enters the waiting queue.
         */
        task->state = TASK_WAITING;

        task_count++;

        if (!queue_push(waiting_queue,
                        &waiting_count,
                        pid)) {

            task_count--;

            remove(task->program_file);
            remove(task->data_file);
            remove(task->map_file);

            return -1;
        }

        printf("[OS] PID %d placed in waiting queue\n",
               pid);
    }

    return pid;
}


/*
 * ============================================================
 * Compile and load
 * ============================================================
 *
 * Input:
 *
 *     program.txt data.byte
 *
 * If the data filename is omitted, the process gets an empty
 * data image.
 * ============================================================
 */

static void compile_and_load(const char *source,
                             const char *data_file)
{
    if (source == NULL || source[0] == '\0')
        return;

    printf("[Compiler] compiling %s\n",
           source);

    compile(source);

    if (create_task(source,
                    data_file) < 0) {

        fprintf(stderr,
                "[OS] Failed to create task for %s\n",
                source);
    }
}


/*
 * ============================================================
 * Lab 6: instruction-frequency report
 * ============================================================
 */

static void make_frequency_filename(const char *source,
                                    int pid,
                                    char *output,
                                    size_t output_size)
{
    const char *name = source;

    if (source != NULL) {
        const char *slash = strrchr(source, '/');
        const char *backslash = strrchr(source, '\\');

        if (slash != NULL && slash + 1 > name)
            name = slash + 1;
        if (backslash != NULL && backslash + 1 > name)
            name = backslash + 1;
    }

    char base[MAX_LINE];
    strncpy(base, name != NULL ? name : "program", sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';

    char *dot = strrchr(base, '.');
    if (dot != NULL)
        *dot = '\0';

    if (base[0] == '\0')
        strcpy(base, "program");

    snprintf(output, output_size, "%s_%d.freq", base, pid);
}

static int write_frequency_report(const Task *task)
{
    if (task == NULL || task->proc_id < 0 || task->proc_id >= NP)
        return -1;

    FILE *map = fopen(task->map_file, "r");
    if (map == NULL)
        return -1;

    char output_name[MAX_LINE];
    make_frequency_filename(task->source, task->pid,
                            output_name, sizeof(output_name));

    FILE *out = fopen(output_name, "w");
    if (out == NULL) {
        fclose(map);
        return -1;
    }

    fprintf(out, "<frequency> <source instruction>\n");

    char line[MAX_LINE * 2];
    int last_source_line = -1;

    while (fgets(line, sizeof(line), map) != NULL) {
        int instruction_pc;
        int source_line;
        char source_text[MAX_LINE];

        if (sscanf(line, "%d\t%d\t%255[^\n]",
                   &instruction_pc, &source_line, source_text) != 3)
            continue;

        if (source_line == last_source_line)
            continue;

        if (instruction_pc < 0 ||
            instruction_pc >= (INSTRUCTION_SIZE / 4))
            continue;

        uint64_t frequency =
            get_instruction_execution_count(task->proc_id,
                                            instruction_pc);

        fprintf(out, "%llu %s\n",
                (unsigned long long)frequency, source_text);

        last_source_line = source_line;
    }

    fclose(map);
    fclose(out);
    return 0;
}

/*
 * ============================================================
 * Finish task
 * ============================================================
 */

static void finish_task(Task *task)
{
    if (task == NULL)
        return;

    if (task->state == TASK_FINISHED)
        return;

    int processor = task->proc_id;

    task->state = TASK_FINISHED;

    if (processor >= 0 &&
        processor < NP) {

        if (write_frequency_report(task) != 0) {
            fprintf(stderr,
                    "[OS] Could not write frequency report for PID %d\n",
                    task->pid);
        }

        unload_process_memory(processor);

        proc_pid[processor] = 0;

        /*
         * Make sure the processor itself cannot be accidentally
         * reused with stale execution state.
         */
        reset(processor);

        task->proc_id = -1;

        printf("[OS] PID %d finished on processor %d\n",
               task->pid,
               processor);
    }

    /*
     * Task files are no longer needed.
     */
    if (task->program_file[0] != '\0')
        remove(task->program_file);

    if (task->data_file[0] != '\0')
        remove(task->data_file);

    if (task->map_file[0] != '\0')
        remove(task->map_file);
}


/*
 * ============================================================
 * Dispatch waiting task
 * ============================================================
 *
 * Called whenever a processor becomes free.
 * ============================================================
 */

static void dispatch_waiting(int processor)
{
    if (processor < 0 || processor >= NP)
        return;

    if (proc_pid[processor] != 0)
        return;

    int pid =
        queue_pop(waiting_queue,
                  &waiting_count);

    if (pid < 0)
        return;

    Task *task = find_task(pid);

    if (task == NULL)
        return;

    if (task->state != TASK_WAITING) {

        /*
         * A stale queue entry should never be allowed to cause
         * an endless retry.
         */
        return;
    }

    if (load_process_memory(processor,
                            task->program_file,
                            task->data_file) != 0) {

        fprintf(stderr,
                "[OS] Could not dispatch waiting PID %d\n",
                pid);

        task->state = TASK_FINISHED;
        task->proc_id = -1;

        remove(task->program_file);
        remove(task->data_file);
        remove(task->map_file);

        return;
    }

    reset(processor);

    task->proc_id = processor;
    task->state = TASK_READY;

    proc_pid[processor] = pid;

    if (!queue_push(ready_queue,
                    &ready_count,
                    pid)) {

        /*
         * Do not leave a processor occupied if the ready queue
         * cannot accept the task.
         */
        unload_process_memory(processor);
        proc_pid[processor] = 0;

        task->proc_id = -1;
        task->state = TASK_FINISHED;

        remove(task->program_file);
        remove(task->data_file);
        remove(task->map_file);

        return;
    }

    printf("[OS] PID %d dispatched to processor %d\n",
           pid,
           processor);
}


/*
 * ============================================================
 * Scheduler
 * ============================================================
 *
 * Every READY task receives at most TIME_SLICE instructions.
 *
 * An important property:
 *
 *     rounds = ready_count
 *
 * is captured before the scheduler starts.
 *
 * Therefore tasks reinserted at the end of the ready queue
 * during this round are NOT processed again during the same
 * round.
 *
 * This guarantees finite work per scheduler round.
 * ============================================================
 */

static void scheduler_round(void)
{
    int rounds = ready_count;

    /*
     * Explicitly bound this scheduler invocation.
     */
    if (rounds > QUEUE_CAPACITY)
        rounds = QUEUE_CAPACITY;

    for (int i = 0;
         i < rounds;
         i++) {

        int pid =
            queue_pop(ready_queue,
                      &ready_count);

        if (pid < 0)
            break;

        Task *task = find_task(pid);

        if (task == NULL)
            continue;

        if (task->state == TASK_FINISHED)
            continue;

        if (task->proc_id < 0 ||
            task->proc_id >= NP)
            continue;

        int processor =
            task->proc_id;

        task->state = TASK_RUNNING;

        /*
         * This is the critical infinite-loop protection.
         *
         * Even if the program contains:
         *
         *     BAL .loop
         *     .loop
         *
         * the processor receives only TIME_SLICE instructions.
         */
        process_instructions(processor,
                              TIME_SLICE);

        /*
         * The processor stopped itself.
         */
        if (end_of_simulation[processor]) {

            finish_task(task);

            /*
             * Reuse the processor immediately if a waiting
             * task exists.
             */
            dispatch_waiting(processor);

        } else {

            /*
             * Task consumed its time slice but is still alive.
             */
            task->state = TASK_READY;

            /*
             * Requeueing is bounded by QUEUE_CAPACITY.
             */
            queue_push(ready_queue,
                       &ready_count,
                       pid);
        }
    }
}


/*
 * ============================================================
 * Non-blocking stdin test
 * ============================================================
 */

static int input_ready(void)
{
    fd_set set;

    FD_ZERO(&set);

    FD_SET(STDIN_FILENO, &set);

    struct timeval timeout;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int result =
        select(STDIN_FILENO + 1,
               &set,
               NULL,
               NULL,
               &timeout);

    return result > 0;
}


/*
 * ============================================================
 * Shell command parsing
 * ============================================================
 *
 * Expected input:
 *
 *     $ program.txt data.byte
 *
 * or:
 *
 *     $ program.txt
 *
 * or:
 *
 *     $ exit
 *
 * The lab specifically requires non-blocking shell input.
 * ============================================================
 */

static void shell(void)
{
    if(shell_exit)
	return;

    if (!input_ready())
        return;

    char line[MAX_LINE];

    if (fgets(line,
              sizeof(line),
              stdin) == NULL) {

        /*
         * EOF is treated as an exit command.
         */
        shell_exit = 1;

        printf("\n[Shell] EOF received; "
               "remaining tasks will continue\n");

        return;
    }

    /*
     * Remove newline.
     */
    line[strcspn(line, "\r\n")] = '\0';

    /*
     * Skip leading spaces/tabs.
     */
    char *s = line;

    while (*s == ' ' || *s == '\t')
        s++;

    if (*s == '\0')
        return;


    /*
     * --------------------------------------------------------
     * exit
     * --------------------------------------------------------
     */

    if (strcmp(s, "exit") == 0) {

        shell_exit = 1;

        printf("[Shell] exiting; "
               "existing tasks will continue\n");

        return;
    }


    /*
     * --------------------------------------------------------
     * Parse:
     *
     *     source [data]
     * --------------------------------------------------------
     */

    char source[MAX_LINE];
    char data[MAX_LINE];

    source[0] = '\0';
    data[0] = '\0';

    int count =
        sscanf(s,
               "%255s %255s",
               source,
               data);

    if (count <= 0)
        return;

    if (count == 1) {

        compile_and_load(source,
                         NULL);

    } else {

        compile_and_load(source,
                         data);
    }
}


/*
 * ============================================================
 * Check whether active tasks remain
 * ============================================================
 */

static int active_tasks_exist(void)
{
    for (int i = 0;
         i < task_count;
         i++) {

        if (tasks[i].state != TASK_FINISHED)
            return 1;
    }

    return 0;
}


/*
 * ============================================================
 * OS initialization
 * ============================================================
 */

static void initialize_os_state(void)
{
    memset(tasks,
           0,
           sizeof(tasks));

    memset(proc_pid,
           0,
           sizeof(proc_pid));

    memset(ready_queue,
           0,
           sizeof(ready_queue));

    memset(waiting_queue,
           0,
           sizeof(waiting_queue));

    task_count = 0;
    next_pid = 1;

    ready_count = 0;
    waiting_count = 0;

    shell_exit = 0;

    /*
     * Initialize the backing disk, swap subsystem, TLBs, cache
     * hierarchy, and finally the Lab 6 memory/page-table layer.
     */
    if (disk_initialize("swap.disk") != DISK_OK) {
        fprintf(stderr, "[OS] Failed to initialize swap disk\n");
        return;
    }

    swap_initialize();
    tlb_initialize();
    cache_initialize();
    initialize_memory();

    /*
     * Reset all processor instances.
     */
    for (int p = 0; p < NP; p++)
        reset(p);
}


/*
 * ============================================================
 * Final OS cleanup
 * ============================================================
 */

static void shutdown_os(void)
{
    /*
     * Finish every task that is still associated with a
     * processor.
     */
    for (int p = 0; p < NP; p++) {

        int pid = proc_pid[p];

        if (pid == 0)
            continue;

        Task *task = find_task(pid);

        if (task != NULL)
            finish_task(task);
    }

    /*
     * Waiting tasks have no processor, but their allocated
     * task files should still be removed.
     */
    for (int i = 0;
         i < task_count;
         i++) {

        Task *task = &tasks[i];

        if (task->state != TASK_FINISHED) {

            task->state = TASK_FINISHED;
            task->proc_id = -1;

            if (task->program_file[0] != '\0')
                remove(task->program_file);

            if (task->data_file[0] != '\0')
                remove(task->data_file);

            if (task->map_file[0] != '\0')
                remove(task->map_file);
        }
    }

    /* Report hardware activity before the hierarchy is reset. */
    cache_print_stats();
    tlb_print_stats();
    swap_print_status();
    disk_print_status();

    /* Flush cache contents before memory is torn down. */
    cache_finalize();
    finalize_memory();
    swap_finalize();
    disk_finalize();
    tlb_finalize();
}


/*
 * ============================================================
 * os_run()
 * ============================================================
 */

void os_run(void)
{
    initialize_os_state();

    printf("Mini OS started with %d processors.\n",
           NP);

    printf("$ ");
    fflush(stdout);


    /*
     * The scheduler loop is explicitly bounded.
     *
     * Under normal operation it terminates when:
     *
     *     shell_exit == 1
     *
     * and there are no active tasks.
     *
     * MAX_SCHEDULER_ROUNDS is an additional hard safety limit.
     */
    unsigned long scheduler_rounds = 0;

    while (scheduler_rounds <
           MAX_SCHEDULER_ROUNDS) {

        scheduler_rounds++;

        /*
         * Run all currently ready tasks for one finite
         * time slice.
         */
        scheduler_round();

        /*
         * Give the shell its turn.
         */
        shell();

        /*
         * Once exit has been entered, no new programs are
         * accepted. Existing tasks continue.
         */
        if (shell_exit && !active_tasks_exist()) {
            	// if (!active_tasks_exist())
        	break;

        }
	/*else {

            printf("$ ");
            fflush(stdout);
        }*/

        /*
         * Small delay required by the lab's real-time
         * multi-processing demonstration.
         */
        usleep(10);
    }


    /*
     * The hard scheduler limit was reached.
     *
     * Do not continue indefinitely.
     */
    if (scheduler_rounds >=
        MAX_SCHEDULER_ROUNDS) {

        fprintf(stderr,
                "[OS] Scheduler safety limit reached; "
                "stopping simulation\n");
    }


    shutdown_os();
}


/*
 * ============================================================
 * os_run_with_program()
 * ============================================================
 *
 * Allows main(argc, argv) to launch an initial program.
 *
 * After loading the initial program, the normal OS shell remains
 * available for additional tasks.
 * ============================================================
 */

void os_run_with_program(const char *program)
{
    initialize_os_state();

    printf("Mini OS started with %d processors.\n",
           NP);

    if (program != NULL &&
        program[0] != '\0') {

        /*
         * Command-line program uses data.byte if it exists.
         */
        FILE *data =
            fopen("data.byte", "rb");

        if (data != NULL) {

            fclose(data);

            compile_and_load(program,
                             "data.byte");

        } else {

            compile_and_load(program,
                             NULL);
        }
    }

    printf("$ ");
    fflush(stdout);


    unsigned long scheduler_rounds = 0;

    while (scheduler_rounds <
           MAX_SCHEDULER_ROUNDS) {

        scheduler_rounds++;

        scheduler_round();

        shell();

        if (shell_exit && !active_tasks_exist()) {
            // if (!active_tasks_exist())
        	break;

        }
	/*else {

            printf("$ ");
            fflush(stdout);
        }*/

        usleep(10);
    }


    if (scheduler_rounds >=
        MAX_SCHEDULER_ROUNDS) {

        fprintf(stderr,
                "[OS] Scheduler safety limit reached; "
                "stopping simulation\n");
    }

    shutdown_os();
}
