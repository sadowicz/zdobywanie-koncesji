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

int checkEnd(char* fifoPath);

int obtainResource(char* resource);
char* createResourceFileName(char* resource, int resourceNumber);
int createFile(char* name, int resourcesObtained);

int main(int argc, char* argv[])
{
	if(argc != 5)
	{
		printf("USAGE: %s -z <resource_name> -s <fifo_path>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char* resourceName = NULL;
	char* fifoPath = NULL;

	if(!parseArgs(argc, argv, &resourceName, &fifoPath))
		return EXIT_FAILURE;
	
	printf("Hello from: %s\n", argv[0]);

	waitForParent();

	printf("started: %s ( resource: %s\tfifo: %s )\n", argv[0], resourceName, fifoPath);

	while(checkEnd(fifoPath))
	{
		if(!obtainResource(resourceName))
			return EXIT_FAILURE;
	}

	printf("ended: %s\n", argv[0]);
	
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

int checkEnd(char* fifoPath)
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

int obtainResource(char* resource)
{
	int res = 1;

	int resObtained = 0;

	for(int resNumber = 10; resNumber <= 99; resNumber++)
	{
		char* name = createResourceFileName(resource, resNumber);
		if(!(res = (name != NULL)))
			break;

		if(access(name, F_OK))
		{	
			if(!(res = createFile(name, ++resObtained)))
				break;

			//TODO register
		}

		free(name);
	}

	return res;
}

char* createResourceFileName(char* resource, int resourceNumber)
{
	char* res = NULL;
	res = (char*)malloc(strlen(resource) + 5);
	if(res)
	{
		if(sprintf(res, "%s_0%d", resource, resourceNumber) < 0)
		{
			fprintf(stderr, "ERROR: createResourceFileName: Unable to write filename into buffer\n");
			res = NULL;
		}
	}
	else
		fprintf(stderr, "ERROR: createResourceFileName: Unable to allocate buffer for file name\n");

	return res;
}

int createFile(char* name, int resourcesObtained)
{	
	int res = 1;
	errno = 0;
	int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
	if(fd != -1)
	{
		if(!(res = (truncate(name, resourcesObtained * UNIT) != -1)))
			fprintf(stderr, "ERROR: createFile: Unable to set file size\n");

		close(fd);
	}
	else if(errno != EEXIST)
	{
		fprintf(stderr, "ERROR: createFile: Unable to open file descriptor\n");
		res = 0;
	}

	return res;
}
