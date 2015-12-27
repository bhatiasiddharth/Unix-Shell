#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>


int numOfJobs=0;
typedef struct job Job;

struct job
{
        int jid;
        pid_t pid;
        pid_t gid;
        int status;
        struct job *next;
};

pid_t main_pid;
pid_t main_pgid;
int main_terminal;


#define bypid 1
#define byjid 2

Job* jobsList=NULL;


int changeJobStatus(int pid, int status)
{
        Job *job = jobsList;
        if (job == NULL)
        {
            return 0;
        }
        else
        {
            while (job != NULL) 
            {
                if (job->pid == pid) 
                {
            	   	job->status = status;
                   	return 1;
               	}    
				job = job->next;
            }
            return 0;
        }
}


Job* insertJob(pid_t pid,pid_t gid, int status)
{
        
        Job *newJob = malloc(sizeof(Job));
		newJob->pid = pid;
        newJob->gid=gid;
        newJob->status = status;
        newJob->next = NULL;

        if (jobsList == NULL) 
        {
            numOfJobs++;
			newJob->jid = numOfJobs;
            return newJob;
        } 

        else 
        {
            Job *tempNode = jobsList;
            while (tempNode->next != NULL) {
                    tempNode = tempNode->next;
            }
            newJob->jid = tempNode->jid + 1;
            tempNode->next = newJob;
            numOfJobs++;
            return jobsList;
        }
}



Job* delJob(Job* job)
{
        if (jobsList == NULL)
            return NULL;

        Job* currentJob;
        Job* beforeCurrentJob;

        currentJob = jobsList->next;
        beforeCurrentJob = jobsList;

        if (beforeCurrentJob->pid == job->pid) {

            beforeCurrentJob = beforeCurrentJob->next;
            numOfJobs--;
            return currentJob;
        }

        while (currentJob != NULL)
        {
            if (currentJob->pid == job->pid)
            {
                numOfJobs--;
                beforeCurrentJob->next = currentJob->next;
            }

            beforeCurrentJob = currentJob;
            currentJob = currentJob->next;
        }

        return jobsList;
}


Job* getJob(int val, int parameter)
{
        Job* job = jobsList;
        switch (parameter)
        {
        	case bypid:
	            while (job != NULL)
	            {
	                if (job->pid == val)
	                    return job;
	                else
	                    job = job->next;
	            }
	            break;
        	case byjid:
	            while (job != NULL)
	            {
		            if (job->jid == val)
			            return job;
		            else
		                job = job->next;
	            }
	            break;
        }
        return NULL;
}



void waitJob(Job* job)
{
    int status;
    while (waitpid(job->pid, &status, WNOHANG) == 0)
    {
        
        if (job->status == 'S')
           	return;
    }
    jobsList = delJob(job);
}



void printJobs()
{
        Job* job = jobsList;
        if (job == NULL) 
        {
            printf("No Jobs\n");
        } 
        else
        {
            while (job != NULL) 
            {
            printf("%d\t%d\t%d\n", job->jid, job->pid, job->status);
            job = job->next;
            }
        }
}


void killJob(int jid)
{

    Job *job = getJob(jid, byjid);
    if(job!=NULL)
    	kill(job->pid, SIGKILL);
	else
		printf("No such job\n");
        
}




void signalHandler_child(int p)
{
    pid_t pid;
    int status;
    pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);
    
    if (pid > 0)
    {
        Job* job = getJob(pid, bypid);
        
        if (job == NULL)
                return;
        
        if (WIFEXITED(status))
        {
            if (job->status == 'B')
            {
                printf("\nDone\t%d\n", job->jid);
                jobsList = delJob(job);
        	}
        }

        else if (WIFSIGNALED(status))
        {
            printf("\nKilled\t%d\n", job->jid);
            jobsList = delJob(job);
        }

        else if (WIFSTOPPED(status))
        {
            if (job->status == 'B')
            {
                tcsetpgrp(main_terminal, main_pgid);
                changeJobStatus(pid, 'W');
                printf("\nSuspended\t%d\n",numOfJobs);
            } 

            else
            {
                tcsetpgrp(main_terminal, job->gid);
                changeJobStatus(pid, 'S');
                printf("\nStopped\t%d\n", numOfJobs);
            }
            return;
        }

        else
        {
            if (job->status == 'B')
            {
                jobsList = delJob(job);
            }
        }

        tcsetpgrp(main_terminal, main_pgid);
    }
}

void bgJob(Job* job, int check)
{


    if (job == NULL)
        return;

    if (check && job->status != 'W')
        job->status = 'W';
    if (check)
                if (kill(-job->gid, SIGCONT) < 0)
                        perror("kill (SIGCONT)");

    tcsetpgrp(main_terminal, main_pgid);
        
}


void fgJob(Job* job, int check)
{

    job->status = 'F';
    tcsetpgrp(main_terminal, job->gid);
    if (check)
    {
    
        if (kill(-job->gid, SIGCONT) < 0)
                perror("kill (SIGCONT)");
    }
    
    
    waitJob(job);

    
    tcsetpgrp(main_terminal, main_pgid);
    
    
}

void execute(char ** arg, char mode )
{
	pid_t pid;
	char *environ[] = { NULL };
	int pidvalue;

	if ((pid=fork())<0)
	{
		perror("Fork Error");
		exit(1);
	}

	else if(pid==0)
	{
		signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, &signalHandler_child);
        signal(SIGTTIN, SIG_DFL);
        //usleep(20000);
        setpgrp();

        if (mode == 'F')
            tcsetpgrp(main_terminal, getpid());

        if (mode == 'B')
            printf("%d\t%d\n", ++numOfJobs, (int) getpid());
		
		if(execvpe(arg[0],arg,environ)<0)
		{
			perror("Execute Error");
			exit(1);
		}

		exit(EXIT_SUCCESS);
	}

	else
	{

		setpgid(pid, pid);
		jobsList = insertJob(pid, pid,(int)mode);
		Job* job = getJob(pid, bypid);
        
		
		if(mode=='F')
		{
			fgJob(job,0);
				
		}
		
		if(mode=='B')
		{
            bgJob(job,0);
	        
		}
        printf("%d\t%d\n", job->pid, job->status);
	}
}



void  parse(char *str, char **argv)
{
   while (*str != '\0')        /* if not the end of line ....... */ 
    {
    	while (*str == ' ' || *str== '\t' || *str == '\n')
            *str++ = '\0';     /* replace white spaces with 0    */
        *argv++ = str;          /* save the argument position     */
        while (*str != '\0' && *str != ' ' && 
        	*str != '\t' && *str != '\n') 
            str++;             /* skip the argument until ...    */
    }
     *argv = '\0';                 /* mark the end of argument list  */
}




int main()
{
	
	


	main_pid = getpid();
	
    main_terminal = STDIN_FILENO;

    int num = isatty(main_terminal);

    if (num)
    {
        while (tcgetpgrp(main_terminal) != (main_pgid = getpgrp()))
            kill(main_pid, SIGTTIN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGCHLD, &signalHandler_child);
        

        setpgid(main_pid,main_pid);
        main_pgid = getpgrp();
        tcsetpgrp(main_terminal, main_pgid);
    }

	
	int len,flagback=0;

	char *arg[64];
	char str[1024];
	char currentdir[1024];

	
	while(1)
	{
		flagback=0;
		int argc=64;
		while (argc != 0)
		{
            arg[argc] = NULL;
            argc--;
    	}

		printf("%s :> ",getcwd(currentdir, 1024));
		
		gets(str);
		len=strlen(str);

		if(str[len-1]=='&')
		{
			flagback=1;
			str[len-1]='\0';
		}


		parse(str,arg);
			
			
		if(arg[0]!='\0')
		{
			if(strcmp(arg[0],"exit")==0)
					 exit(EXIT_SUCCESS);

			else if(strcmp(arg[0],"cd")==0)	
			{
				if (arg[1] == NULL) 
        			chdir(getenv("HOME"));

        			
        		else if(chdir(arg[1])==-1)
					printf("No such directory\n");
			}


			else if (strcmp("jobs", arg[0]) == 0)
			 	printJobs(); 


			else if (strcmp("bg", arg[0]) == 0)
			{
                if (arg[1] != NULL)
                execute(arg+1,'B');   
         
			}


			else if (strcmp("fg", arg[0]) == 0)
			{
				if (arg[1] != NULL)
					{
                        int jobid = (int) atoi(arg[1]);
                        Job *job = getJob(jobid, byjid);
                        if(job!=NULL)
                        {
                            if (job->status == 'S' || job->status == 'W')
                                fgJob(job, 1);
                            else                                                                                          
                                fgJob(job, 0);    
                        }
                        
                    }	

				else
						printf("Enter Job ID\n");

                
				
				
			}


	        else if (strcmp("kill", arg[0]) == 0)
	        {
	                if (arg[1] != NULL)
						killJob(atoi(arg[1]));
					else
						printf("Enter Job ID\n");
	        }


			else if(flagback)
				execute(arg,'B');
			else
				execute(arg,'F');
			
		}
	}
}
