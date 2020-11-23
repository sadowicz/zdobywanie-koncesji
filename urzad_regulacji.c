#define _POSIX_C_SOURCE 2
#define _XOPEN_SOURCE

#define DEFAULT_CH_QUANT 16
#define MAX_FIFO_NAME_LEN 20

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

int parseArgs(int argc, char** argv, double* time, long* childrenQuantity, char** resourceName, char** childName, char** fifoPath);

double strToMillisec(char* str);
long strToChQuantity(char* str);

int createFifo(char** path);
char* randFifoName();

int createChildren(pid_t* pids, long quantity, char* childrenName, char* resourceName, char* fifoPath);
pid_t createChild(char* childName, char* resourceName, char* fifoPath);
char* createChildName(char* namePrefix, long childNo);

int main(int argc, char* argv[])
{
	if(argc < 7 || argc > 10)
	{
		printf("USAGE: %s -t <time> -z <resource_name> -p <children_name> [-n <children_quantity>] [<fifo_path>]\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	double time = 0;
	long childrenQuant = 0;

	char* resourceName = NULL;
	char* childName = NULL;
	char* fifoPath = NULL;

	if(!parseArgs(argc, argv, &time, &childrenQuant, &resourceName, &childName, &fifoPath) || !resourceName || !childName)
		return EXIT_FAILURE;

	if(childrenQuant <= 4)
		childrenQuant = DEFAULT_CH_QUANT;

	if(!createFifo(&fifoPath))
		return EXIT_FAILURE;

	printf("fifo path: %s\n", fifoPath);

	pid_t* pids = (pid_t*)malloc(childrenQuant * sizeof(pid_t));

	if(!createChildren(pids, childrenQuant, childName, resourceName, fifoPath))
		return EXIT_FAILURE;

	free(pids);

	return EXIT_SUCCESS;
}

int parseArgs(int argc, char** argv, double* time, long* childrenQuantity, char** resourceName, char** childName, char** fifoPath)
{
	int res = 1;
	int opt;

	while((opt = getopt(argc, argv, ":t:n:z:p:")) != -1)
	{
		switch(opt)
		{
			case 't':
				*time = strToMillisec(optarg);
				res = (*time != -1);
				break;
		
			case 'n':
				*childrenQuantity = strToChQuantity(optarg);	
				break;

			case 'z':
				*resourceName = argv[optind - 1];
				break;

			case 'p':
				*childName = argv[optind - 1];
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

	if(optind < argc)
		*fifoPath = argv[optind];

	return res;
}

double strToMillisec(char* str)
{
	char* endptr = NULL;
	errno = 0;

	double res = strtod(str, &endptr);

	if(errno || *endptr || res < 0)
	{
		fprintf(stderr, "ERROR: strToMillisec: Unable to convert string to milliseconds\n");
		res = -1;
	}

	return res;
}

long strToChQuantity(char* str)
{
	char* endptr = NULL;
	errno = 0;

	long res = strtol(str, &endptr, 10);

	if(errno || *endptr || res <= 4 )
	{
		fprintf(stderr, "ERROR: strToChQuantity: Unable to convert string to long number quantity greater than 4\n");
		res = -1;
	}

	return res;
}

int createFifo(char** path)
{
	if(!(*path))
		*path = randFifoName();
	
	int res = (*path != NULL);
	
	mode_t mode = (S_IRUSR | S_IWUSR | S_IRGRP| S_IWGRP | S_IROTH | S_IWOTH);
	if(!(res = (mkfifo(*path, mode) == 0)))
		fprintf(stderr, "ERROR: createFifo: Unable to create fifo file ( path: %s )\n", *path);

	return res;
}

char* randFifoName()
{
	char* res = NULL;
	int fd = open("/dev/urandom", O_RDONLY);
	if(fd != -1)
	{
		srand(time(NULL));
		size_t pathLength = 6 + (rand() % MAX_FIFO_NAME_LEN + 1);
		
		res = (char*)malloc(pathLength);
		if(res)
		{
			memcpy(res, "/tmp/", 5);
			
			do
			{
				if(read(fd, res + 5, pathLength - 5) == -1)
				{	
					fprintf(stderr, "ERROR: randFifoName: Unable to read from /dev/urandom\n");
					free(res);
					res = NULL;
					break;
				}

			}while(!res || !access(res, F_OK));	// repeat until res is not name of existing file;
		}
		else
            fprintf(stderr, "ERROR: randFifoName: Unable to allocate name buffer\n");
		
		close(fd);
	}
	else
		fprintf(stderr, "ERROR: randFifoName: Unable to open descriptor to /dev/urandom\n");
	
	return res;
}

int createChildren(pid_t* pids, long quantity, char* childrenName, char* resourceName, char* fifoPath)
{
	int res = 1;

	for(long i = 0; i < quantity; i++)
	{
		char* name = createChildName(childrenName, i);
		if(!(res = (name != NULL)))
			break;
		
		if((pids[i] = createChild(name, resourceName, fifoPath)) == -1)
		{
			res = 0;
			free(name);
			break;
		}

		free(name);
	}

	return res;
}

pid_t createChild(char* childName, char* resourceName, char* fifoPath)
{
	pid_t res = fork();

	switch(res)
	{
		case -1:
			fprintf(stderr, "ERROR: createChild: Unable to fork process\n");
			break;

		case 0:
			if((res = execl("./poszukiwacz", childName, "-z", resourceName, "-s", fifoPath, (char*)NULL)) == -1)
				fprintf(stderr, "ERROR: createChild: Unable to exec ./poszukiwacz ( child: %s )\n", childName);

			break;

		default:
			printf("created: %s\n", childName);
			break;
	}

	return res;
}

char* createChildName(char* namePrefix, long childNo)
{
	char* res = (char*)malloc(strlen(namePrefix) + (childNo / 10) + 2);
	if(res)
	{
		if(sprintf(res, "%s%ld", namePrefix, childNo) <= 0)
		{
			fprintf(stderr, "ERROR: createChildName: Unable to create name\n");
			free(res);
			res = NULL;
		}
	}
	else
		fprintf(stderr, "ERROR: createChildName: Unable to allocate children name buffer\n");

	return res;
}
