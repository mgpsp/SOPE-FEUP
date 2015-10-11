#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h> 
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	char tempFile[strlen(argv[1]) + 1];
	strcpy(tempFile, argv[1]);
	strcat(tempFile, "tempIndex.txt");
	char* path = argv[0];
	path[strlen(argv[0]) - 3] = '\0';

	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
		perror("Error in function signal");

	// number of files to concatenate and sort
	int n = strtol(argv[2], NULL, 10);

	// create and get file descriptor for tempFile
	int fd = open(tempFile, O_WRONLY | O_CREAT, 0750); 
	if (fd == -1)
		perror("Error creating tempIndex");

	int cenas = dup(STDOUT_FILENO);
	if (dup2(fd, STDOUT_FILENO) == -1)
		perror("Error duplicating descriptor");

	// concatenate files
	int i, status;
	for (i = 1; i <= n; i++)
	{
		pid_t pid = fork();
		if (pid == 0) // child
		{
			char fileName[20];
			strcpy(fileName, "index");
			char buffer[5];
			sprintf(buffer, "%d", i);
			strcat(fileName, buffer);
			char file[strlen(path) + 1];
			strcpy(file, path);
			strcat(fileName, ".txt");
			strcat(file, fileName);
			execlp("cat", "cat", file, NULL);
		}
		else if (fork > 0) // parent
			wait(&status);
		else
			perror("Error creating fork");
	}

	// sort files
	pid_t pid = fork();
	if (pid == 0)
		// flag V to interpret numbers as integers and not as characters	
		// flag f to not distinguish lower case characters from upper case characters
		// flag o to write the result into temp File instead of the standard output
		execlp("sort", "sort", "-V", "-f", "-o", tempFile, tempFile, NULL);
	else if (fork < 0)
		perror("Error creating fork");

	wait(&status);

	dup2(cenas, STDOUT_FILENO);
	// create index
	char indexFile[strlen(argv[1]) + 1];
	strcpy(indexFile, argv[1]);
	strcat(indexFile, "index.txt");

	FILE* index = fopen(indexFile, "wa");
	FILE* tempIndex = fopen(tempFile, "r");
	char currentWord[20], location[10];
	char previousWord[20] = "";
	fputs("INDEX", index);
	
	while(fscanf(tempIndex, "%[^:]: %s\n", currentWord, location) == 2)
	{
		if (strcmp(previousWord, currentWord) == 0)
			fprintf(index, ", %s", location);
		else
		{
			strcpy(previousWord, currentWord);
			fprintf(index, "\n\n%s: %s", currentWord, location);
		}
	}

	if (remove(tempFile) != 0)
		perror("Error deleting file");

	return 0;
}