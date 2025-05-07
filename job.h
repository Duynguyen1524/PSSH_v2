// job.h
#ifndef JOB_H
#define JOB_H

#include <sys/types.h>

typedef enum { STOPPED, TERM, BG, FG } JobStatus;

typedef struct {
    char *name;
    pid_t *pids;
    int npids;
    pid_t pgid;
    JobStatus status;
} Job;

#define MAX_JOBS 100

extern Job job_table[MAX_JOBS];

int helper(Job* job_table);
int find_job_by_pid(pid_t pid);
void add_jobs(Job* job_table, char* name,        // Command string
    pid_t* pids,           // List of PIDs in this job
    unsigned int npids,   // Number of PIDs
    pid_t pgid,           // Process Group ID
    JobStatus status);
void remove_jobs(Job *job_table, int job_number);
void print_jobs(Job *job_table);
void update_job_status(Job *job_table, pid_t pgid, JobStatus new_status) ;

#endif // JOB_H