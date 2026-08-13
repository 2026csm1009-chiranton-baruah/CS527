CC=gcc

CFLAGS=-Wall

OBJ=main.o compiler.o processor.o memory.o

simulator: $(OBJ)
	$(CC) -o simulator $(OBJ)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

compiler.o: compiler.c
	$(CC) $(CFLAGS) -c compiler.c

processor.o: processor.c
	$(CC) $(CFLAGS) -c processor.c

memory.o: memory.c
	$(CC) $(CFLAGS) -c memory.c

clean:
	rm -f *.o simulator
