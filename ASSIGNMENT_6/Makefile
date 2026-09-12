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

test: $(TARGET)
	@set -e; \
	rm -f lab6_sum_*.freq program.byte program_*.byte program.map program_*.map processor.log data_*.byte; \
	printf 'tests/lab6_sum.txt tests/lab6_data.byte\nexit\n' | ./$(TARGET) >/tmp/cs527_lab6_test.log 2>&1; \
	freq=$$(ls lab6_sum_*.freq 2>/dev/null | head -n 1); \
	test -n "$$freq"; \
	grep -q '^1 x1 = 0' "$$freq"; \
	grep -q '^10 x15 = x1 - x4' "$$freq"; \
	grep -q '^10 BEQ .exit' "$$freq"; \
	grep -q '^9 print x1' "$$freq"; \
	grep -q '^1 \[x3\] = x2' "$$freq"; \
	echo "Lab 6 tests passed ($$freq)"

clean:
	rm -f $(OBJS) $(TARGET) program.byte program_*.byte program.map program_*.map processor.log data_*.byte *.freq
debug:

	$(CC) -Wall -Wextra -std=c11 -g -O0 -o $(TARGET) $(SRCS)

.PHONY: all clean debug test
