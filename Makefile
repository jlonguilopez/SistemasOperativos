CC = gcc					
CFLAGS = -Wall -Wextra -g

p2: p2.o memoria.o listaMemoria.o
	$(CC) $(CFLAGS) -o p2 p2.o memoria.o listaMemoria.o

p2.o: p2.c memoria.h
	$(CC) $(CFLAGS) -c p2.c

memoria.o: memoria.c memoria.h listaMemoria.h
	$(CC) $(CFLAGS) -c memoria.c

listaMemoria.o: listaMemoria.c listaMemoria.h
	$(CC) $(CFLAGS) -c listaMemoria.c

clean:
	rm -f p2 *.o