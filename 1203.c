#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#define t_quantum 2
#define FRAMESIZE 1024

// 가상 메모리는 16bit address space(0~0xffff) 메인 메모리는 1MB(0~0xfffff) 의 주소공간을 가짐
// page frame size 를 4KB가정, 따라서 offset은 12bit 을 가짐 , PFN은 0~255(256개)의 INDEX를 가진다.
// 가상 메모리 주소를 16bit 으로 한정 하고 page frame size를 4KB로 가정 했기때문에 가상 메모리의 page number은 0~15 이다.
struct msgbuf {
	int pid;
	int io_time;
};
struct msgbuf2{
	int random_address[10];
};

typedef struct page_table_entry{
	int valid;
	int frame_number;
}page_table_entry;

typedef struct page_frame_entry{
	int free;
	int frame_number;

}page_frame_entry;

typedef struct Node{
	int data;
	struct Node* link;
	struct Node* b_link;
}Node;

typedef struct free_PFN{
	int count;
	Node* top;
	Node* bottom;
}free_PFN;

typedef struct p_PCB {
	int pid;
	int burst_time;
	int remaining_wait;
	int time_quantum;
	int state;
	int io_timer;
	struct p_PCB* next;

	struct page_table_entry page_table[16];
}p_PCB;

typedef struct Run_q {
	p_PCB* front;
	p_PCB* rear;
	int count;
}Run_q;

void signal_handler(int signo);
void signal_handler2(int signo);
void add_queue(struct Run_q* run_q, int pid, int burst, int wait, int when, int state);
void add_queue2(struct Run_q* run_q, int pid, int burst, int wait, int when, int state, struct page_table_entry page[]);
void add_queue_first(struct Run_q* run_q, int pid, int burst, int wait, int when, int state);

void InitQueue(struct Run_q* run_q);
int IsEmpty(struct Run_q* run_q);
struct p_PCB* pop_queue(struct Run_q* run_q);

void init(free_PFN* PFN);
int empty(free_PFN* PFN);
void push(free_PFN* PFN, int data);
int pop(free_PFN* PFN);
int pop_bottom(free_PFN* PFN);

struct Node* Search(free_PFN* LRU_S, int data);
void Delete(free_PFN* LRU_S , Node* removenode);

int pids[10]; //used in main
int time_quantum[10]; //used in main
int count = 0; //both used
int exec_time = 0; //used by child
int total_exec_time = 0;//used by child
int io_when[10]; //used by child
int total_CPU_burst_time = 0; //used by parent
int wait = 0;
int child_timing = 0;

struct Run_q run_q; //CPU
struct Run_q wait_q; //IO

struct free_PFN PFN; //free PFN
struct free_PFN LRU_STACK; //LRU STACK
FILE* fp = NULL;

struct page_frame_entry memory[64];

int main()
{
	fp = fopen("123.txt", "w");
	if (fp == NULL) {
		perror("can't make dump text");
	}


	int fd[2];
	int fd1[2];
	srand(time(NULL));
	InitQueue(&run_q);
	InitQueue(&wait_q);
	for (int i = 0; i < 10; i++) {
		pids[i] = 0;
		time_quantum[i] = (rand() % 50) + 20;
	}
	// child fork
	for (int i = 0; i < 10; i++) {
		pipe(fd);
		pipe(fd1);
		int burst = 0;
		int timing = 0;
		int ret = fork();
		if (ret < 0) {
			// fork fail
			perror("fork_failed");
		}
		else if (ret == 0) {
			// child

			close(fd[0]);
			close(fd1[0]);
			// signal handler setup
			struct sigaction old_sa;
			struct sigaction new_sa;
			memset(&new_sa, 0, sizeof(new_sa));
			new_sa.sa_handler = &signal_handler2;
			sigaction(SIGALRM, &new_sa, &old_sa);

			//excution time
			burst = time_quantum[i];
			exec_time = burst;
			write(fd[1], &burst, sizeof(burst));
			while (1);
			exit(0);
			// never reach here
		}
		else if (ret > 0) {
			// parent

			close(fd[1]);
			read(fd[0], &burst, sizeof(burst));

			total_CPU_burst_time = total_CPU_burst_time + burst;
			pids[i] = ret;
			add_queue(&run_q, pids[i], burst, 0, 0, 0);

			//			fprintf(fp, "child %d created, exec %d, timing %d\n", pids[i], burst, timing);
			//fprintf(fp, "child %d created, exec %d\n", pids[i], burst);
			printf("child %d created, exec %d\n", pids[i], burst);
			fflush(fp);
		}
	}

	//set PFN

	for(int i =256; i>=0; i--){
		push(&PFN,i);
	}
	//fprintf(fp, "total cpu burst time is %d\n", total_CPU_burst_time);
	printf("total cpu burst time is %d\n", total_CPU_burst_time);
	fflush(fp);
	// signal handler setup
	struct sigaction old_sa;
	struct sigaction new_sa;
	memset(&new_sa, 0, sizeof(new_sa));
	new_sa.sa_handler = &signal_handler;
	sigaction(SIGALRM, &new_sa, &old_sa);

	// fire the alrm timer
	struct itimerval new_itimer, old_itimer;
	new_itimer.it_interval.tv_sec = 1;
	new_itimer.it_interval.tv_usec = 0;
	new_itimer.it_value.tv_sec = 1;
	new_itimer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &new_itimer, &old_itimer);

	while (1);
	return 0;
}




void signal_handler2(int signo)
{
	srand(time(NULL));
	count++;
	int msgq2;
	int ret2;
	int key2 = 0x12345;
	msgq2= msgget(key2, IPC_CREAT | 0666);
	struct msgbuf2 msg2;
	memset(&msg2, 0, sizeof(msg2));
	srand((unsigned int)time(NULL));

	for(int i=0; i <10; i++){
		msg2.random_address[i] = rand() % 0xffff;
	}
	ret2 = msgsnd(msgq2, &msg2, sizeof(msg2),0);
	if(count == exec_time){
		exit(0);
	}
	fflush(fp);
}

void signal_handler(int signo)
{
	int msgq;
	int msgq2;

	while(PFN.count < 30){
		struct p_PCB* pcb = run_q.front;
		int LRU_data = pop_bottom(&LRU_STACK);
		for(int i =0; i<run_q.count; i++){
			for(int j=0; j<16;j++){
				if(pcb->page_table[j].frame_number==LRU_data){
					pcb->page_table[j].valid=0;
					break;
				}
			}
			pcb = pcb->next;
		}		
	}
	//runq
	if (!IsEmpty(&run_q)) {
		struct p_PCB* r_PCB = malloc(sizeof(p_PCB));
		r_PCB = run_q.front;
		int target_pid = r_PCB->pid;
		kill(target_pid, SIGALRM);
		count++;
		r_PCB->burst_time -= 1;
		r_PCB->io_timer -= 1;

		int ret2;
		int key2 = 0x12345;
		msgq2 = msgget(key2, IPC_CREAT | 0666);
		struct msgbuf2 msg2;
		memset(&msg2, 0,sizeof(msg2));
		ret2 = msgrcv(msgq2, &msg2, sizeof(msg2),0,0);


		int pop_n =0;

		for(int i =0; i<10; i++){
			printf("0x%x",msg2.random_address[i]);
			if(r_PCB->page_table[msg2.random_address[i]>>12].valid == 0){
				//match new table page

				pop_n = pop(&PFN);
				r_PCB->page_table[msg2.random_address[i]>>12].frame_number=pop_n;
				r_PCB->page_table[msg2.random_address[i]>>12].valid = 1;

				//계산
				int result = ((r_PCB->page_table[msg2.random_address[i]>>12].frame_number)<<12)|(msg2.random_address[i]&0xfff);

				struct Node* push_LRU = Search(&LRU_STACK,pop_n);
				if(push_LRU == NULL){
					push(&LRU_STACK,pop_n);
				}
				else{
					Delete(&LRU_STACK,push_LRU);
					push(&LRU_STACK,pop_n);
				}
				printf("-> 0x%x\n", result);
			}
			else if (r_PCB->page_table[msg2.random_address[i]>>12].valid == 1){
				//find physical address
				int result = ((r_PCB->page_table[msg2.random_address[i]>>12].frame_number)<<12)|(msg2.random_address[i]&0xfff);
				struct Node* push_LRU2 = Search(&LRU_STACK, r_PCB->page_table[msg2.random_address[i]>>12].frame_number);
				if(push_LRU2==NULL){
					push(&LRU_STACK,r_PCB->page_table[msg2.random_address[i]>>12].frame_number);
				}
				else{
					Delete(&LRU_STACK,push_LRU2);
					push(&LRU_STACK,r_PCB->page_table[msg2.random_address[i]>>12].frame_number);
				}
				printf("-> 0x%x\n", result);

				//계산
			}
			else{
				printf("ERROR!!!!!!!!!!!\n");
			}
		}
		printf("----%d TABLE-----\n", target_pid);
		for (int i = 0; i <16; i++){
			printf("%2d: %d\n", i, r_PCB->page_table[i].frame_number);
		}
		printf("-----------------\n");
		//exit processor
		if (r_PCB->burst_time == 0) {
			//fprintf(fp, "RUN _Q(%d) count: %d, process end\n", target_pid, count);
			printf("RUN _Q(%d) count: %d, process end\n\n", target_pid, count);
			r_PCB = pop_queue(&run_q);
			free(r_PCB);
		}

		//time-quantum is 0
		else if (r_PCB->time_quantum == 0) {
			// fprintf(fp, "RUN _Q(%d) count: %d, tq: 0. go to end\n", target_pid, count);
			printf("RUN _Q(%d) count: %d, tq: 0. go to end\n", target_pid, count);
			r_PCB = pop_queue(&run_q);
			add_queue2(&run_q, r_PCB->pid, r_PCB->burst_time, 0, r_PCB->io_timer, r_PCB->state, r_PCB->page_table);
		}
		//general case
		else {
			//   fprintf(fp, "RUN _Q(%d) count: %d, tq: %d.\n", target_pid, count, r_PCB->time_quantum);
			printf("RUN _Q(%d) count: %d, tq: %d.\n", target_pid, count, r_PCB->time_quantum);
			r_PCB->time_quantum -= 1;
		}

	}
	//end
	if (count == total_CPU_burst_time){
		//fprintf(fp, "FINISHED\n");
		printf("FINISHED\n");
		msgctl(msgq, IPC_RMID, 0);
		msgctl(msgq2,IPC_RMID, 0);
		fclose(fp);
		exit(0);
	}
	fflush(fp);
}



void add_queue(struct Run_q* run_q, int pid, int burst, int wait, int when, int state) {

	struct p_PCB* newNode = malloc(sizeof(p_PCB));
	//	struct page_table_entry page = malloc(sizeof(page_table_entry));	

	newNode->pid = pid;
	newNode->burst_time = burst;
	newNode->remaining_wait = wait;
	newNode->next = NULL;
	newNode->time_quantum = t_quantum;
	newNode->state = state;
	newNode->io_timer = when;
	if (IsEmpty(run_q)) {
		run_q->front = run_q->rear = newNode;
	}
	else {
		run_q->rear->next = newNode;
		run_q->rear = newNode;
	}

	run_q->count++;
}

void add_queue2(struct Run_q* run_q, int pid, int burst, int wait, int when, int state, struct page_table_entry page[]) {

	struct p_PCB* newNode = malloc(sizeof(p_PCB));

	newNode->pid = pid;
	newNode->burst_time = burst;
	newNode->remaining_wait = wait;
	newNode->next = NULL;
	newNode->time_quantum = t_quantum;
	newNode->state = state;
	newNode->io_timer = when;
	if (IsEmpty(run_q)) {
		run_q->front = run_q->rear = newNode;
	}
	else {
		run_q->rear->next = newNode;
		run_q->rear = newNode;
	}
	for (int i = 0; i<16; i++){
		newNode->page_table[i].valid = page[i].valid;
		newNode->page_table[i].frame_number = page[i].frame_number;
	}
	run_q->count++;
}

void add_queue_first(struct Run_q* run_q, int pid, int burst, int wait, int when, int state) {

	struct p_PCB* newNode = malloc(sizeof(p_PCB));

	newNode->pid = pid;
	newNode->burst_time = burst;
	newNode->remaining_wait = wait;
	newNode->next = NULL;
	newNode->time_quantum = t_quantum;
	newNode->state = state;
	newNode->io_timer = when;
	if (IsEmpty(run_q)) {
		run_q->front = run_q->rear = newNode;
	}
	else {
		newNode->next = run_q->front;
		run_q->front = newNode;
	}

	run_q->count++;
}

struct p_PCB* pop_queue(struct Run_q* run_q) {

	struct p_PCB* newNode = malloc(sizeof(p_PCB));

	newNode = run_q->front;
	run_q->front = newNode->next;
	run_q->count--;
	return newNode;
}

void InitQueue(struct Run_q* run_q) {

	run_q->front = run_q->rear = NULL;
	run_q->count = 0;
}

int IsEmpty(struct Run_q* run_q) {

	if (run_q->count == 0)
		return 1;
	else
		return 0;
}
// data structure function for free_PFN

void init(free_PFN* PFN) {
	PFN->top = NULL;
	PFN->count = 0;
}

int empty(free_PFN* PFN)//공백상태
{
	if (PFN->top == NULL) {
		return 1;
	}
	else
		return 0;
}

void push(free_PFN* PFN, int data)
{
	Node* newptr = (Node*)malloc(sizeof(Node));

	if (newptr == NULL) {
		//		fprintf(stderr, "메모리 할당에러");
		printf("memory error: push\n");
	}
	else {
		if(PFN->top ==NULL){
			newptr->data = data;
			newptr->link = PFN->top;
			PFN->bottom = newptr;
			PFN->top = newptr;
			(PFN->count)++;
		}
		else{
			PFN->top->b_link = newptr;
			newptr->data = data;
			newptr->link = PFN->top;
			PFN->top = newptr;
			(PFN->count)++;
		}
	}
}

int pop(free_PFN* PFN) {
	if (PFN->top == NULL) {
		return -1;
	}
	else {
		Node* newptr = (Node*)malloc(sizeof(Node));

		newptr->link = PFN->top;
		int data = PFN->top->data;
		PFN->top = PFN->top->link;
		(PFN->count)--;
		free(newptr);
		return data;
	}
}
int pop_bottom(free_PFN* PFN) {
	if (PFN->top == NULL) {
		return -1;
	}
	else {
		Node* newptr = (Node*)malloc(sizeof(Node));

		newptr->link = PFN->bottom;
		int data = PFN->bottom->data;
		PFN->bottom->b_link->link = NULL;
		(PFN->count)--;
		free(newptr);
		return data;
	}
}

struct Node* Search(free_PFN* LRU_S, int data){

	if(LRU_S->top ==NULL){
		return NULL;}
	Node* newptr  = LRU_S -> top;

	while(newptr->link ==NULL){
		if(newptr->data==data)
			return newptr;

		newptr = newptr->link;

	}

	return NULL;
}
void Delete(free_PFN* LRU_S , Node* removenode){

	if(removenode == LRU_S->top){
		LRU_S->top = removenode->link;
		free(removenode);
	}
	else{
		removenode->b_link = removenode->link;
		free(removenode);
	}
}



