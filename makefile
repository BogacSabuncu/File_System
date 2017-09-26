CC=gcc -Wall -g -m32

main: disk.o fs.o main.o
	$(CC) disk.o fs.o main.o -o main

main.o: main.c
	$(CC) -c -o main.o main.c

disk.o: disk.c disk.h
	$(CC) -c -o disk.o disk.c

fs.o: fs.c
	$(CC) -c -o fs.o fs.c

clean:
	rm disk.o fs.o main.o main