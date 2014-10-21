dmexec: main.c
	gcc -g -Wall -D_GNU_SOURCE -std=c99 -O8 -o $@ $+

