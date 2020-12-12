TLP_MLFQ : TLP_MLFQ.o
	gcc -o TLP_MLFQ TLP_MLFQ.o

TLP_MLFQ.o : TLP_MLFQ.c
	gcc -c -o TLP_MLFQ.o TLP_MLFQ.c

clean :
	rm *.o TLP_MLFQ

