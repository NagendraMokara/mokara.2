CFLAGS=-Wall -Werror -g

all: master palin

master: master.c palindromes.h
	gcc $(CFLAGS) -o master master.c -lpthread

palin: palin.c palindromes.h
	gcc $(CFLAGS) -o palin palin.c -lpthread

clean:
	rm -f master
	rm -f palin palin.out nopalin.out
