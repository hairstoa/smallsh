smallsh: main.c
	gcc -o smallsh  -Wall -Werror -O0 -g3 -std=c11 main.c

clean :
	-rm *.o
	-rm smallsh