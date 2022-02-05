all: writer.o

writer.o:
	$(CROSS_COMPILE)gcc writer.c -o writer
	#$(CROSS_COMPILE)gcc -c writer.c

clean:
	rm -f *.o writer  # -f ignores the non existense files

