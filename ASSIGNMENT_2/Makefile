# ---------- Compiler ----------
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

# ---------- Target ----------
TARGET = sim

# ---------- Source files ----------
SRCS = main.c compiler.c processor.c memory.c
OBJS = $(SRCS:.c=.o)

# ---------- Build ----------
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile each .c into .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---------- Run ----------
# Usage: make run PROGRAM=sum.txt
run: $(TARGET)
	./$(TARGET) $(PROGRAM)

# ---------- Clean ----------
clean:
	rm -f $(OBJS) $(TARGET) program.byte

# ---------- Rebuild ----------
rebuild: clean all

.PHONY: all run clean rebuild
