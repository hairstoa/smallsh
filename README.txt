Instructions

1) Compile
Download the files and enter the repository that contains them. Then enter the following into the command line(without quotations):

    "make"

2) Run with the command below:

    "smallsh"

2.a) Custom commands (once the smallsh is running):
    "exit"
    The exit command exits this shell. It takes no arguments. 
    When this command is run, any other processes or jobs that your shell has started before it terminates itself are killed.

    "cd"
    The cd command changes the working directory of smallsh.

    By itself - with no arguments - it changes to the directory specified in the HOME environment variable
    This command can also take one argument: the path of a directory to change to. 
    The cd command supports both absolute and relative paths.

    "status"
    The status command prints out either the exit status or the terminating signal of the last foreground process ran by this shell.

2.b) Other smallsh features
    Comments & Blank Lines are ignored
    Expansion of Variable $$ into the process id
    Executing Other Shell Commands via exec() functions
    Input & Output Redirection
    Executing Commands in Foreground & Background
    CRTL-C input sends SIGINT to all parent and child processes simultaneously
    CRTL-Z input sends SIGTSTP to all processes

