#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "builtin.h"
#include "parse.h"

static char *builtin[] = {
    "exit",   /* exits the shell */
    "which", 
    /* displays full path to command */
    NULL
};


int is_builtin(char *cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp(cmd, builtin[i]))
            return 1;
    }

    return 0;
}

void builtin_execute(Task T) {
    int i;
    if (strcmp(T.cmd, "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    else if (strcmp(T.cmd, "which") == 0) {
        if (T.argv[1] == NULL) {
            return;
        }

        for (i = 1; T.argv[i] != NULL; i++) {
            char *cmd = T.argv[i];

            
            if (is_builtin(cmd)) {
                fprintf(stdout,"%s: shell built-in command\n", cmd);
                continue;
            }
            
            char *path_env = getenv("PATH");
            if (!path_env) {
                fprintf(stderr, "which: PATH variable not found\n");
                continue;
            }

            char *PATH = strdup(path_env);
            char *dir = NULL, *state = NULL;
            char probe[PATH_MAX];
            int ret = 0;
            char* tmp;

            for (tmp=PATH; ; tmp=NULL) {
                dir = strtok_r(tmp, ":", &state);
                if (!dir)
                    break;
        
                strncpy(probe, dir, PATH_MAX-1);
                strncat(probe, "/", PATH_MAX-1);
                strncat(probe, cmd, PATH_MAX-1);
        
                if (access(probe, X_OK) == 0) {
                    printf("%s\n", probe);
                    ret = 1;
                    break;
                }
            }
        

            free(PATH);
            if (!ret) {
                
            }
        }
    }
    
}
