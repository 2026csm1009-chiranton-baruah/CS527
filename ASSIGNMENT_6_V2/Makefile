CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

TARGET = simulator
SRCS = main.c compiler.c processor.c memory.c os.c cache.c tlb.c swap.c disk.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c os.h
compiler.o: compiler.c compiler.h
processor.o: processor.c processor.h memory.h cache.h
memory.o: memory.c memory.h processor.h cache.h tlb.h swap.h
os.o: os.c os.h compiler.h memory.h processor.h cache.h tlb.h swap.h disk.h
cache.o: cache.c cache.h memory.h processor.h
tlb.o: tlb.c tlb.h processor.h
swap.o: swap.c swap.h disk.h processor.h
disk.o: disk.c disk.h

clean:
	rm -f $(OBJS) $(TARGET) program.byte program_*.byte program.map program_*.map processor.log data_*.byte *.freq swap.disk
debug:
	$(CC) -Wall -Wextra -std=c11 -g -O0 -o $(TARGET) $(SRCS)

test: $(TARGET)
	@set -e; \
	rm -f lab6_sum_*.freq program.byte program_*.byte program.map program_*.map processor.log data_*.byte swap.disk; \
	printf 'tests/lab6_sum.txt tests/lab6_data.byte\nexit\n' | ./$(TARGET) >/tmp/cs527_lab6_test.log 2>&1; \
	freq=$$(ls lab6_sum_*.freq 2>/dev/null | head -n 1); \
	test -n "$$freq"; \
	grep -q '^1 x1 = 0' "$$freq"; \
	grep -q '^10 x15 = x1 - x4' "$$freq"; \
	grep -q '^10 BEQ .exit' "$$freq"; \
	grep -q '^9 print x1' "$$freq"; \
	grep -q '^1 \[x3\] = x2' "$$freq"; \
	grep -q 'Cache Statistics' /tmp/cs527_lab6_test.log; \
	grep -q 'TLB STATISTICS' /tmp/cs527_lab6_test.log; \
	test -f swap.disk; \
	printf 'tests/newisa.txt tests/newisa_data.byte\nexit\n' | ./$(TARGET) >/tmp/cs527_newisa_test.log 2>&1; \
	grep -q '^2$$' /tmp/cs527_newisa_test.log; \
	grep -q '^1280$$' /tmp/cs527_newisa_test.log; \
	grep -q '^20$$' /tmp/cs527_newisa_test.log; \
	grep -q 'newisa' /tmp/cs527_newisa_test.log; \
	echo "Lab 6 + hardware + new ISA tests passed ($$freq)"

.PHONY: all clean debug test hardware-stress

hardware-stress: $(TARGET)
	@set -e; \
	rm -f lab6_hw_stress_*.freq program.byte program_*.byte program.map program_*.map processor.log data_*.byte swap.disk; \
	printf 'tests/lab6_hw_stress.txt tests/lab6_hw_data.byte\ntests/lab6_hw_stress.txt tests/lab6_hw_data.byte\ntests/lab6_hw_stress.txt tests/lab6_hw_data.byte\ntests/lab6_hw_stress.txt tests/lab6_hw_data.byte\nexit\n' | ./$(TARGET) >/tmp/cs527_lab6_hw_stress.log 2>&1; \
	test $$(ls lab6_hw_stress_*.freq 2>/dev/null | wc -l) -eq 4; \
	grep -q '\[MEM\] page-out' /tmp/cs527_lab6_hw_stress.log; \
	grep -q '\[MEM\] page-in' /tmp/cs527_lab6_hw_stress.log; \
	grep -q 'Cache Statistics' /tmp/cs527_lab6_hw_stress.log; \
	grep -q 'TLB STATISTICS' /tmp/cs527_lab6_hw_stress.log; \
	echo "Hardware paging stress passed"

