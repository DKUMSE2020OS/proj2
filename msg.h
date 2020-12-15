struct msgbuf {
        long mtype;

        // pid will sleep for io_time
        int pid;
        int io_burst;
        int level;
        int cpu_burst;
        int io_work_exec;
	int access_request[10];
};

