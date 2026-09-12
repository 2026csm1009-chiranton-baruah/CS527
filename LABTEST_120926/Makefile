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
	@echo " Running a few programs simultaneously"
	@echo "========================================"
	@printf 'crc_test_fixed.txt data.byte\ncrc16_test.txt data.byte\nbooth2.txt data.byte\nrecurring_division.txt\nhamming_code_2.txt data.byte\na3.txt data.byte\nsod.txt data.byte\nf2.txt data.byte\na4.txt data.byte\ncross.txt data.byte\nmatmul.txt data.byte\neitch.txt data.byte\nexit\n' | ./$(TARGET)

TEST_PROGRAMS = \
	matmul.txt \
	a3.txt \
	fib.txt \
	sod.txt\
	booth2.txt\
	a4.txt \
	crc_test_fixed.txt\
	eitch.txt \
	hamming_code_2.txt\
	mulmul.txt\
	crc16_test.txt\
	recurring_division.txt\
	cross.txt \
	sod.txt\
	f2.txt \
	eitch.txt \
	crc16_test.txt\
	cross.txt \
	hamming_code_2.txt\
	matmul.txt \
	f2.txt \
	crc_test_fixed.txt\
	a3.txt \
	booth2.txt\
	mulmul.txt\
	a4.txt \
	fib.txt\
	recurring_division.txt

stress-test: $(TARGET)
	@echo "========================================"
	@echo " Running $(words $(TEST_PROGRAMS)) programs"
	@echo "========================================"
	@for p in $(TEST_PROGRAMS); do \
		printf '%s data.byte\n' "$$p"; \
	done | { cat; printf 'exit\n'; } | ./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f program.byte program_*.byte data_*.byte processor.log disk.img
