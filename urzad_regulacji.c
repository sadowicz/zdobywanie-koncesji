#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500

#define DEFAULT_CH_QUANT 16
#define MAX_FIFO_NAME_LEN 20

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

int parseArgs(int argc, char** argv, double* time, long* childrenQuantity, char** resourceName, char** childName, char** fifoPath);

double strToMillisec(char* str);
long strToChQuantity(char* str);

int createFifo(char** path);
char* randFifoName();

int manageChildren(long time, long childrenQuant, char* childName, char* resourceName, char* fifoPath, struct timespec* fifoClosureTime);

int createPipe(int pipefd[2]);

int createChildren(pid_t* pids, long quantity, char* childrenName, char* resourceName, char* fifoPath, int pipeWrite);
pid_t createChild(char* childName, char* resourceName, char* fifoPath, int pipeWrite);
char* createChildName(char* namePrefix, long childNo);

int procSleep(double time);

int endChildren(char* fifo, pid_t* pids, long childrenQuantity, struct timespec* fifoClosureTime);
int waitChildren(pid_t* pids, long quantity);

int cleanUp(char* childrenName, struct timespec* fifoClosureTime);
int tryDelete(char*fileName, char* childrenName, struct timespec* fifoClosureTime);
int timespecLater(struct timespec* t1, struct timespec* t2); // check if t1 later than t2

int main(int argc, char* argv[])
{
	int exitStatus = EXIT_SUCCESS;

	if(!(exitStatus = !(argc >= 7 || argc <= 10)))
	{	
		double time = 0;
		long childrenQuant = 0;

		char* resourceName = NULL;
		char* childName = NULL;
		char* fifoPath = NULL;

		if(!(exitStatus = !parseArgs(argc, argv, &time, &childrenQuant, &resourceName, &childName, &fifoPath) || !resourceName || !childName))
		{
			if(childrenQuant <= 4)
				childrenQuant = DEFAULT_CH_QUANT;

			if(!(exitStatus = !createFifo(&fifoPath)))
			{	
				struct timespec fifoClosureTime;

				if(!(exitStatus = !manageChildren(time, childrenQuant, childName, resourceName, fifoPath, &fifoClosureTime)))
					exitStatus = !cleanUp(childName, &fifoClosureTime);
			}
		}
	}
	else
		printf("USAGE: %s -t <time> -z <resource_name> -p <children_name> [-n <children_quantity>] [<fifo_path>]\n", argv[0]);

	return exitStatus;
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

int manageChildren(long time, long childrenQuant, char* childName, char* resourceName, char* fifoPath, struct timespec* fifoClosureTime)
{
	int res = 1;

	int pipeRW[2];
	if((res = createPipe(pipeRW)))
	{		
		pid_t* pids = (pid_t*)malloc(childrenQuant * sizeof(pid_t));
		if((res = (pids != NULL)))
		{
			if((res = createChildren(pids, childrenQuant, childName, resourceName, fifoPath, pipeRW[1])))
			{
				close(pipeRW[0]);	// close read end of pipe
				close(pipeRW[1]);	// close write end of pipe - unlocks children processes
				
				if((res = procSleep(time)))
					res = endChildren(fifoPath, pids, childrenQuant, fifoClosureTime);
			}

			free(pids);
		}
		else
			fprintf(stderr, "ERROR: manageChildren: Unable to allocate children pids array\n");
	}
	
	return res;
}

int createPipe(int pipefd[2])
{
	int res = 1;
	if((res = (!pipe(pipefd))))
	{
		if(pipefd[1] == 4)
		{
			pipefd[1] = dup(pipefd[1]);
			
			if(!(res = (pipefd[1] != -1)))
				fprintf(stderr, "ERROR: createPipe: Unable to dup pipe write\n");
		}
		
		if(res)
		{
			pipefd[0] = dup2(pipefd[0], 4);
		
			if(!(res = (pipefd[0] != -1)))
				fprintf(stderr, "ERROR: createPipe: Unable to dup pipe read descriptor to 4\n");
		}
	}
	else
		fprintf(stderr, "ERROR: createPipe: Unable to create pipe\n");
	
	return res;
}

int createChildren(pid_t* pids, long quantity, char* childrenName, char* resourceName, char* fifoPath, int pipeWrite)
{
	int res = 1;

	for(long i = 0; i < quantity; i++)
	{
		char* name = createChildName(childrenName, i);
		if(!(res = (name != NULL)))
			break;
		
		if((pids[i] = createChild(name, resourceName, fifoPath, pipeWrite)) == -1)
		{
			res = 0;
			free(name);
			break;
		}

		free(name);
	}

	return res;
}

pid_t createChild(char* childName, char* resourceName, char* fifoPath, int pipeWrite)
{
	pid_t res = fork();

	switch(res)
	{
		case -1:
			fprintf(stderr, "ERROR: createChild: Unable to fork process\n");
			break;

		case 0:
			close(pipeWrite);

			if((res = execl("./poszukiwacz", childName, "-z", resourceName, "-s", fifoPath, (char*)NULL)) == -1)
				fprintf(stderr, "ERROR: createChild: Unable to exec ./poszukiwacz ( child: %s )\n", childName);

			break;

		default:
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

int procSleep(double time)
{
	long n_sleeptime = time * 1000000;
	struct timespec t = { .tv_sec = n_sleeptime / 1000000000, .tv_nsec = n_sleeptime % 1000000000 };

	int res = nanosleep(&t, NULL);
	if(res == -1)
		fprintf(stderr, "ERROR: procSleep: Unable to sleep for given time ( time: %f ms )\n", time);

	return !res;
}

int endChildren(char* fifo, pid_t* pids, long childrenQuantity, struct timespec* fifoClosureTime)
{
	int res = 1;
	
	int fd = open(fifo, O_RDONLY);
	if((res = (clock_gettime(CLOCK_REALTIME, fifoClosureTime) != -1)))
	{		
		if((res = (fd != -1)))
		{

			res = waitChildren(pids, childrenQuantity);
			close(fd);
		}
		else
			fprintf(stderr, "ERROR: endInformChildren: Unable to open fifo for read\n");
	}
	else
		fprintf(stderr, "ERROR: endInformChildren: Unable to getFifoClosureTime\n");

	return res;
}

int waitChildren(pid_t* pids, long quantity)
{
	int res = 1;
	
	for(long i = 0; i < quantity; i++)
	{
		if(waitid(P_PID, pids[i], NULL, WEXITED | WSTOPPED) == -1)
		{	
			fprintf(stderr, "ERROR: waitChildren: Waiting failure\n");
			res = 0;
			break;
		}
	}

	return res;
}

int cleanUp(char* childrenName, struct timespec* fifoClosureTime)
{
	int res = 1;

	struct dirent* entry;
	DIR* current = opendir(".");
	if((res = (current != NULL)))
	{
		while((entry = readdir(current)) != NULL)
		{
			if(!(res = tryDelete(entry->d_name, childrenName, fifoClosureTime)))
				break;
		}

		closedir(current);
	}
	else
		fprintf(stderr, "ERROR: cleanUP: Unable to open current working directory\n");

	return res;
}

int tryDelete(char*fileName, char* childrenName, struct timespec* fifoClosureTime)
{
	int res = 1;

	if(!strncmp(fileName, "property", 8))
	{
		if(!(res = (remove(fileName) != -1)))
			fprintf(stderr, "ERROR: tryDelete: Unable to remove file\n");
	}
	else if(!strncmp(fileName, childrenName, strlen(childrenName)))
	{
		struct stat fileStat;
		if((res = (stat(fileName, &fileStat) != -1)))
		{
			if(timespecLater(&fileStat.st_atim, fifoClosureTime))
			{	
				if(!(res = (remove(fileName) != -1)))
					fprintf(stderr, "ERROR: tryDelete: Unable to remove file\n");
			}
		}
		else
			fprintf(stderr, "ERROR: tryDelete: Unable to get file informaion\n");
	}

	return res;
}

int timespecLater(struct timespec* t1, struct timespec* t2)
{
	int res = 0;

	if(t1->tv_sec > t2->tv_sec || (t1->tv_sec == t2->tv_sec && t1->tv_nsec > t2->tv_nsec))
		res = 1;

	return res;
}
