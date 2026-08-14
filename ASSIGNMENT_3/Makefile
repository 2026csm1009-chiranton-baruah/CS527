CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

TARGET = simulator

SRCS = main.c compiler.c processor.c memory.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c compiler.h memory.h processor.h
compiler.o: compiler.c compiler.h
processor.o: processor.c processor.h memory.h
memory.o: memory.c memory.h

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) program.txt

.PHONY: all clean run
