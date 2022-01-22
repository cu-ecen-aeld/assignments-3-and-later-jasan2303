.DEFAULT_GOAL := writer

writer: writer.o
	gcc writer.o -o writer

writer.o:
	gcc -c writer.c

clean:
	rm -f *.o writer  # -f ignores the non existense files

CROSS_COMPILE:
	aarch64-none-linux-gnu-gcc -g -Wall -c -o writer.o writer.c && aarch64-none-linux-gnu-gcc writer.o -o writer
