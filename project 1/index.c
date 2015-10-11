#include <stdio.h> 
#include <unistd.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <dirent.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

int main(int argc, char *argv[]) 
{
	DIR *dir; 
	struct dirent *dentry; 
	struct stat stat_entry; 

	if (argc != 2)
	{ 
		printf("Usage: %s <dir_path>\n", argv[0]); 
		return 1; 
 	} 

	if ((dir = opendir(argv[1])) == NULL)
	{ 
		perror(argv[1]); 
		return 2; 
 	}

 	// check if words.txt exists
 	char docsPath[255];
 	realpath(argv[1], docsPath);
 	strcat(docsPath, "/");
 	char wordsFile[strlen(docsPath) + 1];
 	strcpy(wordsFile, docsPath);
 	strcat(wordsFile, "words.txt");
 	if (access(wordsFile, F_OK) == -1)
 	{
 		perror("words.txt");
 		return 3;
 	}


 	// get path for auxiliary programms
 	int k = 0, status;
 	char swPath[255], cscPath[255], absolutePath[255];
 	realpath(argv[0], absolutePath);
 	absolutePath[strlen(absolutePath) - 5] = '\0';
 	strcpy(swPath, absolutePath);
 	strcpy(cscPath, absolutePath);
 	strcat(swPath, "/sw");
 	strcat(cscPath, "/csc");

 	// read the directory with the files to analyze
 	chdir(docsPath); 
	while ((dentry = readdir(dir)) != NULL)
	{ 
		stat(dentry->d_name, &stat_entry); 
		if (S_ISREG(stat_entry.st_mode))
		{ 
			if (strcmp(dentry->d_name, "words.txt") != 0 && strcmp(dentry->d_name, "index.txt") != 0)
			{
				k++; // file index (to create index file for this file)
				char buffer[5];
				sprintf(buffer, "%d", k);

				pid_t pid = fork();
				if (pid == 0)
				{
					if (execlp(swPath, swPath, docsPath, dentry->d_name, buffer, NULL) == -1)
						perror("Error executing sw");
				}
				else if (pid > 0)
					wait(&status);
				else
					perror("Error creating fork");
			} 
  		}  
  	}

  	// check if there are files to index
  	if (k == 0)
  	{
  		perror("Files to index not found");
  		return 4;
  	}

  	// create index
  	pid_t pid = fork();
  	if (pid == 0)
  	{
		char buffer[5];
		sprintf(buffer, "%d", k);
		if (execlp(cscPath, cscPath, docsPath, buffer, NULL) == -1)
			perror("Error executing csc");
  	}
  	else if (pid > 0)
  		wait(&status);
  	else
  		perror("Error creating fork");

  	// remove temp index files
  	int i;
  	for (i = 1; i <= k; i++)
  	{
  		char buffer[5];
		sprintf(buffer, "%d", i);
		char fileName[strlen(buffer) + 9];
		strcpy(fileName, absolutePath);
		strcat(fileName, "/index");
		strcat(fileName, buffer);
		strcat(fileName, ".txt");
		if (remove(fileName) != 0)
			perror("Error deleting file");
  	}

	return 0; 
}