#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h> 

#define READ 0 
#define WRITE 1

int main(int argc, char *argv[])
{
	// get files path
	char file[strlen(argv[1]) + strlen(argv[2]) + 1];
	strcpy(file, argv[1]);
	char pattern[strlen(argv[1]) + 1];
	strcpy(pattern, argv[1]);
	char* indexFile = argv[0];
	indexFile[strlen(argv[0]) - 2] = '\0';
	char fileName[strlen(argv[2]) + 1];
	strcpy(fileName, argv[2]);

	strcat(file, fileName);
	strcat(pattern, "words.txt");
	strcat(indexFile, "index");
	strcat(indexFile, argv[3]);
	strcat(indexFile, ".txt");

	if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
		perror("Error in function signal");

	int fd[2];
	if (pipe(fd) == -1)
		perror("Error creating pipe");

	pid_t pid = fork();

	if (pid == 0) // child
	{
		close(fd[READ]);
		dup2(fd[WRITE], STDOUT_FILENO);

		// flag o to print only the matching words
		// flag n to print the line-number where the word has been found
		// flag w to select lines that contain the whole word
		// flag f to get the patterns one per line
		execlp("grep", "grep", "-o","-n","-w", "-f", pattern, file, NULL);
	}
	else if (pid > 0) // parent
	{
		char word[40];
		int lineNumber;
		close(fd[WRITE]);

		// get filename without .txt
		fileName[strlen(fileName) - 4] = '\0';

		// read from pipe and write to index in the desired format
		FILE* out = fdopen(fd[READ], "r");
		FILE* index = fopen(indexFile, "wa");

		while (fscanf(out, "%d:%s", &lineNumber, word) == 2)
			fprintf(index, "%s: %s-%d\n", word, fileName, lineNumber);

		fclose(index);
		fclose(out);
		close(fd[READ]);
	}
	else
		perror("Error creating fork");

	return 0;
}