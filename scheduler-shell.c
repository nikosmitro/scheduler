#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "proc-common.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 1                /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */

/* a linked list of the processes that contains their id, pid and the program they execute*/
typedef struct info_proc {
        int id;
        pid_t PID;
        char name[TASK_NAME_SZ];
        struct info_proc * next;
        } process;

/* Global Variables. */
static int nproc;
static process *current_proc,*temp;

/* Disable delivery of SIGALRM and SIGCHLD. */
static void
signals_disable(void)
{
        sigset_t sigset;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGALRM);
        sigaddset(&sigset, SIGCHLD);
        if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
                perror("signals_disable: sigprocmask");
                exit(1);
        }
}
/* Enable delivery of SIGALRM and SIGCHLD.  */
static void
signals_enable(void)
{
        sigset_t sigset;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGALRM);
        sigaddset(&sigset, SIGCHLD);
        if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
                perror("signals_enable: sigprocmask");
                exit(1);
        }
}

static void input(int id, pid_t p, char* name)  /*insert a new procedure to the list*/
{
    process *temp = (struct info_proc *) malloc(sizeof(struct info_proc));      /*make room for the new procedure*/
        temp->id = id;
        temp->PID = p;
        strcpy(temp->name,name);
        temp->next = current_proc->next;
        current_proc->next = temp;      /*we make the new procedure the next procedure of the current,insert it after the current procedure*/
        current_proc = temp;    /*the current_proc is now the new one*/
}

static void delete(int id){     /*delete a procedure from the linked list*/
        temp = current_proc;
        while (temp->next->id!=id)      /*search for the desired procedured we want to delete*/
                temp=temp->next;
        temp->next = temp->next->next;
}

static void
do_child(char *executable)
{
        char *newargv[] = { executable , NULL, NULL, NULL};
        char *newenviron[] = { NULL };
        raise(SIGSTOP);         /*process stoped*/
        execve( executable , newargv, newenviron);/*excecute the program*/
        /* execve() only returns on error */
        perror("scheduler: child: execve");
        exit(1);
}
/* Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void)
{
        temp = current_proc;
        printf("id:%d PID:%d name:%s (current process) \n",temp->id,temp->PID,temp->name);
        while(temp->next!=current_proc){
                        temp=temp->next;
                        printf("id:%d PID:%d name:%s \n",temp->id,temp->PID,temp->name);
        }
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int
sched_kill_task_by_id(int id)
{
        temp = current_proc;
        while (temp->id!=id){   /*search for the process with the desired id*/
                temp=temp->next;
                if(temp==current_proc)
                        return -1;      /*if there isn't return -1*/
        }
        printf("Process %d with PID %d has been killed.\n",id,temp->PID);
        kill(temp->PID, SIGKILL);
        return(id);
}


/* Create a new task.  */
static void
sched_create_task(char *executable)
{
        nproc++;
        printf("Creating process: %s id: %d \n",executable,nproc-1);
        pid_t p = fork();       /*create the new process*/
        if (p<0){
                perror("fork");
        }
        if (p==0){
                do_child(executable);   /*do_child*/
        }
        else{
                input(nproc-1,p,executable);    /*and insert the process to the list*/
        }
}

/* Process requests by the shell.  */
static int
process_request(struct request_struct *rq)
{
        switch (rq->request_no) {
                case REQ_PRINT_TASKS:
                        sched_print_tasks();
                        return 0;

                case REQ_KILL_TASK:
                        return sched_kill_task_by_id(rq->task_arg);

                case REQ_EXEC_TASK:
                        sched_create_task(rq->exec_task_arg);
                        return 0;

                default:
                        return -ENOSYS;
        }
}

/*
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum)
{
        //assert(0 && "Please fill me!");
        kill(current_proc->PID, SIGSTOP);
}

/*
 * SIGCHLD handler
 */
static void
sigchld_handler(int signum)
{
        //assert(0 && "Please fill me!");
        pid_t p;
        int status;
        for(;;){
                //assert(0 && "Please fill me!");
                /*
                 * waitpid : on success, returns the process ID of the child whose  state has changed
                 * -1 : wait for any child process
                 * WNOHANG : return immediately if no child has exited.
                 * WUNTRACED : also return if a child has  stopped  (but  not  traced  via ptrace(2))
                */
                p = waitpid(-1, &status, WUNTRACED | WNOHANG);
                if(p==0)
                        break;
                //explain_wait_status(p, status);
                while(current_proc->PID != p)   /* in case another procedure has been terminated unexpected we search for the rigth pid -process*/
                        current_proc=current_proc->next;
                if (WIFEXITED(status)  || WIFSIGNALED(status)) {
                        /* A child has died */
                        printf("Parent: Child %d has been terminated. Moving to the next one...\n", current_proc->PID);

                        if(current_proc->next == current_proc){
                                printf("All children have died \n");
                                exit(0);        /*if no other children message and exit*/
                        }
                        delete(current_proc->id);       /*else delete the process from list*/
                }
                if (WIFSTOPPED(status)) {
                        /* A child has stopped due to SIGSTOP/SIGTSTP, etc... */
                        printf("Parent: Child %d has been stopped. Moving to the next one...\n", current_proc->PID);
                }
                /* A child has stopped due to SIGSTOP/SIGTSTP, etc... */
                current_proc = current_proc->next;      /*go to the next process*/
                alarm(SCHED_TQ_SEC);    /*alarm*/
                kill(current_proc->PID, SIGCONT);       /*continue the next process*/
        }
}


/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void
install_signal_handlers(void)
{
        sigset_t sigset;
        struct sigaction sa;

        sa.sa_handler = sigchld_handler;
        sa.sa_flags = SA_RESTART;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGCHLD);
        sigaddset(&sigset, SIGALRM);
        sa.sa_mask = sigset;
        if (sigaction(SIGCHLD, &sa, NULL) < 0) {
                perror("sigaction: sigchld");
                exit(1);
        }

        sa.sa_handler = sigalrm_handler;
        if (sigaction(SIGALRM, &sa, NULL) < 0) {
                perror("sigaction: sigalrm");
                exit(1);
        }

        /*
         * Ignore SIGPIPE, so that write()s to pipes
         * with no reader do not result in us being killed,
         * and write() returns EPIPE instead.
         */
        if (signal(SIGPIPE, SIG_IGN) < 0) {
                perror("signal: sigpipe");
                exit(1);
        }
}

static void
do_shell(char *executable, int wfd, int rfd)
{
        char arg1[10], arg2[10];
        char *newargv[] = { executable, NULL, NULL, NULL };
        char *newenviron[] = { NULL };

        sprintf(arg1, "%05d", wfd);
        sprintf(arg2, "%05d", rfd);
        newargv[1] = arg1;
        newargv[2] = arg2;

        raise(SIGSTOP);
        execve(executable, newargv, newenviron);

        /* execve() only returns on error */
        perror("scheduler: child: execve");
        exit(1);
}

/* Create a new shell task.
 *
 * The shell gets special treatment:
 * two pipes are created for communication and passed
 * as command-line arguments to the executable.
 */
static void
sched_create_shell(char *executable, int *request_fd, int *return_fd)
{
        pid_t p;
        int pfds_rq[2], pfds_ret[2];

        if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
                perror("pipe");
                exit(1);
        }
        printf("Forking the shell with id: 0...\n");
        p = fork();
        if (p < 0) {
                perror("scheduler: fork");
                exit(1);
        }

        if (p == 0) {
                /* Child */
                close(pfds_rq[0]);
                close(pfds_ret[1]);
                do_shell(executable, pfds_rq[1], pfds_ret[0]);  /*create the shell*/
                assert(0);
        }
        /* Parent */
        current_proc=(struct info_proc *) malloc(sizeof(struct info_proc));
        current_proc->PID = p;
        strcpy(current_proc->name,SHELL_EXECUTABLE_NAME);
        current_proc->next = current_proc;      /*insert shell to list*/
        close(pfds_rq[1]);
        close(pfds_ret[0]);
        *request_fd = pfds_rq[0];
        *return_fd = pfds_ret[1];
}

static void
shell_request_loop(int request_fd, int return_fd)
{
        int ret;
        struct request_struct rq;

        /*
         * Keep receiving requests from the shell.
         */
        for (;;) {
                if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
                        perror("scheduler: read from shell");
                        fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
                        break;
                }
                signals_disable();
                ret = process_request(&rq);
                signals_enable();

                if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
                        perror("scheduler: write to shell");
                        fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
                        break;
                }
        }
}

int main(int argc, char *argv[])
{
        /* Two file descriptors for communication with the shell */
        static int request_fd, return_fd;
        pid_t p;
        int i;
        nproc = argc; /* number of processes goes here */
        /* Create the shell. */
        sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);
        /*Creat the other processes*/
        for (i=1; i < nproc; i++){
                printf("Forking the process with id :%d\n",i);
                p = fork();
                if (p<0){
                        perror("scheduler: fork");
                        exit(1);
                }
                if (p == 0){ /*Child Code*/
                        do_child(argv[i]);      /* children execute the  excecutable programs*/
                }
                else{   /*father codee*/
                        input(i,p,argv[i]);     /*insert child process in list*/
                }
        }

        /* Wait for all children to raise SIGSTOP before exec()ing. */
        wait_for_ready_children(nproc);

        /* Install SIGALRM and SIGCHLD handlers. */
        install_signal_handlers();

        if (nproc == 0) {
                fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
                exit(1);
        }
        else{
                alarm(SCHED_TQ_SEC);
                current_proc=current_proc->next;        /*the current process is the last one that was created so we send SIGCONT to the next one which is the first = shell*/   
				kill(current_proc->PID, SIGCONT);

        }

        shell_request_loop(request_fd, return_fd);

        /* Now that the shell is gone, just loop forever
         * until we exit from inside a signal handler.
         */
        while (pause())
                ;

        /* Unreachable */
        fprintf(stderr, "Internal error: Reached unreachable point\n");
        return 1;
}

