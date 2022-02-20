all: aesdsocket.o

aesdsocket.o:
	$(CROSS_COMPILE)-gcc -Wall -Werror aesdsocket.c -o aesdsocket
	
clean:
	rm -f *.o aesdsocket  # -f ignores the non existense files
