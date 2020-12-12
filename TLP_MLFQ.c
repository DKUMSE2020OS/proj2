#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include "msg.h"

void signal_handler(int signo);
void signal_handler2(int signo);
int count = 0;
int total_execution_time = 0;
int pids[10];
int time_quantum[5];
int cpu_burst;
int target_pid = 0;
int level_choice = 0;
int already_exit[11];
int exit_num = 0;


typedef struct page{
        struct page* next;
	//1 or 2 level
        int level;
        int pfn;
	//0:empty, 1:using
        int valid;
	int page_num;
}page;

typedef struct page_queue{
        struct page* p;
        page* front;
        page* rear;
        int count;
	int pfn;
}page_queue;

void enpage(struct page_queue* queue, struct page* p) {
        if (queue->count == 0) {
                queue->front = p;
                p->next = NULL;
                queue->rear = p;
        }
        else {
                queue->rear->next = p;
                queue->rear = p;
        }

        queue->count++;
}

void depage(struct page_queue* queue) {
        if (queue->count != 0) {
                queue->count--;
                queue->front = queue->front->next;
        }
}

#define PAGE_DIR_NUM 0x20

struct page_queue* free_page_list = NULL;
struct page_queue* first_level[PAGE_DIR_NUM];

//Process Control Block
typedef struct proc_t {
	struct proc_t* next;
	int pid;
	int state;   //running/ready or wait : 0-run 1-wait
	int remaining_tq;
	int remaining_wait;
	int level;	//0~4 level
}proc_t;

typedef struct procq_t {
	struct proc_t* p;
	proc_t* front;
	proc_t* rear;
	int count;
}procq_t;

struct procq_t* runq[5];
struct procq_t* waitq = NULL;

void add_queue(struct procq_t* queue, struct proc_t* p) {
	if (queue->count == 0) {
		queue->front = p;
		p->next = NULL;
		queue->rear = p;
	}
	else {
		queue->rear->next = p;
		queue->rear = p;
	}

	queue->count++;
}

void dequeue(struct procq_t* queue) {
	if (queue->count != 0) {
		queue->count--;
		queue->front = queue->front->next;
	}
}

int mem_mapping(int va){
	
	//<read_or_write> : 0->read, 1->write

	int page_dir = va >> 11;
	int page_table = (va & 0x7c0) >> 6;
	int offset = va & 0x3f;
	int page_fault = -1;
	int PA;
	static FILE* fp3 = NULL;

	fp3 = fopen("./schedule_dump.txt", "a");
	
	struct page* finder = first_level[page_dir]->front;

	//check if there is 1-level same mapping
	for(int i=0; i<first_level[page_dir]->count; i++){

		if(finder->page_num == page_table){

			page_fault = 1;

			break;
		}
		else{
			finder = finder->next;
		}
	}

	printf("page table pfn = %d ", page_table);
	fprintf(fp3, "page table pfn = %d ", page_table);

	if(page_fault == 1){
		
		int page_dir_pfn = first_level[page_dir]->pfn;
		int page_table_pfn = finder->pfn;

		printf("(0X%x) Page exists!\n", va);
		fprintf(fp3, "(0X%x) Page exists!\n", va);
		
		PA = (page_dir_pfn << 11) | (page_table_pfn << 6) | offset;

		return PA;
	}
	else if(page_fault == -1){

		int page_table_pfn;

		printf("(0X%x) Page fault!\n", va);
		fprintf(fp3, "(0X%x) Page fault!\n", va);

		for(int i=0; i<32; i++){

			struct page* page_mover = free_page_list->front;

			page_mover->pfn = 31 - i;

			page_mover->page_num = i;

			if(page_mover->page_num == page_table){
				page_table_pfn = page_mover->pfn;
			}
			
			depage(free_page_list);

			enpage(first_level[page_dir], page_mover);
		}
			
		PA = (first_level[page_dir]->pfn << 11) | (page_table_pfn << 6) | offset;

		return PA;
	}

	fclose(fp3);
}

void clear_mapping(){
	for(int i=0; i<32; i++){

		int index = first_level[i]->count;

		for(int j=0; j<index; j++){

			struct page* page_cleaner = first_level[i]->front;

			page_cleaner->level = 0;
			page_cleaner->pfn = 0;
			page_cleaner->valid = 0;
			page_cleaner->page_num = 0;

			depage(first_level[i]);

			enpage(free_page_list, page_cleaner);			
		}
	}
}

int main() {

	//make free page list queue and save 0x800 number of pages in the list.
	free_page_list = malloc(sizeof(struct page_queue));

	for(int i=0; i<0x20; i++){
		first_level[i] = malloc(sizeof(struct page_queue));
        }
	

	for(int i=0; i<0x400; i++){

		struct page* p = malloc(sizeof(struct page));

		enpage(free_page_list, p);
		free_page_list->count++;
	}

	printf("free page number : %d\n\n\n", free_page_list->count);

	//fill first level table's pfn with reverse number of 0x20
	for(int i=0; i<0x20; i++){

		first_level[i]->pfn = 0x20 -1 - i;

	}


	static FILE* fp = NULL;

	//open file by 'a' which can continuously write file
	fp = fopen("./schedule_dump.txt", "a");
	fprintf(fp, "<Scheduling Simulation Started>\n\n->10 processes will be executed more than 1000 seconds each!\n\n");
	printf("<Scheduling Simulation Started>\n\n->10 processes will be executed more than 1000 seconds each!\n\n");

	//memory allocation for 5 level queues
	runq[0] = malloc(sizeof(struct procq_t));
	runq[1] = malloc(sizeof(struct procq_t));
	runq[2] = malloc(sizeof(struct procq_t));
	runq[3] = malloc(sizeof(struct procq_t));
	runq[4] = malloc(sizeof(struct procq_t));

	//memory allocation for waitq
	waitq = malloc(sizeof(struct procq_t));
	cpu_burst = 4;

	for (int i = 0; i < 10; i++) {
		pids[i] = 0;
		time_quantum[i] = 0;
	}
	// child fork
	for (int i = 0; i < 10; i++) {
		int ret = fork();

		//Each process has excution time of 1000~1020 sec
		total_execution_time = (rand() % 20) + 1000;

		if (ret < 0) {
			// fork fail
			perror("fork_failed");
		}
		else if (ret == 0) {
			// child
			cpu_burst = 4;
			// signal handler setup
			struct sigaction old_sa;
			struct sigaction new_sa;
			memset(&new_sa, 0, sizeof(new_sa));
			//Call signal handler2 for child process
			new_sa.sa_handler = &signal_handler2;
			sigaction(SIGALRM, &new_sa, &old_sa);

			while (1);   //wait until signal comes
			exit(0);
			// never reach here
		}
		else if (ret > 0) {
			// parent
			pids[i] = ret;

			fprintf(fp, "child %d created : Total execution time : %d\n", pids[i], total_execution_time);
			printf("child %d created : Total execution time : %d\n", pids[i], total_execution_time);

			// alloc new pcb
			struct proc_t* p = malloc(sizeof(struct proc_t));
			p->remaining_tq = total_execution_time;
			p->pid = ret;
			// put it in the runq
			add_queue(runq[0], p);
		}
	}

	// signal handler setup
	struct sigaction old_sa;
	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	//Call signal handler for operating system process(parent)
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	// fire the alrm timer
	struct itimerval new_itimer, old_itimer;
	//1sec interval
	new_itimer.it_interval.tv_sec = 1;
	new_itimer.it_interval.tv_usec = 0;
	//Execution starts after 1sec (waiting for child fork)
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	fclose(fp);

	while (1);
	return 0;
}

void signal_handler2(int signo)
{
	static FILE* fp2 = NULL;
	fp2 = fopen("./schedule_dump.txt", "a");

	printf("\n****************child***************\n");
	fprintf(fp2, "\n****************child***************\n");

	//Choose IO of percentage 25% (1/4)
	int io_work_exec = rand() % 4;
	//IO time : 1 ~ 3 sec
	int io_burst = (rand() % 2) + 2;
	int msgq;
	int ret;
	int key = 0x12345;
	struct msgbuf msg;

	if (!(io_work_exec == 1 && cpu_burst == 4)) {
		
		//if cpu burst becomes 1, set it to 4 again
		if (cpu_burst == 1)
			cpu_burst = 4;
		else
			cpu_burst--;
	}

	//Send message
	msgq = msgget(key, IPC_CREAT | 0666);
	printf("msgq id: %d\n", msgq);
	fprintf(fp2, "msgq id: %d\n", msgq);
	memset(&msg, 0, sizeof(msg));
	for(int i=0; i<10; i++){
		msg.access_request[i] = rand() % 0x10000;
	}
	msg.mtype = getpid();
	msg.pid = getpid();
	msg.io_burst = io_burst;
	msg.cpu_burst = cpu_burst;
	msg.io_work_exec = io_work_exec;
	ret = msgsnd(msgq, &msg, sizeof(msg), IPC_NOWAIT);
	printf("msgsnd ret: %d\n", ret);
	fprintf(fp2, "msgsnd ret: %d\n", ret);



	printf("(%d) SIGALRM signaled!\n", getpid());
	fprintf(fp2, "(%d) SIGALRM signaled!\n", getpid());

	count++;
	fclose(fp2);
}

void signal_handler(int signo)
{
	static FILE* fp2 = NULL;
	fp2 = fopen("./schedule_dump.txt", "a");

	int msgq;
	int ret;
	int key = 0x12345;
	msgq = msgget(key, IPC_CREAT | 0666);

	struct msgbuf msg;


	fprintf(fp2, "\n****************OS******************\n");
	printf("\n****************OS******************\n");
	fprintf(fp2, "time passed : %d\n", count);
	printf("time passed : %d\n", count);


	memset(&msg, 0, sizeof(msg));
	ret = msgrcv(msgq, &msg, sizeof(msg), 0, MSG_NOERROR | IPC_NOWAIT);
	printf("[ MSG received ]\n");
	fprintf(fp2, "[ MSG received ]\n");
	printf("msg.pid: %d\n", msg.pid);
	fprintf(fp2, "msg.pid: %d\n", msg.pid);
	printf("msg.io_burst: %d\n", msg.io_burst);
	fprintf(fp2, "msg.io_burst: %d\n", msg.io_burst);
	printf("msg.cpu_burst : %d\n", msg.cpu_burst);
	fprintf(fp2, "msg.cpu_burst : %d\n", msg.cpu_burst);
	
	printf("\n<Memory Translation>\n");
	fprintf(fp2, "\n<Memory Translation>\n");

	fclose(fp2);

	for(int i=0; i<10; i++){
		int pa = mem_mapping(msg.access_request[i]);

		fp2 = fopen("./schedule_dump.txt", "a");

		printf("Virtual address : 0X%4x -> Physical address : 0X%4x\n", msg.access_request[i], pa);
		fprintf(fp2, "Virtual address : 0X%4x -> Physical address : 0X%4x\n", msg.access_request[i], pa);

		fclose(fp2);
	}
	
	fp2 = fopen("./schedule_dump.txt", "a");

	//decrease remaining wait time which is front of the queue
	if (waitq->count > 0)
		waitq->front->remaining_wait--;

	//if remaining wait time of front node in wait queue becomes zero, it is sent to appropriate runq
	if (waitq->count > 0) {
		if (waitq->front->remaining_wait == 0) {
			struct proc_t* waitp = waitq->front;
			dequeue(waitq);
			add_queue(runq[waitp->level], waitp);
			printf("\n-----(%d)finished IO and returned to runq(level : %d)-----\n", waitp->pid, waitp->level);
			fprintf(fp2, "\n-----(%d)finished IO and returned to runq(level : %d)-----\n", waitp->pid, waitp->level);
		}
	}

	//Choose which process to run in multi-level queue
	//save the level choice in 'level_choice' variable, because it will be needed
	if (msg.cpu_burst == 4 || count == 0 || target_pid == already_exit[exit_num - 1] || target_pid == 1) {
		if (runq[4]->count != 0) {
			target_pid = runq[4]->front->pid;
			level_choice = 4;
			printf("\nPicked process from level : 4\n");
			fprintf(fp2, "\nPicked process from level : 4\n");
		}
		else if (runq[3]->count != 0) {
			target_pid = runq[3]->front->pid;
			level_choice = 3;
			printf("\nPicked process from level : 3\n");
			fprintf(fp2, "\nPicked process from level : 3\n");
		}
		else if (runq[2]->count != 0) {
			target_pid = runq[2]->front->pid;
			level_choice = 2;
			printf("\nPicked process from level : 2\n");
			fprintf(fp2, "\nPicked process from level : 2\n");
		}
		else if (runq[1]->count != 0) {
			target_pid = runq[1]->front->pid;
			level_choice = 1;
			printf("\nPicked process from level : 1\n");
			fprintf(fp2, "\nPicked process from level : 1\n");
		}
		else if (runq[0]->count != 0) {
			target_pid = runq[0]->front->pid;
			level_choice = 0;
			printf("\nPicked process from level : 0\n");
			fprintf(fp2, "\nPicked process from level : 0\n");
		}
		else
			target_pid = 1;
	}

	struct proc_t* p = runq[level_choice]->front;
	//Process chooses to do IO work
	if (msg.io_work_exec == 1 && msg.cpu_burst == 4) {
		printf("\n-----pid (%d) is sent to WAITQ -> pick other process-----\n", runq[level_choice]->front->pid);
		fprintf(fp2, "\n-----pid (%d) is sent to WAITQ -> pick other process-----\n", runq[level_choice]->front->pid);

		dequeue(runq[level_choice]);
		add_queue(waitq, p);
		waitq->rear->remaining_wait = msg.io_burst;
		
		//Level up the process beacause it does IO work
		if (p->level != 4 && msg.cpu_burst == 4)
			p->level++;

		if (runq[4]->count != 0) {
			target_pid = runq[4]->front->pid;
			level_choice = 4;
			printf("\nPicked process from level : 4\n");
			fprintf(fp2, "\nPicked process from level : 4\n");
		}
		else if (runq[3]->count != 0) {
			target_pid = runq[3]->front->pid;
			level_choice = 3;
			printf("\nPicked process from level : 3\n");
			fprintf(fp2, "\nPicked process from level : 3\n");
		}
		else if (runq[2]->count != 0) {
			target_pid = runq[2]->front->pid;
			level_choice = 2;
			printf("\nPicked process from level : 2\n");
			fprintf(fp2, "\nPicked process from level : 2\n");
		}
		else if (runq[1]->count != 0) {
			target_pid = runq[1]->front->pid;
			level_choice = 1;
			printf("\nPicked process from level : 1\n");
			fprintf(fp2, "\nPicked process from level : 1\n");
		}
		else if (runq[0]->count != 0) {
			target_pid = runq[0]->front->pid;
			level_choice = 0;
			printf("\nPicked process from level : 0\n");
			fprintf(fp2, "\nPicked process from level : 0\n");
		}

		//To know whether runq is empty, set target pid to 1
		else
			target_pid = 1;


	}

	else {
		if (msg.cpu_burst == 1) {
			if (p->level != 0)
				p->level--;
		}
	}


	printf("(%d)->(%d) signal!\n", getpid(), target_pid);
	fprintf(fp2, "(%d)->(%d) signal!\n", getpid(), target_pid);

	// send child a signal SIGUSR1
	kill(target_pid, SIGALRM);

	int runq_count = runq[0]->count + runq[1]->count + runq[2]->count + runq[3]->count + runq[4]->count;

	//decrease the remaining time quantum
	if (!(msg.io_work_exec == 1 && msg.cpu_burst == 4) && target_pid != already_exit[exit_num - 1] && runq_count != 0) {
		runq[level_choice]->front->remaining_tq--;
		printf("\n\n(%d) pid's remaining tq : %d\n", runq[level_choice]->front->pid, runq[level_choice]->front->remaining_tq);
		fprintf(fp2, "\n\n(%d) pid remaining tq : %d\n", runq[level_choice]->front->pid, runq[level_choice]->front->remaining_tq);
	}

	if (msg.cpu_burst == 1 && target_pid != already_exit[exit_num - 1]) {

		if (!(msg.io_work_exec == 1 && msg.cpu_burst == 4)) {
			if (runq[level_choice]->front->remaining_tq <= 0) {
				
				//if remaining time quantum is zero, kill the process
				kill(runq[level_choice]->front->pid, SIGKILL);
				printf("process (%d) finished executing!\n", runq[level_choice]->front->pid);
				fprintf(fp2, "process (%d) finished executing!\n", runq[level_choice]->front->pid);

				already_exit[exit_num] = runq[level_choice]->front->pid;
				exit_num++;

				//delete the dead process's node
				if (runq[level_choice]->count >= 2) {
					runq[level_choice]->front = runq[level_choice]->front->next;

				}
				else {
					runq[level_choice]->front = NULL;
					runq[level_choice]->rear = NULL;

				}

				//free(runq[level_choice]->front);
				runq[level_choice]->count--;
			}
			else {
				struct proc_t* pp = runq[level_choice]->front;
				dequeue(runq[level_choice]);
				add_queue(runq[pp->level], pp);
				printf("process (%d) is sent to runq level %d!\n", pp->pid, pp->level);
				fprintf(fp2, "process (%d) is sent to runq level %d!\n", pp->pid, pp->level);
			}
		}


	}

	count++;

	
	
	if(msg.cpu_burst == 1){
		clear_mapping();
		printf("Memory cleaned!\n");
		fprintf(fp2, "Memory cleaned!\n");
	}

	printf("Left free page count : 0x%x\n",free_page_list->count);
	fprintf(fp2, "Left free page count : 0x%x\n", free_page_list->count);


	//If none of the process is alive, parent process exits.
	if (runq[0]->count == 0 && runq[1]->count == 0 && runq[2]->count == 0 && runq[3]->count == 0 && runq[4]->count == 0 && waitq->count == 0) {
		exit(0);
	}

	printf("\n< Queue Statistics >\n");

	fprintf(fp2, "runq[0] count ::::: %d\n", runq[0]->count);
	fprintf(fp2, "runq[1] count ::::: %d\n", runq[1]->count);
	fprintf(fp2, "runq[2] count ::::: %d\n", runq[2]->count);
	fprintf(fp2, "runq[3] count ::::: %d\n", runq[3]->count);
	fprintf(fp2, "runq[4] count ::::: %d\n", runq[4]->count);
	fprintf(fp2, "waitq   count ::::: %d\n", waitq->count);


	printf("runq[0] count ::::: %d\n", runq[0]->count);
	printf("runq[1] count ::::: %d\n", runq[1]->count);
	printf("runq[2] count ::::: %d\n", runq[2]->count);
	printf("runq[3] count ::::: %d\n", runq[3]->count);
	printf("runq[4] count ::::: %d\n", runq[4]->count);
	printf("waitq   count ::::: %d\n", waitq->count);

	fclose(fp2);
}
