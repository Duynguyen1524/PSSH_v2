Justin Nguyen
14554307
Project PSSH version 2

How To Compile & Run
====================
$ make clean
$ make
$ ./pssh

Description
===========
The program is a lite version of a shell. It can perform simple task as shell. For example, ls, grep, echo,... and can do piping
These are some basic 
$ make clean
The make clean command will remove the .o files and the pssh program.

$ make
The make command will add the .o files for the code files and create the pssh program.

$ ./pssh
Once you enter this program, it will now run your pssh shell program. It will behave as a lite version of unix shell

$ exit or control + C
You can  exit the program pssh by type exit or doing ctrl + c which terminates the program.

update for version 2 
It builds upon the previous version by introducing Job Control functionalities. The shell supports standard shell commands (e.g., ls, grep, echo, etc.), piping, and now features full job control using process groups, signals, and signal handlers.
