1. To compile use make
$ make
gcc -Wall -Werror -g -o master master.c -lpthread
gcc -Wall -Werror -g -o palin palin.c -lpthread

2. To execute:
$ ./master -n 20 strings.txt
