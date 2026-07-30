// Tran, Toan
// Teegela, Hyndavi 
// Pham, Michelle
// Garcia, Natalia 
// Sender & Receiver - Interprocess Communication (Message Queues and Shared Memory)
// C version

#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
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
static void* sharedMemPtr;

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - pointer to the id of the allocated shared memory 
 * @param msqid - pointer to the id of the allocated message queue
 * @param sharedMemPtr - pointer to the shared memory pointer
 */
void init(int* shmid, int* msqid, void** sharedMemPtr)
{
	key_t key = ftok("keyfile.txt", 'a');
	if (key == -1) 
	{
		perror("ftok");
		exit(-1);
	}

	*shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, S_IRUSR | S_IWUSR);
	if (*shmid == -1) 
	{
		perror("shmget");
		exit(-1);
	}

	*sharedMemPtr = shmat(*shmid, NULL, 0);
	if (*sharedMemPtr == (void*)-1) 
	{
		perror("shmat");
		exit(-1);
	}

	*msqid = msgget(key, S_IRUSR | S_IWUSR);
	if (*msqid == -1) 
	{
		perror("msgget");
		exit(-1);
	}
	printf("Shared memory and message queue initialized successfully\n");
}

/**
 * Performs the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */
void cleanUp(int shmid, int msqid, void* sharedMemPtr)
{
	(void)shmid;
	(void)msqid;

	if (shmdt(sharedMemPtr) == -1) 
	{
		perror("shmdt");
		exit(-1);
	}
	printf("Successfully cleanup from shared memory\n");
}

/**
 * The main send function
 * @param fileName - the name of the file
 * @return - the number of bytes sent
 */
unsigned long sendFile(const char* fileName)
{
	/* A buffer to store message we will send to the receiver. */
	struct message sndMsg; 
	
	/* A buffer to store message received from the receiver. */
	struct ackMessage rcvMsg;
		
	/* The number of bytes sent */
	unsigned long numBytesSent = 0;
	
	/* Open the file */
	FILE* fp = fopen(fileName, "r");

	/* Was the file open? */
	if (!fp)
	{
		perror("fopen");
		exit(-1);
	}
	
	/* Read the whole file */
	while (!feof(fp))
	{
		printf("Reading from file: %s\n", fileName);
		printf("Start sending data...\n");
		
		sndMsg.size = (int)fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp);
		if (sndMsg.size < 0)
		{
			perror("fread");
			exit(-1);
		}
		
		/* count the number of bytes sent */		
		numBytesSent += sndMsg.size;
			
		/* Send a message to the receiver telling him that the data is ready
 		 * to be read (message of type SENDER_DATA_TYPE).
 		 */
		sndMsg.mtype = SENDER_DATA_TYPE;
		if (msgsnd(msqid, &sndMsg, sizeof(struct message) - sizeof(long), 0) == -1)
		{
			perror("msgsnd");
			exit(-1);
		}

		/* Wait until the receiver sends us a message of type RECV_DONE_TYPE telling us 
 		 * that he finished saving a chunk of memory. 
 		 */
		if (msgrcv(msqid, &rcvMsg, sizeof(struct ackMessage) - sizeof(long), RECV_DONE_TYPE, 0) == -1)
		{
			perror("msgrcv");
			exit(-1);
		}
		printf("File chunk sent, waiting for receiver to finish saving...\n");
	}
	
	/* Once we are out of the above loop, we have finished sending the file.
 	 * Lets tell the receiver that we have nothing more to send. We will do this by
 	 * sending a message of type SENDER_DATA_TYPE with size field set to 0. 	
	 */
	sndMsg.mtype = SENDER_DATA_TYPE;
	sndMsg.size = 0;

	if (msgsnd(msqid, &sndMsg, sizeof(struct message) - sizeof(long), 0) == -1)
	{
		perror("msgsnd");
		exit(-1);
	}

	/* Close the file */
	fclose(fp);
	printf("File transfer completed\n");
	
	return numBytesSent;
}

/**
 * Used to send the name of the file to the receiver
 * @param fileName - the name of the file to send
 */
void sendFileName(const char* fileName)
{
	printf("Sending file name: %s\n", fileName);

	/* Get the length of the file name */
	int fileNameSize = (int)strlen(fileName);

	/* Make sure the file name does not exceed 
	 * the maximum buffer size in the fileNameMsg
	 * struct. If exceeds, then terminate with an error.
	 */
	if (fileNameSize > MAX_FILE_NAME_SIZE) 
	{
		fprintf(stderr, "File name exceeds maximum size of %d characters.\n", MAX_FILE_NAME_SIZE);
		exit(-1);
	}

	/* Create an instance of the struct representing the message
	 * containing the name of the file.
	 */
	struct fileNameMsg fileNameMessage;

	/* Set the message type FILE_NAME_TRANSFER_TYPE */
	fileNameMessage.mtype = FILE_NAME_TRANSFER_TYPE;

	/* Set the file name in the message */
	strncpy(fileNameMessage.fileName, fileName, (size_t)fileNameSize);
	fileNameMessage.fileName[fileNameSize] = '\0';
	
	/* Send the message using msgsnd */
	if (msgsnd(msqid, &fileNameMessage, sizeof(struct fileNameMsg) - sizeof(long), 0) == -1) 
	{
		perror("msgsnd");
		exit(-1);
	}
	printf("File name sent successfully\n");
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	
	/* Check the command line arguments */
	if (argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}
		
	/* Connect to shared memory and the message queue */
	init(&shmid, &msqid, &sharedMemPtr);
	
	/* Send the name of the file */
	sendFileName(argv[1]);
		
	/* Send the file */
	fprintf(stderr, "The number of bytes sent is %lu\n", sendFile(argv[1]));
	
	/* Cleanup */
	cleanUp(shmid, msqid, sharedMemPtr);
		
	return 0;
}
