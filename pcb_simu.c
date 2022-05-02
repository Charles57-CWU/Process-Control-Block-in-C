#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define PROCESS_INFO_SIZE 38

static int proc_left_to_exec;
static int all_proc_active;


//PCB structure
struct PCB {
    char name[17];
    int id;
    char activity;
    int burstTime;
    int baseRegister;
    long limitRegister;
    char priority;
};

//pass arg for schedule routine
struct routines{
    int arr_sizes;
    int process_to_start;
    //int process_to_end;
    int sched_type;
    int processors;
    struct PCB *pass_to_thread;
};

void* new_core_thread(void* args);
void* updated_core_thread(void* args);

//readPCB and printPCB functions obtained from github https://github.com/ElizabethBrooks/ProcessScheduling_PCB/blob/master/processScheduler_PCB.c
//everything else is my own work
void readPCB(struct PCB *get_PCB, int process_count, char* filename) {
    FILE *read_ptr;
    read_ptr = fopen(filename, "rb");
    char procBuffer[PROCESS_INFO_SIZE];

    //get processes
    for (int counter = 0; counter < process_count; counter++) {
        fread(procBuffer, PROCESS_INFO_SIZE, 1, read_ptr);
        memcpy(&get_PCB[counter].name, &procBuffer[0], 16);
        get_PCB[counter].name[16] = '\0';
        memcpy(&get_PCB[counter].id, &procBuffer[16], sizeof(int));
        memcpy(&get_PCB[counter].activity, &procBuffer[20], sizeof(char));
        memcpy(&get_PCB[counter].burstTime, &procBuffer[21], sizeof(int));
        memcpy(&get_PCB[counter].baseRegister, &procBuffer[25], sizeof(int));
        memcpy(&get_PCB[counter].limitRegister, &procBuffer[29], sizeof(long));
        memcpy(&get_PCB[counter].priority, &procBuffer[37], sizeof(char));
    }

    fclose(read_ptr);
}

//print PCB
void printPCB(struct PCB *print_PCB, int process_count) {
    int counter = 0;
    printf("Name         ID       Status        Burst      BaseReg       LimReg     Priority\n");
    for (counter = 0; counter < process_count; counter++) {
        printf("%2s %12d %12d %12d %12d %12lu %12d\n", print_PCB[counter].name, print_PCB[counter].id,
               print_PCB[counter].activity, print_PCB[counter].burstTime, print_PCB[counter].baseRegister,
               print_PCB[counter].limitRegister,
               print_PCB[counter].priority);
    }
    printf("Name         ID       Status        Burst      BaseReg       LimReg     Priority\n");

}


int* roundRobinqueue(struct PCB *rq_PCB, int start, int end, int *arr) {
    int cnt = 0;
    int chk = 0;
    while(cnt < end) {
        if (rq_PCB[chk+start].activity == 1) {
            arr[cnt] = rq_PCB[chk + start].id;
            cnt++;
        }
        chk++;
    }
    return arr;
}

//round robin
void roundRobin(struct PCB *rr_PCB, int start, int end, int *arr, int core) {
    int still_process = end;
    while(still_process > 0) {
        for (int counter = 0; counter < end; counter++) {
            if (rr_PCB[arr[counter]].burstTime >= 0 ) {
                printf("Executing process ID %d using Round Robin on core %d. Time left is %d ms.\n", rr_PCB[arr[counter]].id, core,
                       rr_PCB[arr[counter]].burstTime * 100);
                rr_PCB[arr[counter]].burstTime = rr_PCB[arr[counter]].burstTime - 3;
                usleep(300* 1000);
                if (rr_PCB[arr[counter]].burstTime <= 0) {
                    proc_left_to_exec--;
                    still_process--;
                    printf("Process ID %d completed on core %d. Processes left: %d.\n", rr_PCB[arr[counter]].id, core, proc_left_to_exec);
                    rr_PCB[arr[counter]].activity = 0;
                    rr_PCB[arr[counter]].burstTime = -1;
                }
            }
            if (still_process <= 0 || all_proc_active == 0) {
                if (still_process <= 0) {
                    all_proc_active = 0;
                    printf("Core %d is empty.\n", core);
                }
                return;
            }
        }
    }
}

//sort queue based on priority
int* sortByPriority(struct PCB *sp_PCB, int start, int end, int *arr) {

    int j = 0;
    int low_value;
    int old_id;
    int cnt = 0;
    int chk = 0;
    while(cnt < end) {
        if (sp_PCB[chk+start].activity == 1) {
            arr[cnt] = sp_PCB[chk + start].id;
            cnt++;
        }
        chk++;
    }

    for(int counter=0; counter < end; counter++) {
            low_value = sp_PCB[arr[counter]].priority;
            for(int k = j; k < end; k++) {
                if(sp_PCB[arr[k]].priority < low_value) {
                    low_value = sp_PCB[arr[k]].priority;
                    old_id = arr[j];
                    arr[j] = sp_PCB[arr[k]].id;
                    arr[k] = old_id;
                }
            }
            j++;
    }
    return arr;
}

//sort queue based on time
int* sortByTime(struct PCB *st_PCB, int start, int end, int *arr) {
    int j = 0;
    int low_value;
    int old_id;
    int cnt = 0;
    int chk = 0;
    while(cnt < end) {
        if (st_PCB[chk+start].activity == 1) {
            arr[cnt] = st_PCB[chk + start].id;
            cnt++;
        }
        chk++;
    }

    for(int counter=0; counter < end; counter++) {
        low_value = st_PCB[arr[counter]].burstTime;
        for(int k = j; k < end; k++) {
            if(st_PCB[arr[k]].burstTime < low_value) {
                low_value = st_PCB[arr[k]].burstTime;
                old_id = arr[j];
                arr[j] = st_PCB[arr[k]].id;
                arr[k] = old_id;
            }
        }
        j++;
    }
    return arr;
}

//SJF algorithm
void shortestJobFirst(struct PCB *short_PCB, int process_count, int *arr, int core) {

    int still_process = process_count;
    for(int counter=0; counter < process_count; counter++) {
        printf("Executing process ID %d using shortest job first on core %d. Time required is %d ms.\n", short_PCB[arr[counter]].id, core, short_PCB[arr[counter]].burstTime*100);

        usleep(short_PCB[arr[counter]].burstTime * 100 * 1000);
        proc_left_to_exec--;
        still_process --;
        printf("Process ID %d completed on core %d. Processes left: %d\n", short_PCB[arr[counter]].id, core, proc_left_to_exec);
        short_PCB[arr[counter]].burstTime = -1;
        short_PCB[arr[counter]].activity = 0;
        if (still_process <= 0 || all_proc_active == 0) {
            if (still_process <= 0) {
                all_proc_active = 0;
                printf("Core %d is empty.\n", core);
            }
            return;
        }
    }
}

//priority algorithm
void priority(struct PCB *prior_PCB, int process_count, int *arr, int core) {

    int time_tracker = 0;
    int still_process = process_count;
    for(int counter = 0; counter < process_count; counter++) {
        printf("Executing process ID %d using priority scheduling on core %d. Priority is %d. Time required is %d ms.\n", prior_PCB[arr[counter]].id, core, prior_PCB[arr[counter]].priority, prior_PCB[arr[counter]].burstTime*100);


        usleep(prior_PCB[arr[counter]].burstTime * 100 * 1000);
        time_tracker = time_tracker + prior_PCB[arr[counter]].burstTime * 100;

        //age processes after 20secs
        if (time_tracker > 20000) {
            printf("Begin aging on core %d...\n", core);
            time_tracker = 0;
            for(int new_cnt = 0; new_cnt < process_count; new_cnt++) {
                if (prior_PCB[arr[new_cnt]].priority > 0) {
                    prior_PCB[arr[new_cnt]].priority--;
                }
            }
            printf("Done aging on core %d...\n", core);
        }

        proc_left_to_exec--;
        still_process--;
        printf("Process ID %d completed on core %d. Processes left: %d\n", prior_PCB[arr[counter]].id, core, proc_left_to_exec);
        prior_PCB[arr[counter]].burstTime = -1;
        prior_PCB[arr[counter]].activity = 0;
        if (still_process <= 0 || all_proc_active == 0) {
            if (still_process <= 0) {
                all_proc_active = 0;
                printf("Core %d is empty.\n", core);
            }
            return;
        }
    }
}

//get binary file size and number of processes
int numberOfProcess(char *filename) {
    FILE *get_size_ptr;
    get_size_ptr = fopen(filename, "rb");
    if (get_size_ptr == NULL) {
    	fprintf(stderr, "Can't open file.\n");
    	exit(1);
    }

    long sz;
    int number_process;

    fseek(get_size_ptr, 0, SEEK_END);
    sz = ftell(get_size_ptr);
    printf("File size is %ld bytes.\n", sz);
    if (sz % PROCESS_INFO_SIZE != 0) {
    	printf("File size is %ld bytes, but should be a multiple of 38 bytes.\n");
	exit(1);
    }
    number_process = sz/PROCESS_INFO_SIZE;

    printf("Found %d processes.\n", number_process);
    fclose(get_size_ptr);

    return number_process;
}

//thread each "core"
void* new_core_thread(void* args) {

    struct routines *data = args;
    printf("\n%d processes given to core %d.\n\n", data -> arr_sizes, data->processors);
    printf("index start %d\n", data->process_to_start);

    if (data -> sched_type == 1) {
        int arr[data -> arr_sizes];
        int* zarr = sortByPriority(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, arr);
        priority(data -> pass_to_thread, data -> arr_sizes, zarr, data->processors);
    } else if (data -> sched_type == 2) {
        int carr[data -> arr_sizes];
        int* czarr = sortByTime(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, carr);
        shortestJobFirst(data -> pass_to_thread, data -> arr_sizes, czarr, data->processors);
    } else {
        int farr[data -> arr_sizes];
        int *fzarr = roundRobinqueue(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, farr);
        roundRobin(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, fzarr, data->processors);
    }

    return NULL;
}

//time 0
void initial_split_between_processors(struct PCB *pass_PCB, struct routines pass_routines, int num_cores, int process_count, char *args[]) {
    int j = 2;
    int k = 1;

    int add;
    int index_start = 0;
    int index_end;
    pass_routines.pass_to_thread = pass_PCB;

    pthread_t coreThreads[num_cores];
    for (int i = 0; i < num_cores; i++) {
        add = process_count * atof(args[j]);
        index_end = index_start + add - 1;
        pass_routines.arr_sizes = add;
        pass_routines.process_to_start = index_start;
        //pass_routines.process_to_end = index_end;
        pass_routines.sched_type = atoi(args[k]);
        pass_routines.processors = i;
        pthread_create(&coreThreads[i], NULL, new_core_thread, (void*)&pass_routines);
        sleep(1);
        index_start = index_end + 1;
        j = j + 2;
        k = k + 2;
    }

    for (int i = 0; i < num_cores; i++) {
        pthread_join(coreThreads[i], NULL);
    }

}

//thread each core after load balance
void* updated_core_thread(void* args) {

    struct routines *data = args;
    printf("\n%d processes given to core %d.\n\n", data -> arr_sizes, data->processors);

    if (data -> sched_type == 1) {
        int arr[data -> arr_sizes];
        int* zarr = sortByPriority(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, arr);
        priority(data -> pass_to_thread, data -> arr_sizes, zarr, data->processors);
    } else if (data -> sched_type == 2) {
        int carr[data -> arr_sizes];
        int* czarr = sortByTime(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, carr);
        shortestJobFirst(data -> pass_to_thread, data -> arr_sizes, czarr, data->processors);
    } else {
        int farr[data -> arr_sizes];
        int *fzarr = roundRobinqueue(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, farr);
        roundRobin(data -> pass_to_thread, data -> process_to_start, data -> arr_sizes, fzarr, data->processors);
    }

    return NULL;
}

//load balancing
void balanced_split_between_processors(struct PCB *updated_PCB, struct routines update_routines, int num_cores, int process_count, char *args[]) {
    all_proc_active = 1;
    int k = 1;
    int index_start = 0;
    int extra_process = 0;
    int process_per_core = 0;
    int add = 0;

    if (proc_left_to_exec <= 0) {
    	return; 
    }

    process_per_core = proc_left_to_exec / num_cores;
    extra_process = proc_left_to_exec % num_cores;

    update_routines.pass_to_thread = updated_PCB;

    int thread_to_join = 0;
    pthread_t updatedCoreThreads[num_cores];
    for (int j = 0; j < num_cores; j++) {

        add = process_per_core;

        if (process_per_core == 0 && extra_process == 0) {
            break; //no more processes to give
        }
	thread_to_join++;

        if (extra_process > 0) {
            add = process_per_core + 1;
            extra_process--;
        }

        //get index of first process for new core
        int cnt = 0;
        int chk = 0;
        while(cnt < add) {
            if (updated_PCB[chk+index_start].activity == 1) {
                cnt++;
                if (cnt ==  1) {
                    update_routines.process_to_start = chk + index_start;
                }
            }
            chk++;
        }

        index_start = chk + index_start;

        update_routines.sched_type = atoi(args[k]);
        update_routines.arr_sizes = add;
        update_routines.processors = j;
        k = k + 2;
        pthread_create(&updatedCoreThreads[j], NULL, updated_core_thread, (void*)&update_routines);
        sleep(2);
    }

    for (int i = 0; i < thread_to_join; i++) {
        pthread_join(updatedCoreThreads[i], NULL);
    }

}

int get_total_memory(struct PCB *mem_PCB, int process_count) {
    int mem = 0;
    for (int counter = 0; counter < process_count; counter++) {
        mem = mem + (mem_PCB[counter].limitRegister - mem_PCB[counter].baseRegister);
    }

    return mem;
}

int main(int argc, char *argv[]) {
    printf("\nPriority Scheduling = 1, Shortest Job First = 2, Round Robin = 3.\n");
    printf("\nChecking Arguments...\n");

    for(int i = 1; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    if (argc % 2 != 0) {
        printf("\nError: Program found an even amount of arguments but there should be an odd amount.\n");
        printf("For each processor please provide scheduling algorithm and load percentage. (2 arguments each)\n");
        printf("In addition please provide the name of the binary file with the PCB information as the final argument.\n");
        printf("Program Exiting\n");
        return 1;
    }

    //get process load, scheduling algorithm for each core
    float total_process_load = 0;
    int num_cores = 0;
    for (int i = 1; i < argc - 1; i++) {
        //sched. arguments
        if (i % 2 != 0) {
            if (atoi(argv[i]) < 1 || atoi(argv[i]) > 3) {
                printf("\nError: Invalid scheduling algorithm detected.\n");
                printf("Priority Scheduling = 1, Shortest Job First = 2, Round Robin = 3.\n");
                printf("Program Exiting\n");
                return 1;
            }
            num_cores++;
        }

        //load arguments
        if (i % 2 == 0) {
            if (atof(argv[i]) < 0 || atof(argv[i]) > 1) {
                printf("\nError: Invalid process load detected. Load value must be within 0 to 1.\n");
                printf("Program Exiting\n");
                return 1;
            }
            total_process_load = total_process_load + atof(argv[i]);
        }

    }

    if (total_process_load != 1.0) {
        printf("\nError: Invalid process loads. Process load must total 1 between all processors.\n");
        printf("Program Exiting\n");
        return 1;
    }

    printf("\nFile name is %s.\n", argv[argc-1]);
    printf("%d processors were inputted by command line.\n", num_cores);
    int j = 0;
    for (int i = 0; i <= num_cores+1;) {
        printf("Core %d: Schedule Algorithm: %s, Load Percentage: %s\n", j, argv[i+1], argv[i+2]);
        j++;
        i = i + 2;
    }

    //get binary file size and number of processes
    printf("\nFinding number of processes in %s...\n", argv[argc-1]);
    int num_proc = numberOfProcess(argv[argc-1]);
    proc_left_to_exec = num_proc;
    printf("\nBegin CPU scheduling simulation in 5 seconds...\n");
    sleep(5);



    struct PCB pass_PCB[num_proc];
    readPCB(pass_PCB, num_proc, argv[argc-1]);
    printPCB(pass_PCB, num_proc);

    //total memory
    int total_mem = 0;
    total_mem = get_total_memory(pass_PCB, num_proc);

    struct routines core_routines;

    printf("\n");
    all_proc_active = 1;
    initial_split_between_processors(pass_PCB, core_routines, num_cores, num_proc, argv);
    while (proc_left_to_exec > 0) {
        if (all_proc_active == 0) {
            printf("Balancing load...\n");

            balanced_split_between_processors(pass_PCB, core_routines, num_cores, num_proc, argv);
            
        }

    }

    printPCB(pass_PCB, num_proc);

    printf("\n%d Processes executed on %d cores. Total memory of all processes is %d bytes.\n", num_proc, num_cores, total_mem);

    return 0;
}
