#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include "utilities.h"

#define SHARED 0
#define SHM_SIZE sizeof(SharedMemory)

SharedMemory *shm;

int getCounter()
{
	int i = 0;
	int min = shm->table[i].clientsToServe;
	int minIndex = 0;
	i++;
	while(!shm->table[i].empty && i < MAX_COUNTERS)
	{
		if (shm->table[i].clientsToServe < min)
		{
			min = shm->table[i].clientsToServe;
			minIndex = i;
		}
		i++;
	}
	shm->table[minIndex].clientsToServe++;
	return minIndex;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{ 
		printf("Usage: %s <shm_name> <num_clients>\n", argv[0]); 
		exit(1);
 	}

 	int numClients = strtol(argv[2], NULL, 10);
 	if (numClients == 0)
 	{
 		printf("Invalid number of clients\n");
 		exit(1);
 	}

	char* SHM_NAME = argv[1];

	//check if name has '/'' at the beginning
	char memName[100];
	if (SHM_NAME[0] != '/')
	{
		strcpy(memName, "/");
		strcat(memName, SHM_NAME);
	}
	else
		strcpy(memName, SHM_NAME);

	char fileName[100];
	strcpy(fileName, memName + 1);
	strcat(fileName, ".log");
	int fdlog = open(fileName, O_WRONLY | O_APPEND);

	//check if the shared memory was already created and exit otherwise
	int shmfd = shm_open(SHM_NAME, O_RDWR, 0600);
	if (shmfd < 0)
	{
		perror("Shared memory not created");
		exit(1);
	}
	else
	{
		//attach shared memory region to virtual memory
		shm = (SharedMemory *) mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
		if (shm == MAP_FAILED)
		{
			perror("WRITER failure in mmap()");
			exit(2);
		}
	}

	int i;
	for (i = 0; i < numClients; i++)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			char* fifoName = getFIFO(getpid(), 0);
			errno = 0;
			if (mkfifo(fifoName, 0660) < 0 && errno != 17)
			{
				perror("Failure in mkfifo()");
				exit(4);
			}

			pthread_mutex_lock(&shm->mutex);
			int index = getCounter();
			int fdC = open(shm->table[index].fifoName, O_WRONLY);
			writeToLog(fdlog, getTime(), "Client", index + 1, "pede_atendimento", fifoName);
			write(fdC, fifoName, strlen(fifoName) + 1);
			close(fdC);
			pthread_mutex_unlock(&shm->mutex);
			
			int fd = open(fifoName, O_RDONLY);
			char message[17];
			read(fd, message, 17);
			writeToLog(fdlog, getTime(), "Client", index + 1, "fim_atendimento", fifoName);
			close(fd);
			unlink(fifoName);
			_exit(EXIT_SUCCESS);
		}
		else if (pid < 0)
		{
			perror("Failure creating fork()");
			exit(3);
		}
	}
	exit(EXIT_SUCCESS);
}
