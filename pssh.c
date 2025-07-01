#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/wait.h>  
#include <readline/readline.h>
#include <fcntl.h>
#include "builtin.h"
#include "parse.h"
#include <signal.h>
#include "job.h"
#define MAX_JOBS 100

/*******************************************
 * Set to 1 to view the command line parse *
 * Set to 0 before submitting!             *
 *******************************************/
#define DEBUG_PARSE 0

void bring_job_to_fg(Job* job_table,int job_num){
    Job *job_ptr = &job_table[job_num];

    if (job_ptr->name == NULL) {
        fprintf(stderr, "pssh: no such job [%d]\n", job_num);
        return;
    }
    pid_t shell_pgid = getpgrp();
    signal(SIGTTOU, SIG_IGN);
    if (tcsetpgrp(STDIN_FILENO, job_ptr->pgid) < 0) {
        perror("tcsetpgrp");
        exit(EXIT_FAILURE);
    }
    if (job_table[job_num].status == STOPPED)
        kill(-job_ptr->pgid, SIGCONT);
    job_ptr->status = FG;
    int status;
    for (int i = 0; i < job_ptr->npids; i++) {
        waitpid(job_ptr->pids[i], &status, WUNTRACED);
        if (WIFSTOPPED(status)){
            job_ptr->status = STOPPED;
            printf("\n[%d] suspended %s\n", job_num, job_ptr->name);
             // Return control to shell
            break;
        }
    }
    
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0) {
        perror("tcsetpgrp (restore shell)");
    }
    signal(SIGTTOU, SIG_DFL);
    int still_running = 0;
    for (int i = 0; i < job_ptr->npids; i++) {
        if (kill(job_ptr->pids[i], 0) == 0) {
            still_running = 1;
            break;
        }
    }

    if (!still_running) {
        remove_jobs(job_table, job_num);
    }
    
}

void continue_job_in_bg(Job* job_table, int job_num){
    Job *job_ptr = &job_table[job_num];

    if (job_ptr->name == NULL) {
        fprintf(stderr, "pssh: no such job [%d]\n", job_num);
        return;
    }
    kill(-job_ptr->pgid, SIGCONT);
    job_ptr->status = BG;

    

}



void sigchild_handler(int sig) {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int job_number = -1;

        // Find job_number matching the pid
        for (int i = 0; i < MAX_JOBS; i++) {
            if (job_table[i].name != NULL) {
                for (int j = 0; j < job_table[i].npids; j++) {
                    if (job_table[i].pids[j] == pid) {
                        job_number = i;
                        if (WIFEXITED(status) || WIFSIGNALED(status)) {
                            job_table[i].pids[j] = -1;
                        }
                        break;
                    }
                }
            }
            if (job_number != -1) break;
        }

        if (job_number == -1) {
            fprintf(stderr, "pssh: unknown child %d\n", pid);
            continue;
        }

        // Handle various cases
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Check if all PIDs are -1 now
            int all_done = 1;
            for (int k = 0; k < job_table[job_number].npids; k++) {
                if (job_table[job_number].pids[k] != -1) {
                    all_done = 0;
                    break;
                }
            }

            if (all_done) {
                if (job_table[job_number].status == BG || job_table[job_number].status == STOPPED) {
                    printf("\n[%d] + done %s\n", job_number, job_table[job_number].name);
                }
                job_table[job_number].status = TERM;
                remove_jobs(job_table, job_number);
            }

        } else if (WIFSTOPPED(status)) {
            job_table[job_number].status = STOPPED;
            printf("\n[%d] + suspended %s\n", job_number, job_table[job_number].name);

        } else if (WIFCONTINUED(status)) {
            job_table[job_number].status = BG;
            printf("\n[%d] + continued %s\n", job_number, job_table[job_number].name);
        }
    }
}

    
void print_banner()
{
    printf ("                    ________   \n");
    printf ("_________________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* **returns** a string used to build the prompt
 * (DO NOT JUST printf() IN HERE!)
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
static char *build_prompt()
{   static char cwd[PATH_MAX + 2];
    getcwd( cwd, PATH_MAX);
    strcat(cwd, "$ ");
    return  cwd;
}


/* return true if command is found, either:
 *   - a valid fully qualified path was supplied to an existing file
 *   - the executable file was found in the system's PATH
 * false is returned otherwise */
static int command_found(const char *cmd)
{
    char *dir;
    char *tmp;
    char *PATH;
    char *state;
    char probe[PATH_MAX];

    int ret = 0;

    if (access(cmd, X_OK) == 0)
        return 1;

    PATH = strdup(getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r(tmp, ":", &state);
        if (!dir)
            break;

        strncpy(probe, dir, PATH_MAX-1);
        strncat(probe, "/", PATH_MAX-1);
        strncat(probe, cmd, PATH_MAX-1);

        if (access(probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free(PATH);
    return ret;
}


/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks(Parse *P,char *cmdline) {
    if (P->ntasks == 1 && is_builtin(P->tasks[0].cmd) && (P->infile == NULL && P->outfile == NULL)) {
        builtin_execute(P->tasks[0]);
        return;
    }

    unsigned int j;
    pid_t pid[P->ntasks];
    int fd[P->ntasks - 1][2];

    // Create pipes for communication
    for (int i = 0; i < P->ntasks - 1; i++) {
        if (pipe(fd[i]) == -1) {
            fprintf(stderr, "Failed to create pipes\n");
            exit(EXIT_FAILURE);
        }
    }

    // Fork child processes for each task
    for (j = 0; j < P->ntasks; j++) {
        pid[j] = fork();
        if (pid[j] < 0) {
            fprintf(stderr, "error -- failed to fork()");
            exit(EXIT_FAILURE);
        }
        setpgid(pid[j], pid[0]);
        if (pid[j] == 0) { // Child process
            // Input redirection: From previous pipe or infile
            if (j > 0) {
                if (dup2(fd[j - 1][0], STDIN_FILENO) < 0) {
                    fprintf(stderr, "dup2() failed!\n");
                    exit(EXIT_FAILURE);
                }
            } else if (P->infile) { // Redirect from input file
                int fd_input = open(P->infile, O_RDONLY);
                if (fd_input == -1) {
                    fprintf(stderr, "open() input file failed\n");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_input, STDIN_FILENO) < 0) {
                    fprintf(stderr, "dup2() failed!\n");
                    exit(EXIT_FAILURE);
                }
                close(fd_input);
            }

            // Output redirection: To next pipe or outfile
            if (j < P->ntasks - 1) {
                if (dup2(fd[j][1], STDOUT_FILENO) < 0) {
                    fprintf(stderr, "dup2() failed!\n");
                    exit(EXIT_FAILURE);
                }
            } else if (P->outfile) { // Redirect to output file
                int fd_output = open(P->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_output == -1) {
                    fprintf(stderr, "open() output file failed\n");
                    exit(EXIT_FAILURE);
                }
                if (dup2(fd_output, STDOUT_FILENO) < 0) {
                    fprintf(stderr, "dup2() failed!\n");
                    exit(EXIT_FAILURE);
                }
                close(fd_output);
            }

            for (int i = 0; i < P->ntasks - 1; i++) {
                if (i != j - 1) close(fd[i][0]); 
                if (i != j) close(fd[i][1]);   
            }

            // Execute the task
            if(is_builtin(P->tasks[j].cmd)){
                builtin_execute(P->tasks[j]);
                exit(EXIT_SUCCESS);
            }
            else if (command_found(P->tasks[j].cmd)) {
                
                execvp(P->tasks[j].cmd, P->tasks[j].argv);
                fprintf(stderr, "pssh: found but can't exec: %s\n", P->tasks[j].cmd);
                exit(EXIT_FAILURE);
            } else {
                fprintf(stderr, "pssh: command not found: %s\n", P->tasks[j].cmd);
                exit(EXIT_FAILURE);
            }
        }
    }

    
    for (int i = 0; i < P->ntasks - 1; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }
    if (!isatty(STDOUT_FILENO)) {
        printf("STDOUT_FILENO is not a tty.\n");
        exit(EXIT_FAILURE);
    }
    if (!P->background) {
        pid_t shell_pgid = getpgrp();
        signal(SIGTTOU, SIG_IGN);
        if (tcsetpgrp(STDIN_FILENO, pid[0]) < 0) {
            perror("tcsetpgrp");
        }

        // Wait for whole job process group
        int status;
        pid_t w;
        while ((w = waitpid(-pid[0], &status, WUNTRACED)) > 0) {
            if (WIFSTOPPED(status)) break;
            if (WIFEXITED(status) || WIFSIGNALED(status)) break;
        }

        if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0)
            perror("tcsetpgrp restore");
        signal(SIGTTOU, SIG_DFL);

        if (WIFSTOPPED(status)) {
            int job_num = helper(job_table);
            add_jobs(job_table, cmdline, pid, P->ntasks, pid[0], STOPPED);
            printf("[%d] + suspended %s\n", job_num, cmdline);
        }}  else {
        // Background job
        int job_number = helper( job_table);
        add_jobs(job_table, cmdline, pid, P->ntasks, pid[0], BG);
        printf("[%d] ", job_number);
        for (int i = 0; i < P->ntasks; i++) {
            printf("%d ", pid[i]);
        }
    }
}



int main(int argc, char **argv)
{
    char *cmdline;
    Parse *P;

    print_banner();
    signal(SIGCHLD, sigchild_handler);
    while (1) {
        /* do NOT replace readline() with scanf() or anything else! */
        cmdline = readline(build_prompt());
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit(EXIT_SUCCESS);
        char *orig_cmdline = strdup(cmdline);
        P = parse_cmdline(cmdline);
        if (!P)
            goto next;

        if (P->invalid_syntax) {
            printf("pssh: invalid syntax\n");
            goto next;
        }

#if DEBUG_PARSE
        parse_debug(P);
#endif
    if (P->ntasks == 1) {
        if (strcmp(P->tasks[0].cmd, "jobs") == 0) {
            print_jobs(job_table);
            goto next;
        }
        else if (strcmp(P->tasks[0].cmd, "fg") == 0){
            if (P->tasks[0].argv[1] == NULL||P->tasks[0].argv[1][0] != '%') {
                printf("Usage: fg %%<job number>\n");
                goto next;}
            else{
                char* token = strtok(P->tasks[0].argv[1], "%");
                int job_num = atoi(token);
                if (job_num > 100 || job_num < 0 || job_table[job_num].name == NULL) {
                    fprintf(stderr, "pssh: invalid job number: [%d]\n", job_num);
                    goto next;
                }
                else{
                    bring_job_to_fg(job_table,job_num);
                    goto next;
                }
            }

            
        }
        else if (strcmp(P->tasks[0].cmd, "bg") == 0){
            if (P->tasks[0].argv[1] == NULL|| P->tasks[0].argv[1][0] != '%') {
                printf("Usage: bg %%<job number>\n");
                goto next;
            }
            else {
                char* token = strtok(P->tasks[0].argv[1], "%");
                int job_num = atoi(token);
                if (job_num > 100 || job_num < 0 || job_table[job_num].name == NULL) {
                    fprintf(stderr, "pssh: invalid job number: [%d]\n", job_num);
                    goto next;
                }
                else{
                    
                    continue_job_in_bg(job_table,job_num);
                    goto next;
                }
            

            }
            
        } 
        else if (strcmp(P->tasks[0].cmd, "kill") == 0) {
            int sig = SIGTERM;
            int arg_start = 1;
        
            if (P->tasks[0].argv[1] == NULL) {
                printf("Usage: kill [-s <signal>] <pid> | %%<job> ...\n");
                goto next;
            }
        
            // Handle optional -s <signal> flag
            if (strcmp(P->tasks[0].argv[1], "-s") == 0) {
                if (P->tasks[0].argv[2] == NULL) {
                    printf("Usage: kill [-s <signal>] <pid> | %%<job> ...\n");
                    goto next;
                }
                sig = atoi(P->tasks[0].argv[2]);
                arg_start = 3;
            }
            
            // Loop through targets (job numbers or pids)
            for (int i = arg_start; P->tasks[0].argv[i] != NULL; i++) {
                char *target = P->tasks[0].argv[i];
        
                if (target[0] == '%') {
                    // Job number
                    int job_num = atoi(target + 1);
                    if (job_num < 0 || job_num >= MAX_JOBS || job_table[job_num].name == NULL) {
                        fprintf(stderr, "pssh: invalid job number: [%d]\n", job_num);
                        continue;}
                    kill(-job_table[job_num].pgid, sig);
                       
        
                } else {
                    // PID
                    pid_t pid = atoi(target);
                    if (kill(pid, 0) == -1) { // Check if valid PID
                        fprintf(stderr, "pssh: invalid pid: [%d]\n", pid);
                        continue;
                    }
        
                    kill(pid, sig);
                     
                }
            }
        
            goto next;
        }
        

    }
        execute_tasks(P,orig_cmdline);

    next:
        parse_destroy(&P);
        free(cmdline);
    }
}
