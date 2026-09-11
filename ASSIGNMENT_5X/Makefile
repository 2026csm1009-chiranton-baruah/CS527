CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
TARGET = simulator
OBJS = main.o compiler.o processor.o memory.o disk.o os.o tlb.o swap.o cache.o
.PHONY: all clean test
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
main.o: main.c os.h
	$(CC) $(CFLAGS) -c main.c
compiler.o: compiler.c compiler.h
	$(CC) $(CFLAGS) -c compiler.c
processor.o: processor.c processor.h memory.h
	$(CC) $(CFLAGS) -c processor.c
memory.o: memory.c memory.h processor.h disk.h tlb.h swap.h cache.h
	$(CC) $(CFLAGS) -c memory.c
disk.o: disk.c disk.h
	$(CC) $(CFLAGS) -c disk.c
tlb.o: tlb.c tlb.h processor.h
	$(CC) $(CFLAGS) -c tlb.c
swap.o: swap.c swap.h disk.h
	$(CC) $(CFLAGS) -c swap.c
cache.o: cache.c cache.h memory.h processor.h
	$(CC) $(CFLAGS) -c cache.c
os.o: os.c os.h processor.h memory.h compiler.h disk.h
	$(CC) $(CFLAGS) -c os.c
test: $(TARGET)
	@echo "========================================"
	@echo " Running 6 programs simultaneously"
	@echo "========================================"
	@printf 'a3.txt data.byte\nf2.txt data.byte\ncross.txt data.byte\nmatmul.txt data.byte\nfib.txt data.byte\neitch.txt data.byte\nexit\n' | ./$(TARGET)
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f program.byte program_*.byte data_*.byte processor.log disk.img
