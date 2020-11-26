#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#define _DEFAULT_SOURCE

#define UNIT 384

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int parseArgs(int argc, char** argv, char** resourceName, char** fifoPath);

void waitForParent();

int checkEnd();

int obtainResource();
char* createResourceFileName(long resourceNumber);
int createFile(char* name);

void markResource(int status, void* resourceNumber);
char* createMarkedName();

char* procName = NULL;
char* resourceName = NULL;
char* fifoPath = NULL;

int resourcesObtained = 0;

int main(int argc, char* argv[])
{
	if(argc != 5)
	{
		printf("USAGE: %s -z <resource_name> -s <fifo_path>\n", argv[0]);
		return EXIT_FAILURE;
	}

	procName = argv[0];

	if(!parseArgs(argc, argv, &resourceName, &fifoPath))
		return EXIT_FAILURE;
	
	waitForParent();

	while(checkEnd(fifoPath))
	{
		if(!obtainResource(resourceName))
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int parseArgs(int argc, char** argv, char** resourceName, char** fifoPath)
{
	int res = 1;
	int opt;

	while((opt = getopt(argc, argv, ":z:s:")) != -1)
	{
		switch(opt)
		{
			case 'z':
				*resourceName = argv[optind - 1];
				break;

			case 's':
				*fifoPath = argv[optind - 1];
				break;

			case ':':
				fprintf(stderr, "ERROR: parseArgs: Param ( %c ) needs value\n", opt);
				res = 0;
				break;

			case '?':
				fprintf(stderr, "ERROR: parseArgs: Unknown param ( %c )\n", optopt);
				res = 0;
				break;
		}
	}

	return res;
}

void waitForParent()
{
	char temp;

	read(4, &temp, 1);
	close(4);
}

int checkEnd()
{
	int res = 1;

	errno = 0;
	int fd = open(fifoPath, O_WRONLY | O_NONBLOCK);

	if(fd != -1)
	{
		res = 0;
		close(fd);
	}
	else if(errno != ENXIO)
	{
		fprintf(stderr, "ERROR: checkEnd: Unrecognized error while opening fifo file\n");
		res = 0;
	}

	return res;
}

int obtainResource()
{
	int res = 1;

	for(long resNumber = 10; resNumber <= 99; resNumber++)
	{
		char* name = createResourceFileName(resNumber);
		if(!(res = (name != NULL)))
			break;

		if(access(name, F_OK))
		{	
			int crFileRes = createFile(name); 
	
			if(crFileRes == 1)
			{
				if(!(res = on_exit(markResource, (void*)resNumber) == 0))
					fprintf(stderr, "ERROR: obtainResource: Unable to register function\n");
			}
			else if(crFileRes == -1)
				res = 0;

			break;
		}

		free(name);
	}

	return res;
}

char* createResourceFileName(long resourceNumber)
{
	char* res = NULL;
	res = (char*)malloc(strlen(resourceName) + 18);
	if(res)
	{
		if(sprintf(res, "property_0%ld.%smine", resourceNumber, resourceName) < 0)
		{
			fprintf(stderr, "ERROR: createResourceFileName: Unable to write filename into buffer\n");
			res = NULL;
		}
	}
	else
		fprintf(stderr, "ERROR: createResourceFileName: Unable to allocate buffer for file name\n");

	return res;
}

int createFile(char* name)
{	
	int res = 1;
	errno = 0;
	int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if((res = (fd != -1)))
	{
		if(!(res = (truncate(name, (++resourcesObtained) * UNIT) != -1)))
			fprintf(stderr, "ERROR: createFile: Unable to set file size\n");

		close(fd);
	}
	else if(errno != EEXIST)
	{
		fprintf(stderr, "ERROR: createFile: Unable to open file descriptor\n");
		res = -1;
	}

	return res;
}

void markResource(int status, void* resourceNumber)
{
	long number = (long)resourceNumber;

	char* oldName = createResourceFileName(number);
	char* newName = createMarkedName();

	rename(oldName, newName);
}

char* createMarkedName()
{
	char* res = NULL;
	res = (char*)malloc(strlen(procName) + strlen(resourceName) + 10);
	if(res)
	{
		if(sprintf(res, "%s_%smine.#%d", procName, resourceName, resourcesObtained--) < 0)
		{
			fprintf(stderr, "ERROR: createMarkedName: Unable to write filename into buffer\n");
			res = NULL;
		}
	}
	else
		fprintf(stderr, "ERROR: createMarkedName: Unable to allocate buffer for file name\n");

	return res;
}
