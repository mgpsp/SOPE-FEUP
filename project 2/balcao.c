#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "utilities.h"

#define SHARED 0
#define SHM_SIZE sizeof(SharedMemory)

SharedMemory *shm;
int closeCounter = 0;
int fdR, fdW;
char fileName[100];
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void createSharedMemory(char* SHM_NAME)
{
	//check if name has '/'' at the beginning
	char memName[100];
	if (SHM_NAME[0] != '/')
	{
		strcpy(memName, "/");
		strcat(memName, SHM_NAME);
	}
	else
		strcpy(memName, SHM_NAME);

	//create the shared memory region
	int shmfd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
	if (shmfd < 0)
	{
		perror("WRITER failure in shm_open()");
		exit(1);
	}

	//specify region size
	if (ftruncate(shmfd, SHM_SIZE) < 0)
	{
		perror("WRITER failure in ftruncate()");
		exit(1);
	}

	//attach this region to virtual memory
	shm = (SharedMemory *) mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
	if (shm == MAP_FAILED)
	{
		perror("WRITER failure in mmap()");
		exit(1);
	}

	//write the global variables into the shared memory
	shm->openingDate = (int) time(NULL);
	shm->numCounters = 0;

	int i;
	for (i = 0; i < MAX_COUNTERS; i++)
	{
		shm->table[i].empty = 1;
		shm->table[i].averageServingTime = 0;
	}

	//create & initialize mutex
	pthread_mutexattr_t mutexAttr;

	if (pthread_mutexattr_init(&mutexAttr))
	{
		perror("Failure in pthread_mutexattr_init()");
		exit(1);
	}

	if (pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED))
	{
		perror("Failure in pthread_mutexattr_setpshared()");
		exit(1);
	}

	if (pthread_mutex_init(&shm->mutex, &mutexAttr) != 0)
	{
		perror("Failure in pthread_mutex_init()");
		exit(1);
	}

	pthread_mutexattr_destroy(&mutexAttr);

	//create log file
	remove(fileName);
	int fd = open(fileName, O_WRONLY | O_CREAT, 0600);
	if (fd == -1)
	{
		perror("Failure in open()");
		exit(1);
	}
	char message1[] = "quando\t\t\t| quem \t | balcao | o_que\t\t| canal_criado/usado\n";
	char message2[] = "------------------------------------------------------------------------------------\n";
	write(fd, message1, strlen(message1));
	write(fd, message2, strlen(message2));
	writeToLog(fd, getTime(), "Balcao", 1, "inicia_mempart", "-");
	close(fd);
}

void destroySharedMemory(SharedMemory *shm, char* SHM_NAME)
{
	if (munmap(shm,SHM_SIZE) < 0)
	{
		perror("WRITER failure in munmap()");
		exit(1);
	}

	if (shm_unlink(SHM_NAME) < 0)
	{
		perror("WRITER failure in shm_unlink()");
		exit(1);
	}
}

void sigalrmHandler(int signo)
{
	close(fdR);
	close(fdW);
	if (unlink(getFIFO(getpid(), 1)) == -1)
	{
		perror("Failure in unlink()");
		exit(1);
	}
	closeCounter = 1;
}

void generateStatistics()
{
	int closingDate = (int) time(NULL);

	int i = 0;
	int totalClients = 0;
	float servingTime = 0;
	while (i < MAX_COUNTERS && !shm->table[i].empty)
	{
		printf("\n----- Counter %d -----\n", i + 1);
		printf("Working time: %d\n", shm->table[i].duration);
		printf("Served clients: %d\n", shm->table[i].servedClients);
		printf("Average serving time: %g\n", shm->table[i].averageServingTime);
		printf("---------------------\n");
		totalClients += shm->table[i].servedClients;
		servingTime += shm->table[i].averageServingTime * shm->table[i].servedClients;
		i++;
	}

	printf("\nStore's working time: %d\n", closingDate - shm->openingDate);
	printf("Total number of served clients: %d\n", totalClients);
	if (totalClients != 0)
		printf("Average serving time: %g\n", servingTime / (float) totalClients);
}

int lastCounter()
{
	int i;
	for (i = 0; i < MAX_COUNTERS; i++)
	{
		if (!shm->table[i].empty)
			if (shm->table[i].duration == -1)
				return 0;
	}
	return 1;
}

void * serveClient(void * arg)
{
	//get wait time
	pthread_mutex_lock(&shm->mutex);
	shm->table[((ServeInfo *) arg)->index].clientsToServeTWT++;
	int waitTime = shm->table[((ServeInfo *) arg)->index].clientsToServeTWT;
	if (waitTime > 10)
		waitTime = 10;
	pthread_mutex_unlock(&shm->mutex);
	
	int fdlog = open(fileName, O_WRONLY | O_APPEND);
	if (fdlog == -1)
	{
		perror("Failure in open()");
		exit(1);
	}
	writeToLog(fdlog, getTime(), "Balcao", ((ServeInfo *) arg)->index + 1, "inicia_atend_cli", ((ServeInfo *) arg)->clientFIFO);

	//wait to serve client and update counter information
	sleep(waitTime);
	pthread_mutex_lock(&shm->mutex);
	float oldAverage = shm->table[((ServeInfo *) arg)->index].averageServingTime;
	int servedClients = shm->table[((ServeInfo *) arg)->index].servedClients;
	shm->table[((ServeInfo *) arg)->index].clientsToServe--; 
	shm->table[((ServeInfo *) arg)->index].servedClients++;
	shm->table[((ServeInfo *) arg)->index].averageServingTime = (oldAverage * servedClients + waitTime)/ (float) shm->table[((ServeInfo *) arg)->index].servedClients;

	writeToLog(fdlog, getTime(), "Balcao", ((ServeInfo *) arg)->index + 1, "fim_atend_cli", ((ServeInfo *) arg)->clientFIFO);
	close(fdlog);

	//signal client
	int fd = open(((ServeInfo *) arg)->clientFIFO, O_WRONLY);
	if (fd == -1)
	{
		perror("Failure in open()");
		exit(1);
	}
	write(fd, "fim_atendimento", strlen("fim_atendimento") + 1);
	close(fd);
	pthread_mutex_unlock(&shm->mutex);
	//printf("%f\n", shm->table[((ServeInfo *) arg)->index].averageServingTime);
	free(arg);

	//signal main thread if all clients are served
	pthread_mutex_lock(&shm->mutex);
	if (shm->table[((ServeInfo *) arg)->index].clientsToServe == 0)
		pthread_cond_signal(&cond);
	pthread_mutex_unlock(&shm->mutex);

	return NULL;
}

int readLine(int fd, char* message)
{
	int n;

	do
	{
		n = read(fd, message, 1);
	} while(n > 0 && *message++ != '\0');

	return n;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{ 
		printf("Usage: %s <shm_name> <duration>\n", argv[0]); 
		exit(1);
 	} 

	int duration = strtol(argv[2], NULL, 0);
	if (duration == 0)
	{
		printf("Invalid duration\n");
		exit(1);
	}

	char* SHM_NAME = argv[1];
	int shmfd;

	//create filename
	if (SHM_NAME[0] == '/')
		strcpy(fileName, SHM_NAME + 1);
	else
		strcpy(fileName, SHM_NAME);
	strcat(fileName, ".log");

	//create semaphore
	sem_t *sem = sem_open("/semf", O_CREAT, 0660, 1);

	sem_wait(sem);

	//check if the shared memory was already created and create it otherwise
	shmfd = shm_open(SHM_NAME, O_RDWR, 0600);
	if (shmfd < 0)
		createSharedMemory(SHM_NAME);
	else
	{
		//attach shared memory region to virtual memory
		shm = (SharedMemory *) mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
		if (shm == MAP_FAILED)
		{
			perror("WRITER failure in mmap()");
			exit(1);
		}
	}

	sem_post(sem);

	//initialize condition variable
	if (pthread_cond_init(&cond, NULL))
	{
		perror("Failure in pthread_cond_init()");
		exit(1);
	}

	//implement signal handler for SIGALRM
	if (signal(SIGALRM, sigalrmHandler) == SIG_ERR)
	{
		perror("Failure in signal()");
		exit(7);
	}

	//update global variable
	pthread_mutex_lock(&shm->mutex);
	shm->numCounters++;
	pthread_mutex_unlock(&shm->mutex);

	if (shm->numCounters > 50)
	{
		printf("Maximum number of counters is 50\n");
		exit(1);
	}

	//get table line index
	int index = shm->numCounters - 1;

	//create FIFO
	char *fifoName = getFIFO(getpid(), 1);
	if (mkfifo(fifoName, 0660) < 0)
	{
		perror("Failure in mkfifo()");
		exit(1);
	}

	int fd = open(fileName, O_WRONLY | O_APPEND);
	if (fd == -1)
	{
		perror("Failure in open()");
		exit(1);
	}

	writeToLog(fd, getTime(), "Balcao", shm->numCounters, "cria_linh_mempart", fifoName);

	//create counter
	TableEntry counter;
	counter.empty = 0;
	counter.number = shm->numCounters;
	counter.openingDate = (int) time(NULL);
	counter.duration = -1;
	strcpy(counter.fifoName, fifoName);
	counter.clientsToServe = 0;
	counter.clientsToServeTWT = 0;
	counter.servedClients = 0;
	counter.averageServingTime = 0;

	//add new counter to the table
	shm->table[index] = counter;

	int openingCounter = (int) time(NULL);

	//set alarm to signal the closing of the counter
	alarm(duration);

	//get clients info and open serving thread
	fdR = open(fifoName, O_RDONLY);
	fdW = open(fifoName, O_WRONLY);
	pthread_attr_t attr;
	char message[20];
	if (pthread_attr_init(&attr))
	{
		perror("Failure in pthread_attr_init()");
		exit(1);
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
	{
		perror("Failure in pthread_attr_setdetachstate()");
		exit(1);
	}
	while (1)
	{
		if (readLine(fdR, message))
		{
			if (closeCounter)
				break;
			
			ServeInfo *info = (ServeInfo *) malloc(sizeof(ServeInfo));
			strcpy(info->clientFIFO, message);
			info->index = index;
			pthread_t tid;
			if (pthread_create(&tid, &attr, serveClient, (void*) info))
			{
				perror("Failure in pthread_create()");
				exit(1);
			}
		}
	}

	//wait for all clients to be served
	pthread_mutex_lock(&shm->mutex);
	while(shm->table[index].clientsToServe != 0)
		pthread_cond_wait(&cond, &shm->mutex);
	pthread_mutex_unlock(&shm->mutex);
	pthread_cond_destroy(&cond);

	int closingCounter = (int) time(NULL);

	printf("Counter %d closed\n", index + 1);

	//set counter as closed
	shm->table[index].duration = closingCounter - openingCounter;

	writeToLog(fd, getTime(), "Balcao", index + 1, "fecha_balcao", fifoName);

	//close and remove shared memory region and semaphore
	sem_wait(sem);
	if (lastCounter())
	{
		writeToLog(fd, getTime(), "Balcao", index + 1, "fecha_loja\t", fifoName);
		sem_close(sem);
		sem_unlink("/semf");
		generateStatistics();
		pthread_mutex_destroy(&shm->mutex);
		destroySharedMemory(shm, SHM_NAME);
	}
	else
		sem_post(sem);

	close(fd);
	exit(EXIT_SUCCESS);
}
