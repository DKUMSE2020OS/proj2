import os 
import time
import threading
import random
import signal
import copy 
import multiprocessing

total_time = multiprocessing.Value('d',0)
end = multiprocessing.Value('d',0)


shared_cpu_burst = multiprocessing.Value('d',0)
shared_io_burst = multiprocessing.Value('d',0)
shared_c_time_q = multiprocessing.Value('d',0)
shared_i_time_q = multiprocessing.Value('d',0)
shared_c_turn = multiprocessing.Value('d',0)
shared_i_turn = multiprocessing.Value('d',0)


class PCB:
    c_turn =0
    i_turn =0

    pid =0
    cpu_burst =0
    io_burst =0

    c_time_q =0
    i_time_q =0

    require_mem =0



def play():
    if total_time.value == 500:
        exit(0)
    signal.signal(signal.SIGUSR1,child)
    while True:
        time.sleep(1)


def child(a,b):
    if shared_c_turn.value == 1:
        shared_cpu_burst.value-=1
        shared_c_time_q.value-=1
        os.kill(os.getppid(),signal.SIGUSR2)
        
    elif shared_i_turn.value ==1:
        shared_io_burst.value-=1
        shared_i_time_q.value-=1
        os.kill(os.getppid(),signal.SIGUSR2)
        

def arrange(a,b):
    if shared_c_turn.value == 1:
        ready_q[0].cpu_burst = shared_cpu_burst.value
        ready_q[0].c_time_q = shared_c_time_q.value
        shared_c_turn.value = 0
        ready_q[0].c_turn = shared_c_turn.value
        
        
    elif shared_i_turn.value == 1:
        wait_q[0].io_burst = shared_io_burst.value
        wait_q[0].i_time_q  = shared_i_time_q.value
        shared_i_turn.value = 0
        wait_q[0].i_turn = shared_i_turn.value
        for x in range(len(acc_mem_ch)):
                if acc_mem_ch[x][0] == ready_q[0].pid:
                    for i in range(acc_mem_ch[x][1],acc_mem_ch[x][2]+1):
                        P_M[i] = 0
                    acc_mem_ch.pop(x)
                break

    if ready_q[0].cpu_burst == 0:
        tmp = ready_q.pop(0)
        tmp.c_time_q = 3
        wait_q.append(tmp)
            
    elif ready_q[0].c_time_q == 0:
        tmp = ready_q.pop(0)
        tmp.c_time_q=3
        ready_q.append(tmp)

    if wait_q!=[]:
        if wait_q[0].io_burst ==0 :
            tmp = wait_q.pop(0)
            tmp.i_time_q=3
            tmp.cpu_burst = random.randint(8,15)
            tmp.io_burst = random.randint(3,15)
            ready_q.append(tmp)

        elif wait_q[0].i_time_q ==0:
            tmp = wait_q.pop(0)
            tmp.i_time_q=3
            wait_q.append(tmp)

    end.value = 1
        

def parent():
    signal.signal(signal.SIGUSR2,arrange)
    display()
    tr =0
    if end.value == 0 and ready_q[0].c_turn == 0:
        ready_q[0].c_turn = 1
        shared_cpu_burst.value = ready_q[0].cpu_burst
        shared_c_time_q.value = ready_q[0].c_time_q
        shared_c_turn.value = ready_q[0].c_turn
        mem_acc()
        os.kill(ready_q[0].pid,signal.SIGUSR1)
        while end.value == 0:
            tr =0
        end.value =0
    
    if wait_q != [] and wait_q[0].i_turn ==0:
        wait_q[0].i_turn =1
        shared_io_burst.value = wait_q[0].io_burst
        shared_i_time_q.value = wait_q[0].i_time_q
        shared_i_turn.value = wait_q[0].i_turn
        os.kill(wait_q[0].pid,signal.SIGUSR1)
        while end.value == 0:
            tr =0
        end.value =0
    
    total_time.value+=1

def mem_acc():
    for x in range(len(acc_mem_ch)):
        if acc_mem_ch[x][0]== ready_q[0].pid:
            return
    load =0
    req_mem = ready_q[0].require_mem
    while load != 1:
        for x in range(1024):
            if x+ready_q[0].require_mem <=1024:
                if P_M[x]==0 and P_M[x+ready_q[0].require_mem]==0:
                    for i in range(x,x+ready_q[0].require_mem+1):
                        P_M[i] = 1
                    tmp = []
                    tmp.append(ready_q[0].pid)
                    tmp.append(x)
                    tmp.append(x+ready_q[0].require_mem)
                    acc_mem_ch.append(tmp)
                    load = 1
                    return
        for x in range(acc_mem_ch[0][1],acc_mem_ch[0][2]+1):
            P_M[x] = 0
        acc_mem_ch.pop(0)
        
    


def ready_init(pid):
    new_pcb = PCB()
    new_pcb.pid = pid
    new_pcb.cpu_burst = random.randint(4,8)
    new_pcb.io_burst = random.randint(4,8)
    new_pcb.c_time_q = 3
    new_pcb.i_time_q = 3
    new_pcb.require_mem = random.randint(64,256)
    ready_q.append(new_pcb)

def display():
    print('******************load_mem****************************')
    print(acc_mem_ch)
    print('******************ready_q****************************')
    for x in range(len(ready_q)):
        print('id : ',ready_q[x].pid,'cpu :',ready_q[x].cpu_burst ,'io : ',ready_q[x].io_burst,'time_q : ',ready_q[x].c_time_q)
    print('******************wait_q****************************')
    if wait_q != []:
        for x in range(len(wait_q)):
            print('id : ',wait_q[x].pid,'cpu :',wait_q[x].cpu_burst,'io :',wait_q[x].io_burst,'time_q : ',wait_q[x].i_time_q)
    print('\n\n')

def mem_init(P_M):
    #assumption : mem == 1GB
    for x in range(1025):
        P_M.append(0)


if __name__=='__main__':
    ready_q = []
    wait_q = []
    
    acc_mem_ch = []

    P_M = []
    mem_init(P_M)

    for x in range(10):
        pid = os.fork()
        if pid == 0:
            play()
        else:
            ready_init(pid)
    
    while total_time.value != 500:
        parent()
        time.sleep(0.3)
        

    exit(0)

