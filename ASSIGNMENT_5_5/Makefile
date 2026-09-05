CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2

TARGET = simulator
DISK_TEST = disk_test

OBJS = main.o compiler.o processor.o memory.o disk.o os.o

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

memory.o: memory.c memory.h processor.h disk.h
	$(CC) $(CFLAGS) -c memory.c

disk.o: disk.c disk.h
	$(CC) $(CFLAGS) -c disk.c

os.o: os.c os.h processor.h memory.h compiler.h disk.h
	$(CC) $(CFLAGS) -c os.c

# ------------------------------------------------------------
# Disk subsystem test executable
# ------------------------------------------------------------

$(DISK_TEST): tests/disk_tests.c tests/disk_tests.h tests/disk_test_main.c disk.c disk.h
	$(CC) $(CFLAGS) -I. -o $@ \
		tests/disk_tests.c \
		tests/disk_test_main.c \
		disk.c

# ------------------------------------------------------------
# Cumulative test suite
# ------------------------------------------------------------

test: clean $(TARGET) $(DISK_TEST)
	@echo ""
	@echo "========================================"
	@echo " Mini OS Cumulative Test Suite"
	@echo "========================================"
	@echo ""

	@echo "[1/2] Disk subsystem tests"
	@./$(DISK_TEST)

	@echo ""
	@echo "[2/2] Simulator integration test"
	@printf 'disk\nps\na3.txt data.byte\nf2.txt data.byte\ncross.txt cross_data.byte\nexit\n' | ./$(TARGET)

	@echo ""
	@echo "========================================"
	@echo " ALL TESTS PASSED"
	@echo "========================================"

# ------------------------------------------------------------
# Cleanup
# ------------------------------------------------------------

clean:
	rm -f $(OBJS) $(TARGET) $(DISK_TEST) \
	      program.byte program_*.byte \
	      processor.log data_*.byte \
	      disk_test.img disk.img
