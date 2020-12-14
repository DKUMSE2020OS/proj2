proj2: proj2.o
	gcc -o proj2 proj2.o

proj2.o: proj2.c
	gcc -c -o proj2.o proj2.c

clean:
	rm *.o proj2
