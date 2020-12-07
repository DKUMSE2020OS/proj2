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
    vm_list = []
    pm_list = []
    c_turn =0
    i_turn =0
    pid =0
    cpu_burst =0
    io_burst =0
    c_time_q =0
    i_time_q =0
    


def play():
    if total_time.value == 500:
        exit(0)
        
    signal.signal(signal.SIGUSR1,process)
    while True:
        time.sleep(1)


def process(a,b):
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
            tmp.io_burst = random.randint(3,8)
            ready_q.append(tmp)
            for x in range(len(p_m)):
                if p_m[x] == tmp.pid:
                    p_m[x] = 0

        elif wait_q[0].i_time_q ==0:
            tmp = wait_q.pop(0)
            tmp.i_time_q=3
            wait_q.append(tmp)

    end.value = 1
        
def mem_cal(new_pcb,vm_tmp):

    ind1 = new_pcb.vm_list[vm_tmp][0]
    ind2 = new_pcb.vm_list[vm_tmp][1]
    off_s = new_pcb.vm_list[vm_tmp][2]

    tmp = P_T_1[ind1]
    tmp2 = P_T_2[tmp][ind2]

    w_st = calculate_address(tmp,tmp2,off_s)
    return int(w_st,2)



def MMU():
    signal.signal(signal.SIGUSR2,arrange)
    display()
    tr =0
    if end.value == 0 and ready_q[0].c_turn == 0:
        ready_q[0].c_turn = 1
        shared_cpu_burst.value = ready_q[0].cpu_burst
        shared_c_time_q.value = ready_q[0].c_time_q
        shared_c_turn.value = ready_q[0].c_turn

        pm_set(1)

        os.kill(ready_q[0].pid,signal.SIGUSR1)
        while end.value == 0:
            tr =0
        end.value =0
    
    if wait_q != [] and wait_q[0].i_turn ==0:
        wait_q[0].i_turn =1
        shared_io_burst.value = wait_q[0].io_burst
        shared_i_time_q.value = wait_q[0].i_time_q
        shared_i_turn.value = wait_q[0].i_turn

        pm_set(2)

        os.kill(wait_q[0].pid,signal.SIGUSR1)
        while end.value == 0:
            tr =0
        end.value =0
    
    total_time.value+=1

def pm_set(param):

    if param == 1:
        mem_li = []
    
        if TLB != []:
            for c_tmp in range(len(TLB)):
                if TLB[c_tmp][0] == ready_q[0].pid:
                    mem_li == TLB[c_tmp][1]
                    break
                else:
                    mem_li = ready_q[0].pm_list

        already_load=0
        hdd_hit=0
        new_fr=0
        hdd_load =0
        for x in range(len(mem_li)):
            if p_m[mem_li[x]] == ready_q[0].pid:
                already_load+=1
                continue
            
            tmp = []
            tmp.append(ready_q[0].pid)
            tmp.append(mem_li[x])
            if tmp in hdd:
                p_m[tmp[1]] = tmp[0]
                hdd.remove(tmp)
                hdd_hit+=1
                continue
            
            if p_m[mem_li[x]] == 0:
                p_m[mem_li[x]] = ready_q[0].pid
                new_fr+=1
            elif p_m[mem_li[x]] != 0 or p_m[mem_li[x]] != ready_q[0].pid:
                tmp = []
                tmp.append(p_m[mem_li[x]])
                tmp.append(mem_li[x])
                hdd.append(tmp)
                hdd_load+=1
                p_m[mem_li[x]] = 0
                p_m[mem_li[x]] = ready_q[0].pid
        
        print('pid : ',ready_q[0].pid ,'load : ',already_load,'hdd_load: ',hdd_hit ,'new_frame: ',new_fr,'hdd_swap',hdd_load,'/ total frame : ',len(mem_li))
    

    elif param == 2:
        mem_li = []
    
        if TLB != []:
            for c_tmp in range(len(TLB)):
                if TLB[c_tmp][0] == wait_q[0].pid:
                    mem_li == TLB[c_tmp][1]
                    break
                else:
                    mem_li = wait_q[0].pm_list
        already_load=0
        hdd_hit=0
        new_fr=0
        hdd_load=0
        for x in range(len(mem_li)):
            if p_m[mem_li[x]] == wait_q[0].pid:
                already_load+=1
                continue
            
            tmp = []
            tmp.append(wait_q[0].pid)
            tmp.append(mem_li[x])
            if tmp in hdd:
                p_m[tmp[1]] = tmp[0]
                hdd.remove(tmp)
                hdd_hit+=1
                continue
            
            if p_m[mem_li[x]] == 0:
                p_m[mem_li[x]] = wait_q[0].pid
                new_fr+=1
            elif p_m[mem_li[x]] != 0 or p_m[mem_li[x]] != wait_q[0].pid:
                tmp = []
                tmp.append(p_m[mem_li[x]])
                tmp.append(mem_li[x])
                hdd.append(tmp)
                p_m[mem_li[x]] = 0
                p_m[mem_li[x]] = wait_q[0].pid
        
        print('pid : ',ready_q[0].pid ,'load : ',already_load,'hdd_load: ',hdd_hit ,'new_frame: ',new_fr,'hdd_swap',hdd_load,'/',len(wait_q[0].pm_list))
    
    c_tmp_li = []
    c_tmp_li.append(ready_q[0].pid)
    c_tmp_li.append(mem_li)

    if len(TLB)<=5:
        TLB.append(c_tmp_li)
    else:
        TLB.pop(0)
        TLB.append(c_tmp_li)
    
   

def process_set_init_mem(pid):
    new_pcb = PCB()
    new_pcb.vm_list=[]
    new_pcb.pm_list=[]
    new_pcb.pid = pid
    new_pcb.cpu_burst = random.randint(4,8)
    new_pcb.io_burst = random.randint(2,5)
    new_pcb.c_time_q = 3
    new_pcb.i_time_q = 3
    vm_init(new_pcb)
    ready_q.append(new_pcb)


def vm_init(new_pcb):
    total_F = random.randint(512,1024)
    for vm_tmp in range(total_F):
        tmp_li = []
        tmp_li.append(random.randint(0,15))
        tmp_li.append(random.randint(0,15))
        tmp_li.append(random.randint(0,31))
        new_pcb.vm_list.append(tmp_li)
        tmp_t = mem_cal(new_pcb,vm_tmp)
        new_pcb.pm_list.append(tmp_t)
            
def calculate_address(tmp,tmp2,off_s):
    tmp = str(bin(tmp))[2:]
    tmp2 = str(bin(tmp2))[2:]
    off_s = str(bin(off_s))[2:]

    return tmp+tmp2+off_s

def pm_init():
    for x in range(8192):
        p_m.append(0)


def PT_init():

    for pt_tmp in range(16):
        P_T_1.append(random.randint(0,15))

    for pt_tmp in range(16):
        tmp = []
        for pt2_tmp in range(16):
            tmp.append(random.randint(0,15))
        P_T_2.append(tmp) 



def display():
    print('******************mem_usage****************************')
    n_cnt=0
    for x in p_m:
        if x != 0:
            n_cnt+=1
    print('using swap daemon : make free space forcely ')
    print('mem_usage : ', n_cnt,'/',len(p_m)/4)
            
    print('*************now_load_program_list***********************')
    tmp_p = set(p_m)
    print(tmp_p)
    print('******************swap_space_fragment_num****************************')
    print(len(hdd))
    print('******************ready_q****************************')
    samp=[]
    for x in range(len(ready_q)):
        samp.append(ready_q[x].pid)
    print(samp)
    print('******************wait_q****************************')
    if wait_q != []:
        samp=[]
        for x in range(len(wait_q)):
            samp.append(wait_q[x].pid)
        print(samp)
    print('\n\n\n')    


if __name__=='__main__':
    ready_q = []
    wait_q = []
    P_T_1 = []
    P_T_2 = []
    p_m = []
    TLB = []
    hdd=[]

    pm_init()
    PT_init()

    print('################# 32154231 jeong younghwan ##################')
    print('\n\nimplement list :base&bound, two level paging, swaping , TLB , demand zeroing , swap daemon\n\n')
    print('today date : 2020 / 11 / 30 \n')
    print('Thank you')

    for x in range(10):
        pid = os.fork()
        if pid == 0:
            play()
        else:
            process_set_init_mem(pid)
    
    while total_time.value != 500:
        MMU()
        time.sleep(0.7)
        

    exit(0)
