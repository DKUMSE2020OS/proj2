/*
    OS Project #2 Multi-Process Execution with Virtual Memory
    Group
    32161570 Park Kitae
    32162066 Byun Sangun
    CPU -> RunQ, Round Robin with Time Quantum 4

    Compile : make

    View Result : cat result.txt
*/
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
#include <sys/stat.h>
#include <fcntl.h>


/*
    Virtual Memory 32bit
    Physical Memory 32bit
*/
#define CHILDNUM 10
#define PDIRNUM 1024
#define PTLBNUM 1024
#define FRAMENUM 1024   // 4B X 1KB = 4KB
#define QNUM 32
#define Time_Q 4           //Time_Quantum

struct Proc_t {     // PCB
    pid_t pid;
    int burst;
    int time_quantum;
    struct Proc_t *next;    // Next Node
};


struct Queue_t{   //Run_Queue
    struct Proc_t* front;
    struct Proc_t* rear;
    int count;
};

typedef struct{     // 2nd Table
    int valid;
    int pfn;
}TABLE;

typedef struct{     // 1st Table
    int valid;
    TABLE*  pt;
}PDIR; 

struct msg_buffer{
    long int  mtype;
    int pid_index;
    unsigned int  vm[10];
};

PDIR pdir[CHILDNUM][PDIRNUM];
pid_t pids[CHILDNUM];                 // PID per Process

struct Queue_t runq_t;           // Tick: 1sec

struct msg_buffer msg;
int msgq;
int ret;
int key = 0x12345;
int count = 0;
int process_time[10];           // Burst Time per Process

int state = 0;

int random_burst_time = 0;      // Burst time per Process
int total_burst_time = 0;       // Total Burst Time
int RQ[QNUM];   //RUN_QUEUE
int hd,tl = 0;  // Head, Tail
int free_page_list[1024];   //FREE PAGE LIST
int fpl_tl,fpl_hd = 0;      //FREE PAGE LIST Head, Tail
int idx, total_tik, proc_done = 0;

FILE* fd = NULL;

void init_queue(struct Queue_t q);
void add_queue(struct Queue_t *rq, int pid, int burst, int tq);
struct Proc_t* pop_queue(struct Queue_t* rq);
void signal_handler(int signo);
void signal_handler2(int signo);
void toFreePageList(PDIR* p_dir);
int MMU(int free_page_list[], int index ,int vm);

void init_queue(struct Queue_t q){   //Initialize Queue
    q.front = NULL;
    q.rear = NULL;
    q.count = 0;
}

void add_queue(struct Queue_t *rq, int pid, int burst, int tq){

    struct Proc_t *temp = malloc(sizeof(struct Proc_t));

    temp->pid = pid;
    temp->burst = burst;
    temp->time_quantum = tq;
    temp->next = NULL;
    
    if(rq->count == 0){     //Queue Empty or not?
        rq->front = temp;
        rq->rear = temp;
    } else {
        rq->rear->next = temp;
        rq->rear = temp;
    }
    
    rq->count++;
}

struct Proc_t* pop_queue(struct Queue_t* rq){   //Ruturn Process

    struct Proc_t* temp = malloc(sizeof(struct Proc_t));

    temp = rq->front;
    rq->front = temp->next;
    rq->count--;

    return temp;
}

int main(){
    printf("-------------PROJECT #2 Virtual Memory-------------\n");
    printf("-------------32161570 Kitae Park----------------------\n");
    printf("-------------32162066 Sangun Byun---------------------\n\n");
    printf("------------------------------------------------------\n\n");

    //Open file 'result.txt'
    fd = fopen("result.txt", "w");

    if(fd == NULL){
        perror("Failed to Create txt file.\n");
        exit(0);
    }

    unsigned int vm[10];
    unsigned int pdir_idx[10];
    unsigned int tbl_idx[10];
    unsigned int offset[10];
    int pid_index;
    int physical_addr = 0;

    init_queue(runq_t);
    srand(time(NULL));
    msgq = msgget( key, IPC_CREAT | 0666);

    // Init Processes
    for (int i = 0 ; i < 10; i++) {
        pids[i] = 0;    // Initialize PID
        process_time[i] = rand() % 50 + 100;  // Set Burst TIme
    }

    // Init FreePageLists
    for(int i = 0 ; i < FRAMENUM; i++){
        free_page_list[i] = i ;
        fpl_tl++ ;
    }

    for (int i = 0 ; i < CHILDNUM; i++){

        RQ[(tl++) % QNUM] = idx;

        // Init PDIR
        for(int l = 0; l < CHILDNUM ; l++){
            for(int j = 0; j < PDIRNUM ; j++){
                pdir[l][j].valid =0;
                pdir[l][j].pt = NULL;
            }
        }

        int ret = fork();
        
        if(ret < 0){    //Fork Failed
            perror("fork_failed");
        } else if (ret == 0) {  //Child

            // Signal handler setup
            struct sigaction old_sa;
            struct sigaction new_sa;
            memset(&new_sa, 0, sizeof(new_sa));
            new_sa.sa_handler = &signal_handler2;
            sigaction(SIGALRM, &new_sa, &old_sa);
            // Execution Time
            random_burst_time = process_time[i];

            //Repeating
            while (1);
            exit(0); // Never reach here

        } else if (ret > 0){    //Parent

            total_burst_time += process_time[i];
            pids[i] = ret;
            printf("Child %d created, BT: %dsec\n", pids[i], process_time[i]);

            // Put it in the RunQ
            add_queue(&runq_t, pids[i], process_time[i], Time_Q-1);
        }

        idx ++;
    }

    printf("-------------------Now Scheduling---------------------\n\n");
    printf("-------------------Please Wait---------------------\n\n");

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

    while(1){
        ret = msgrcv(msgq,&msg,sizeof(msg),IPC_NOWAIT,IPC_NOWAIT);

        if(ret != -1){

            pid_index = msg.pid_index;

            for(int k = 0; k < CHILDNUM; k++){

                vm[k]=msg.vm[k];

                pdir_idx[k] = (vm[k] & 0xffc00000) >> 22;   // 10bit
                tbl_idx[k] = (vm[k] & 0x3ff000) >> 12;    // 10bit
                offset[k] = vm[k] & 0xfff;

                // First Paging
                if(pdir[pid_index][pdir_idx[k]].valid == 0){
                    printf("1-Level PAGE FAULT!\n");
                    fprintf(fd, "1-Level PAGE FAULT!\n");
                    TABLE* table = (TABLE*)calloc(PTLBNUM, sizeof(TABLE));
                    pdir[pid_index][pdir_idx[k]].pt = table;
                    pdir[pid_index][pdir_idx[k]].valid = 1;
                }

                TABLE* ptbl = pdir[pid_index][pdir_idx[k]].pt;

                // Second Paging
                if(ptbl[tbl_idx[k]].valid== 0){
                    // sprintf
                    printf("2-Level PAGE FAULT!\n");
                    fprintf(fd, "2-Level PAGE FAULT!\n");

                    if(fpl_hd != fpl_tl || (runq_t.count > 0) ){
                        ptbl[tbl_idx[k]].pfn = free_page_list[(fpl_hd % FRAMENUM)];
                        ptbl[tbl_idx[k]].valid = 1;
                        fpl_hd++;
                    } 
                    else {
                            
                        for(int k = 0; k < CHILDNUM ; k ++) {
                            kill(pids[k],SIGKILL);
                        }
                        msgctl(msgq, IPC_RMID, NULL);
                        exit(0);
                        return 0;
                    }
                }

                physical_addr = MMU(free_page_list, fpl_hd, vm[k]);

                printf("PID : %d -> ", pids[k]);
                printf("VA : 0x%08x[PDIR : 0x%03x, PTBL : 0x%03x , OFFSET : 0x%04x] -> PA:0x%08x\n"
                , vm[k], pdir_idx[k], tbl_idx[k], offset[k], physical_addr);

                fprintf(fd, "PID : %d -> ", pids[k]);
                fprintf(fd, "VA : 0x%08x[PDIR : 0x%03x, PTBL : 0x%03x , OFFSET : 0x%04x] -> PA:0x%08x\n"
                    , vm[k], pdir_idx[k], tbl_idx[k], offset[k], physical_addr);
                
            }
            memset(&msg, 0, sizeof(msg));
        }
    }

    while (1);
    fclose(fd);
    return 0;


}

int MMU(int free_page_list[], int index ,int vm){
    
    int offset = vm & 0xfff;

    int physical_addr;
    int pfn;
    
    pfn = free_page_list[(fpl_hd%FRAMENUM)];
    
    physical_addr = (pfn<<12) + offset;
    
    return physical_addr;
}

void signal_handler2(int signo){    //Signal Child(User)->Parent(OS)
 
    count++;
       
    memset(&msg,0,sizeof(msg));
    msg.mtype = IPC_NOWAIT;
    msg.pid_index = idx;
        
    unsigned int temp_add;
    for (int k = 0; k < 10; k++){   // Random Mem Access

        temp_add = (rand() % 0x4) << 22; // 10 bit for directory page table
        temp_add |= (rand() % 0x20) << 12; // 10 bit for page table
        temp_add |= (rand() % 0xfff); // 12 bit for offset
        msg.vm[k] = temp_add;
    }
    
    ret = msgsnd(msgq, &msg, sizeof(msg),IPC_NOWAIT);

    if(count == random_burst_time) {
        printf("-------------Child (%d) CPU Execution Completed!-------------\n\n", getpid());
        idx++;
        exit(0);
    }

}

void toFreePageList(PDIR* p_dir)
{
    PDIR* temp_dir = p_dir;
    for(int i=0; i< PDIRNUM ; i++)
    {
        if(temp_dir[i].valid == 1 ){
            temp_dir[i].valid=0;
            for(int j=0 ; j < PTLBNUM ; j++){
                if((temp_dir[i].pt)[j].valid == 1){
                    free_page_list[(fpl_tl++)%FRAMENUM]=(temp_dir[i].pt)[j].pfn;
                }
            }
            temp_dir[i].pt =NULL;
            free(temp_dir[i].pt);
        }
    }
}

void signal_handler(int signo)  //Signal Parent(OS)->Child(User)
{
    
    struct Proc_t* p = malloc(sizeof(struct Proc_t));
    p = runq_t.front;
    int target_pid = p->pid;
    
    if(proc_done == 1){
        toFreePageList(pdir[RQ[(hd-1)%QNUM]]);
        proc_done = 0;
    }
    
    count++;
    printf("\n———————————Scheduler——————————\n");
    printf("Count : %d\n", count);
    printf("total_burst_time : %d \n", total_burst_time);
   
    fprintf(fd, "\n———————————Scheduler——————————\n");
    fprintf(fd, "Count : %d\n", count);
    fprintf(fd, "total_burst_time : %d \n", total_burst_time);
        

    int burst = 0;  //Remaining Burst Time

    // Run queue

    printf(" CPU Burst : Parent (%d) -> Child (%d)\n", getpid(), target_pid);
    fprintf(fd, " CPU Burst : Parent (%d) -> Child (%d)\n", getpid(), target_pid);

    // Send child a signal SIGUSR1
    kill(target_pid, SIGALRM);

    p->burst -= 1;

        
    if(p->burst == 0){  // In RunQ, consume all burst time
        hd++;
        p = pop_queue(&runq_t);
        printf("\t->Remain_burst_time : %d\n", 0);
        fprintf(fd, "\t->Remain_burst_time : %d\n", 0);
        free(p);
    } else if (p->time_quantum == 0) {    // In RunQ, consume all time quantum
        hd++;
        p = pop_queue(&runq_t);
        add_queue(&runq_t, p->pid, p->burst, Time_Q-1);
        burst = p->burst;

        printf("\t->Remain_burst_time : %d\n", burst);
        fprintf(fd, "\t->Remain_burst_time : %d\n", burst);
    } else {  // In RunQ, remain both time quantum and burst time
        p->time_quantum -= 1;
        burst = p->burst;
                
        printf("\t->Remain_burst_time : %d\n", burst);
        fprintf(fd, "\t->Remain_burst_time : %d\n", burst);
    }


    struct Proc_t* temp = malloc(sizeof(struct Proc_t));
    temp = runq_t.front;


    //Print Current Status of Run Queue
    printf("\nRUN  QUEUE -> ");
    fprintf(fd, "\nRUN  QUEUE -> ");

    for(int i = 0; i < runq_t.count; i++) {
        printf(" [%d] ", temp->pid);
        fprintf(fd, " [%d] ", temp->pid);
        temp = temp->next;
    }
        
    printf("\n\n");
    fprintf(fd, "\n\n");


    printf("\nTime Quantum : %d\n", Time_Q);
    printf("Run_Queue count: %d \n", runq_t.count);
    printf("——————————————————————————————\n");

    fprintf(fd, "\nTime Quantum : %d\n", Time_Q);
    fprintf(fd, "Run_Queue count: %d \n", runq_t.count);
    fprintf(fd, "——————————————————————————————\n");

    // Finish Scheduling
    if (count == total_burst_time ) {
        printf("—————————————————————————————————————————————————————\n");
        printf("————————————Result are saved as result.txt———————————\n");
        printf("—————————————————————————————————————————————————————\n");
        fprintf(fd, "—————————————————————Programme END———————————————————\n");
        fprintf(fd, "—————————————————————————————————————————————————————\n");
        
        fclose(fd);
        exit(0);
    }

}

