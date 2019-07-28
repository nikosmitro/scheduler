# scheduler

Program - Exercise 1

Creation of a round-robin scheduler that works at the user space and schedules the children processes. Every processs run for a specific time by cycling order until it is terminated by itself or by signal. The scheduler uses the signals SIGSTOP and SIGCONT to stop and start a process. Each process has a PID that is stored in two arrays to determine which process should run every time.


Program - Exercise 2

Expansion of the sheduler of the program 1. The user can give commands and control the scheduler via a process that is called cortex. The cortex is normally scheduled by the scheduler. The commands that the user can give are :
a)'p' : Prints a list with the process that are running.
b)'k' : Takes the PID of a process and ask the scheduler to terminate it.
c)'e' : Takes the name of an executable and asks the creation of a new process in which the executable will run.
d)'q' : The cortex terminates.


Program - Exercise 3

Expansion of the sheduler of the program 1. The scheduler works with priorities HIGH and LOW. Each process is assigned with a priority HIGH or LOW. The new processes have always LOW priority. First the scheduler runs the HIGH process only until they terminate, and then the LOW ones. The user can change the priority of a process using the cortex by pressing 'l' ('h') and the PID of the process to chang the priority to LOW (HIGH).

