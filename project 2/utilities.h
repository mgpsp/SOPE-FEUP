#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_COUNTERS 50

typedef struct {
	int empty;
	int number;
	int openingDate;
	int duration;
	char fifoName[20];
	int clientsToServeTWT;
	int clientsToServe;
	int servedClients;
	float averageServingTime;
} TableEntry;

typedef struct {
	int openingDate;
	int numCounters;
	pthread_mutex_t mutex;

	TableEntry table[MAX_COUNTERS];
} SharedMemory;

typedef struct {
	char clientFIFO[20];
	int index;
} ServeInfo;

char* getTime()
{
	time_t timer;
    char* buffer = malloc(26);
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);

    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    return buffer;
}

void writeToLog(int fd, char* timeStamp, char* who, int counter, char* what, char* channel)
{
	char message[200];
	char ch[15];

	//rempove /tmp/
	if (strcmp(channel, "-") != 0)
		strncpy(ch, channel + 5, strlen(channel) - 4);
	else
		strcpy(ch, "-");

	sprintf(message, "%s\t| %s | %d\t  | %s\t| %s\n", timeStamp, who, counter, what, ch);
	
	if (write(fd, message, strlen(message)) < strlen(message))
	{
		perror("Failure in write()");
		exit(1);
	}
}

char* getFIFO(pid_t pidN, int counter)
{
	char pid[10];
	sprintf(pid, "%d", pidN);
	char *fifoName = malloc(100);
	if (counter)
		strcpy(fifoName, "/tmp/fb_");
	else
		strcpy(fifoName, "/tmp/fc_");
	strcat(fifoName, pid);
	return fifoName;
}

#endif