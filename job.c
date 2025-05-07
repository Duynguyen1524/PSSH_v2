// job.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "job.h"

Job job_table[MAX_JOBS];


int helper(Job* job_table){
    //find the min index job we can have
    int i = 0;
    for(i = 0;i<MAX_JOBS;i++){
        if(job_table[i].pids == NULL){
            return i;
        }
    }
    return -1;
}
void add_jobs(Job* job_table, char* name,        // Command string
    pid_t* pids,           // List of PIDs in this job
    unsigned int npids,   // Number of PIDs
    pid_t pgid,           // Process Group ID
    JobStatus status)  // Current job status)
    {   int index = helper(job_table);
        if (index == -1) {
            fprintf(stderr, "Table full!\n");
            return;
        }
        job_table[index].name = malloc(strlen(name)+1);
        strcpy(job_table[index].name , name);
        job_table[index].pids = malloc(sizeof(pid_t) * npids);
        memcpy(job_table[index].pids, pids, sizeof(pid_t) * npids);
        job_table[index].npids = npids;
        job_table[index].pgid = pgid;
        job_table[index].status = status;
    }
void remove_jobs(Job* job_table, int job_number){
    if (job_number >= 0 && job_number < MAX_JOBS && job_table[job_number].name != NULL){
        free(job_table[job_number].name);
        free(job_table[job_number].pids);

        job_table[job_number].name = NULL;
        job_table[job_number].pids = NULL;
        job_table[job_number].npids = 0;
        job_table[job_number].pgid =0;
        job_table[job_number].status = TERM;
        }
    }
void print_jobs(Job* job_table){
    char state[10]; 
    int i; 
    
    for ( i = 0; i < MAX_JOBS; i++){
        if (job_table[i].name != NULL) {
        if (job_table[i].status == STOPPED) {
           strcpy(state , "stopped");}
        else
            strcpy(state, "running");
        printf("[%d] + %s\t%s\n", i,state,job_table[i].name);
        }
    }
}
void update_job_status(Job *job_table, pid_t pgid, JobStatus new_status) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].name != NULL && job_table[i].pgid == pgid) {
            job_table[i].status = new_status;

            return;
        }
    }
    fprintf(stderr, "pssh: No job found with pgid %d\n", pgid);
}
