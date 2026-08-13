#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "compiler.h"

#define MAX_LINES 512
#define MAX_LABELS 128

typedef struct {
    char name[64];
    int index;
} Label;

static Label labels[MAX_LABELS];
static int label_count = 0;

static char *trim(char *s) {
    while (isspace(*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e >= s && isspace(*e)) *e-- = 0;
    return s;
}

static int regnum(const char *s) {
    if (s[0] != 'x') return -1;
    return atoi(s + 1);
}

static int is_number(const char *s) {
    if (*s == 0) return 0;
    while (*s) {
        if (!isdigit(*s)) return 0;
        s++;
    }
    return 1;
}

static void add_label(const char *name, int idx) {
    strcpy(labels[label_count].name, name);
    labels[label_count].index = idx;
    label_count++;
}

static int find_label(const char *name) {
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].index;
    }
    return -1;
}

static void emit(FILE *out, int op, int d, int s1, int s2) {
    fprintf(out, "%02X %02X %02X %02X\n", op & 0xFF, d & 0xFF, s1 & 0xFF, s2 & 0xFF);
}

void compile(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Cannot open %s\n", filename);
        exit(1);
    }

    char lines[MAX_LINES][256];
    int count = 0;

    while (fgets(lines[count], sizeof(lines[count]), fp)) {
        count++;
    }
    fclose(fp);

    int ins = 0;
    for (int i = 0; i < count; i++) {
        char *p = strchr(lines[i], '%');
        if (p) *p = 0;

        char *s = trim(lines[i]);
        if (*s == 0) continue;

        if (s[0] == '.') {
            add_label(s, ins);
        } else {
            ins++;
        }
    }

    FILE *out = fopen("program.byte", "w");

    int pc = 0;

    for (int i = 0; i < count; i++) {
        char buf[256];
        strcpy(buf, lines[i]);

        char *p = strchr(buf, '%');
        if (p) *p = 0;

        char *s = trim(buf);
        if (*s == 0) continue;
        if (s[0] == '.') continue;

        if (strncmp(s, "BEQ", 3) == 0 || strncmp(s, "BNE", 3) == 0 ||
            strncmp(s, "BGE", 3) == 0 || strncmp(s, "BLT", 3) == 0 ||
            strncmp(s, "BGT", 3) == 0 || strncmp(s, "BLE", 3) == 0 ||
            strncmp(s, "BAL", 3) == 0) {

            char op[8], lab[64];
            sscanf(s, "%s %s", op, lab);

            int target = find_label(lab);
            int off = target - pc;

            int opc = 0x1E;
            if (!strcmp(op, "BEQ")) opc = 0x10;
            else if (!strcmp(op, "BNE")) opc = 0x11;
            else if (!strcmp(op, "BGE")) opc = 0x12;
            else if (!strcmp(op, "BLT")) opc = 0x13;
            else if (!strcmp(op, "BGT")) opc = 0x14;
            else if (!strcmp(op, "BLE")) opc = 0x15;

            emit(out, opc, 0, 0, off);
            pc++;
            continue;
        }

        char lhs[64], rhs[128];

        if (sscanf(s, "%[^=]=%[^\n]", lhs, rhs) == 2) {
            char *L = trim(lhs);
            char *R = trim(rhs);

            if (L[0] == '[') {
                char addr[64];
                sscanf(L, "[%[^]]]", addr);

                int rd = regnum(addr);
                int rs = regnum(R);

                emit(out, 0x06, rd, rs, 0);
                pc++;
                continue;
            }

            int d = regnum(L);

            if (R[0] == '[') {
                char addr[64];
                sscanf(R, "[%[^]]]", addr);

                if (is_number(addr)) {
                    emit(out, 0x0F, 255, 0, atoi(addr));
                    emit(out, 0x05, d, 255, 0);
                    pc += 2;
                } else {
                    emit(out, 0x05, d, regnum(addr), 0);
                    pc++;
                }
                continue;
            }

            if (is_number(R)) {
                emit(out, 0x0F, d, 0, atoi(R));
                pc++;
                continue;
            }

            char a[64], op, b[64];
            if (sscanf(R, "%s %c %s", a, &op, b) == 3) {
                int ra = regnum(a);

                if (is_number(b)) {
                    int imm = atoi(b);
                    int opc = 0x09;

                    if (op == '-') opc = 0x0A;
                    else if (op == '*') opc = 0x0B;
                    else if (op == '/') opc = 0x0C;

                    emit(out, opc, d, ra, imm);
                } else {
                    int rb = regnum(b);
                    int opc = 0x01;

                    if (op == '-') opc = 0x02;
                    else if (op == '*') opc = 0x03;
                    else if (op == '/') opc = 0x04;

                    emit(out, opc, d, ra, rb);
                }
                pc++;
            }
        }
    }

    emit(out, 0x00, 0, 0, 0);
    fclose(out);
}
