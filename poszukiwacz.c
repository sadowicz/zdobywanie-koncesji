#define _POSIX_C_SOURCE 2
#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int parseArgs(int argc, char** argv, char** resourceName, char** fifoPath);

void waitForParent();

int checkEnd(char* fifoPath);

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

	int counter;
	while(checkEnd(fifoPath))
	{
		counter++;
	}

	printf("ended: %s ( counter: %d )\n", argv[0], counter);
	
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
