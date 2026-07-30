// Tran, Toan
// Teegela, Hyndavi 
// Pham, Michelle
// Garcia, Natalia 
// Sender & Receiver - Interprocess Communication (Message Queues and Shared Memory)
// C version

#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "msg.h"

/* The size of the shared memory segment */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
static int shmid, msqid;

/* The pointer to the shared memory */
static void *sharedMemPtr = NULL;

/**
 * The function for receiving the name of the file
 * @param fileName - buffer to store the received file name
 * @param maxSize - maximum size of the buffer
 */
void recvFileName(char* fileName, int maxSize)
{
	/* declare an instance of the fileNameMsg struct to be
	 * used for holding the message received from the sender.
	 */
	struct fileNameMsg fileNameMessage;

	/* Receive the file name using msgrcv() */
	if (msgrcv(msqid, &fileNameMessage, sizeof(struct fileNameMsg) - sizeof(long), FILE_NAME_TRANSFER_TYPE, 0) == -1) 
	{
		perror("msgrcv");
		exit(-1);
	}
	
	/* copy the received file name into the output buffer */
	strncpy(fileName, fileNameMessage.fileName, (size_t)(maxSize - 1));
	fileName[maxSize - 1] = '\0';
	printf("Received file name: %s\n", fileName);
}

 /**
  * Sets up the shared memory segment and message queue
  * @param shmid - pointer to the id of the allocated shared memory 
  * @param msqid - pointer to the id of the shared memory
  * @param sharedMemPtr - pointer to the shared memory pointer
  */
void init(int* shmid, int* msqid, void** sharedMemPtr)
{
	printf("Initializing receiver...\n");
	key_t key = ftok("keyfile.txt", 'a');
	if (key == -1) 
	{
		perror("ftok");
		exit(-1);
	}
	
	/* Allocate a shared memory segment. The size of the segment must be SHARED_MEMORY_CHUNK_SIZE. */
	*shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, S_IRUSR | S_IWUSR | IPC_CREAT);
	if (*shmid == -1) 
	{
		perror("shmget");
		exit(-1);
	}
	
	/* Attach to the shared memory */
	*sharedMemPtr = shmat(*shmid, NULL, 0);
	if (*sharedMemPtr == (void*)-1) 
	{
		perror("shmat");
		exit(-1);
	}
	
	/* Create a message queue */
	*msqid = msgget(key, S_IRUSR | S_IWUSR | IPC_CREAT);
	if (*msqid == -1) 
	{
		perror("msgget");
		exit(-1);
	}
}
 
/**
 * The main loop
 * @param fileName - the name of the file received from the sender.
 * @return - the number of bytes received
 */
unsigned long mainLoop(const char* fileName)
{
	/* The size of the message received from the sender */
	int msgSize = -1;
	
	/* The number of bytes received */
	unsigned long numBytesRecv = 0;
	
	/* The string representing the file name received from the sender */
	char recvFileNameStr[256];
	strncpy(recvFileNameStr, fileName, sizeof(recvFileNameStr) - 6);
	recvFileNameStr[sizeof(recvFileNameStr) - 6] = '\0';
	
	/* append __recv to the end of file name */
	strcat(recvFileNameStr, "__recv");
	
	/* Open the file for writing */
	FILE* fp = fopen(recvFileNameStr, "w");
			
	/* Error checks */
	if (!fp)
	{
		perror("fopen");	
		exit(-1);
	}
	printf("Converting file: %s to %s\n", fileName, recvFileNameStr);
	printf("Start receiving data...\n");

	/* Keep receiving until the sender sets the size to 0, indicating that
 	 * there is no more data to send.
 	 */	
	while (msgSize != 0)
	{	
		/* Receive the message and get the value of the size field. */
		struct message rcvMsg;
		if (msgrcv(msqid, &rcvMsg, sizeof(struct message) - sizeof(long), SENDER_DATA_TYPE, 0) == -1)
		{
			perror("msgrcv");
			exit(-1);
		}
		printf("Received message\n");
		
		/* If the sender is not telling us that we are done, then get to work */
		msgSize = rcvMsg.size;

		if (msgSize != 0)
		{
			/* count the number of bytes received */
			numBytesRecv += (unsigned long)msgSize;
			
			/* Save the shared memory to file */
			if (fwrite(sharedMemPtr, sizeof(char), (size_t)msgSize, fp) != (size_t)msgSize)
			{
				perror("fwrite");
			}
			
			/* Tell the sender that we are ready for the next set of bytes. 
 			 * I.e., send a message of type RECV_DONE_TYPE.
 			 */
			struct ackMessage sndMsg;
			sndMsg.mtype = RECV_DONE_TYPE;
			if (msgsnd(msqid, &sndMsg, 0, 0) == -1)
			{
				perror("msgsnd");
				exit(-1);
			}
		}
		/* We are done */
		else
		{
			/* Close the file */
			fclose(fp);
		}
	}
	printf("File transfer completed\n");

	return numBytesRecv;
}

/**
 * Performs cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */
void cleanUp(int shmid, int msqid, void* sharedMemPtr)
{
	/* Detach from shared memory */
	if (shmdt(sharedMemPtr) == -1) 
	{
		perror("shmdt");
		exit(-1);
	}
	
	/* Deallocate the shared memory segment */
	if (shmctl(shmid, IPC_RMID, NULL) == -1) 
	{
		perror("shmctl");
		exit(-1);
	}
	
	/* Deallocate the message queue */
	if (msgctl(msqid, IPC_RMID, NULL) == -1) 
	{
		perror("msgctl");
		exit(-1);
	}
	printf("Successfully cleaned up resources\n");
}

/**
 * Handles the exit signal
 * @param signal - the signal type
 */
void ctrlCSignal(int signal)
{
	(void)signal;

	/* Free system V resources */
	cleanUp(shmid, msqid, sharedMemPtr);
	exit(0);
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	
	/* Install a signal handler. If user presses Ctrl-c, the program
 	 * should delete the message queue and the shared memory segment 
	 * before exiting.
 	 */
	if (signal(SIGINT, ctrlCSignal) == SIG_ERR)
	{
		perror("signal");
		exit(-1);
	} 
			
	/* Initialize */
	init(&shmid, &msqid, &sharedMemPtr);
	
	/* Receive the file name from the sender */
	char fileName[MAX_FILE_NAME_SIZE];
	recvFileName(fileName, MAX_FILE_NAME_SIZE);
	
	/* Go to the main loop */
	fprintf(stderr, "The number of bytes received is: %lu\n", mainLoop(fileName));

	/* Detach from shared memory segment, and deallocate shared memory 
	 * and message queue (i.e. call cleanup) 
	 */
	cleanUp(shmid, msqid, sharedMemPtr);
		
	return 0;
}
