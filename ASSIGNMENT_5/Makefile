CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

TARGET = simulator
SRCS = main.c compiler.c processor.c memory.c os.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c os.h
compiler.o: compiler.c compiler.h
processor.o: processor.c processor.h memory.h
memory.o: memory.c memory.h processor.h
os.o: os.c os.h compiler.h memory.h processor.h

clean:
	rm -f $(OBJS) $(TARGET) program.byte program_*.byte processor.log data_*.byte
debug:
	$(CC) -Wall -Wextra -std=c11 -g -O0 -o $(TARGET) $(SRCS)

.PHONY: all clean
