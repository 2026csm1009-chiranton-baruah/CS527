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

#define MAX_TASKS 128
#define TIME_SLICE 10
#define QUEUE_CAPACITY MAX_TASKS

typedef enum { TASK_READY, TASK_RUNNING, TASK_WAITING, TASK_FINISHED } TaskState;

typedef struct {
    int pid;
    int proc_id;
    char source[256];
    char program_file[256];
    char data_file[256];
    TaskState state;
} Task;

static Task tasks[MAX_TASKS];
static int task_count = 0;
static int next_pid = 1;
static int proc_pid[NP];
static int ready_queue[QUEUE_CAPACITY];
static int waiting_queue[QUEUE_CAPACITY];
static int ready_count = 0;
static int waiting_count = 0;
static int shell_exit = 0;

static void queue_push(int *queue, int *count, int value) {
    if (*count < QUEUE_CAPACITY)
        queue[(*count)++] = value;
}

static int queue_pop(int *queue, int *count) {
    if (*count == 0) return -1;
    int value = queue[0];
    memmove(queue, queue + 1, (size_t)(*count - 1) * sizeof(queue[0]));
    (*count)--;
    return value;
}

static Task *find_task(int pid) {
    for (int i = 0; i < task_count; i++)
        if (tasks[i].pid == pid) return &tasks[i];
    return NULL;
}

static int find_free_processor(void) {
    for (int p = 0; p < NP; p++)
        if (proc_pid[p] == 0) return p;
    return -1;
}

static int create_task(const char *program) {
    if (task_count >= MAX_TASKS) {
        fprintf(stderr, "OS: task table full\n");
        return -1;
    }

    char data_file[256];
    char program_file[256];
    int pid = next_pid++;
    snprintf(program_file, sizeof(program_file), "program_%d.byte", pid);
    snprintf(data_file, sizeof(data_file), "data_%d.byte", pid);

    /* Each process gets its own copy of the current data.byte. */
    FILE *src = fopen("data.byte", "r");
    FILE *dst = fopen(data_file, "w");
    if (src && dst) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
            fwrite(buf, 1, n, dst);
    }
    if (src) fclose(src);
    if (dst) fclose(dst);

    /* program.byte was just produced by the compiler; give this task its own copy. */
    FILE *psrc = fopen("program.byte", "rb");
    FILE *pdst = fopen(program_file, "wb");
    if (psrc && pdst) {
        char pbuf[4096];
        size_t pn;
        while ((pn = fread(pbuf, 1, sizeof(pbuf), psrc)) > 0)
            fwrite(pbuf, 1, pn, pdst);
    }
    if (psrc) fclose(psrc);
    if (pdst) fclose(pdst);

    int p = find_free_processor();

    Task *t = &tasks[task_count++];
    t->pid = pid;
    t->proc_id = -1;
    t->state = (p >= 0) ? TASK_READY : TASK_WAITING;
    strncpy(t->source, program, sizeof(t->source) - 1);
    t->source[sizeof(t->source) - 1] = '\0';
    strncpy(t->program_file, program_file, sizeof(t->program_file) - 1);
    t->program_file[sizeof(t->program_file) - 1] = '\0';
    strncpy(t->data_file, data_file, sizeof(t->data_file) - 1);
    t->data_file[sizeof(t->data_file) - 1] = '\0';

    if (p >= 0) {
        t->proc_id = p;
        proc_pid[p] = pid;
        initialize_memory(p, program_file, data_file);
        reset(p);
        queue_push(ready_queue, &ready_count, pid);
        printf("[OS] PID %d loaded on processor %d\n", pid, p);
    } else {
        queue_push(waiting_queue, &waiting_count, pid);
        printf("[OS] PID %d placed in waiting queue\n", pid);
    }

    return pid;
}

static void compile_and_load(const char *source) {
    printf("[Compiler] compiling %s\n", source);
    compile(source);
    create_task(source);
}

static void finish_task(Task *t) {
    if (!t || t->proc_id < 0) return;

    int p = t->proc_id;
    t->state = TASK_FINISHED;
    finalize_memory(p, t->data_file);
    proc_pid[p] = 0;
    t->proc_id = -1;

    printf("[OS] PID %d finished on processor %d\n", t->pid, p);
}

static void dispatch_waiting(int p) {
    if (proc_pid[p] != 0) return;

    int pid = queue_pop(waiting_queue, &waiting_count);
    if (pid < 0) return;

    Task *t = find_task(pid);
    if (!t) return;

    t->proc_id = p;
    t->state = TASK_READY;
    proc_pid[p] = pid;
    initialize_memory(p, t->program_file, t->data_file);
    reset(p);
    queue_push(ready_queue, &ready_count, pid);

    printf("[OS] PID %d dispatched to processor %d\n", pid, p);
}

static void scheduler_round(void) {
    int rounds = ready_count;

    for (int i = 0; i < rounds; i++) {
        int pid = queue_pop(ready_queue, &ready_count);
        if (pid < 0) break;

        Task *t = find_task(pid);
        if (!t || t->state == TASK_FINISHED || t->proc_id < 0)
            continue;

        int p = t->proc_id;
        t->state = TASK_RUNNING;
        process_instructions(p, TIME_SLICE);

        if (end_of_simulation[p]) {
            finish_task(t);
            dispatch_waiting(p);
        } else {
            t->state = TASK_READY;
            queue_push(ready_queue, &ready_count, pid);
        }
    }
}

static int input_ready(void) {
    fd_set set;
    struct timeval timeout = {0, 0};
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    return select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout) > 0;
}

static void shell(void) {
    if (!input_ready()) return;

    char line[256];
    if (!fgets(line, sizeof(line), stdin)) {
        shell_exit = 1;
        return;
    }

    char *s = line;
    while (*s == ' ' || *s == '\t') s++;
    s[strcspn(s, "\r\n")] = '\0';

    if (*s == '\0') return;

    if (strcmp(s, "exit") == 0) {
        shell_exit = 1;
        printf("[Shell] exiting; existing tasks will continue\n");
        return;
    }

    compile_and_load(s);
}

void os_run(void) {
    memset(proc_pid, 0, sizeof(proc_pid));
    memset(tasks, 0, sizeof(tasks));

    printf("Mini OS started with %d processors.\n", NP);
    printf("$ ");
    fflush(stdout);

    while (!shell_exit || ready_count > 0) {
        scheduler_round();
        shell();

        if (!shell_exit) {
            printf("$ ");
            fflush(stdout);
        }

        if (ready_count == 0 && shell_exit) {
            int any_running = 0;
            for (int p = 0; p < NP; p++)
                if (proc_pid[p] != 0) any_running = 1;
            if (!any_running) break;
        }

        usleep(10);
    }

    for (int p = 0; p < NP; p++) {
        if (proc_pid[p] != 0) {
            Task *t = find_task(proc_pid[p]);
            if (t) finish_task(t);
        }
    }
}
