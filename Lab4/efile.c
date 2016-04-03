// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk 
// Jonathan W. Valvano 3/16/11

#include "efile.h"
#include "edisk.h"
#include "string.h"
#include "UART.h"
#include "OS.h"
#define NUMBEROFBLOCKS 100
#define FILESIZEONDIRECTORY 21
#define MAXNUMBEROFFILES (512)/FILESIZEONDIRECTORY //this is for one block
#define FIRSTDATAINDEX 6
#define LASTDATAINDEX 511
struct file Directory [MAXNUMBEROFFILES];
extern struct TCB * RunPt;
void readDirectoryFromDisk(void);
void updateDirectoryToDisk(void);
struct Sema4 semaphorePool[MAXNUMBEROFFILES]; 
struct Sema4 printingMutex5;
struct Sema4 readingMutex4;
struct Sema4 readerMutex3;
struct Sema4 directoryMutex2;
struct Sema4 fileMutex1; //global file mutex... only one file open at a time
unsigned char buff[512];
uint32_t threadIdOpened;
unsigned char buff1[512];
unsigned char buff2[512];
unsigned char buff3[512];
unsigned char buff4[512];
unsigned char buff5[512];

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void) // initialize file system
{
	uint32_t status;
	threadIdOpened = -1;
	if(eDisk_Status(0) == 0) //if already initialized
	{
		return 1;
	}
	OS_InitSemaphore(&printingMutex5, 1);
	OS_InitSemaphore(&readingMutex4,1);
	OS_InitSemaphore(&readerMutex3,1);
	OS_InitSemaphore(&directoryMutex2,1);
	OS_InitSemaphore(&fileMutex1,1); 
	status = eDisk_Init(0);
	if(status != 0)
	{
		return 1;
	}
	readDirectoryFromDisk();
	for(int i = 0; i < MAXNUMBEROFFILES; i++) //asign semaphores. Even to free space file!
	{
		if(Directory[i].valid == 1)
		{
			OS_InitSemaphore(&semaphorePool[i],1);
			Directory[i].semaphorePt = &semaphorePool[i];
		}
	}
	updateDirectoryToDisk();
	return 0;	
}

//ASSUMPTIONS
//Directory[0] will contain the free space available
//First four bytes of each block will contain next address. EXCEPT FOR BLOCK 0 BECAUSE THAT'S THE DIRECTORY (NOT A FILE!)
//This function will fill the directory structure with what it finds on the disk
//-1 will be the same thing as null ((reached end of block))
void readDirectoryFromDisk()
{
	  uint32_t status;
		status = StartCritical();
		eDisk_ReadBlock(buff, 0); //read directory in block 0
		for(int i = 0; i < MAXNUMBEROFFILES; i++)
		{
			Directory[i].valid = buff[i * FILESIZEONDIRECTORY];
			//Construct file name
			for(int j = 0 ; j < 8 ; j ++ )
			{
				Directory[i].name[j] = buff[((i*FILESIZEONDIRECTORY) + 1 ) + j];
			}
			uint32_t tempStartingBlock = 0;
			for(int j = 0 ; j < 4 ; j ++ )
			{
				tempStartingBlock |= (buff[((i*FILESIZEONDIRECTORY) + 9 ) + j] << ((3 - j) * 8));
			}
			Directory[i].startingBlock = tempStartingBlock;
			
			uint32_t tempSize = 0;
			for(int j = 0 ; j < 4 ; j ++ )
			{
				tempSize |= (buff[((i*FILESIZEONDIRECTORY) + 13 ) + j] << ((3 - j) * 8));
			}			
			Directory[i].size = tempSize;
		}
		EndCritical(status);
}

void updateDirectoryToDisk()
{
	  uint32_t status;
		status = StartCritical();
	 // uint32_t status;
		//status = StartCritical();
		for(int i = 0; i < MAXNUMBEROFFILES; i++)
		{
			buff[i * FILESIZEONDIRECTORY] = Directory[i].valid;
			//Construct file name
			for(int j = 0 ; j < 8 ; j ++ )
			{
				 buff[((i*FILESIZEONDIRECTORY) + 1 ) + j] = Directory[i].name[j] ;
			}
			//this writes starting block into the buffer
			buff[9 + i* FILESIZEONDIRECTORY] = (Directory[i].startingBlock >> (24)) & 0xFF;
			buff[10 + i* FILESIZEONDIRECTORY] = (Directory[i].startingBlock >> (16) & 0xFF);
			buff[11 + i* FILESIZEONDIRECTORY] = (Directory[i].startingBlock >> (8) & 0xFF);
			buff[12 + i* FILESIZEONDIRECTORY] = (Directory[i].startingBlock & 0xFF);
			//this writes the size of each file
			buff[13 + i* FILESIZEONDIRECTORY] = (Directory[i].size >> (24)) & 0xFF;
			buff[14 + i* FILESIZEONDIRECTORY] = (Directory[i].size >> (16) & 0xFF);
			buff[15 + i* FILESIZEONDIRECTORY] = (Directory[i].size >> (8) & 0xFF);
			buff[16 + i* FILESIZEONDIRECTORY] = (Directory[i].size & 0xFF);				
		}
		eDisk_WriteBlock(buff, 0);
		EndCritical(status);
}

//returns the next block integer of a given block number
uint32_t getNextBlockNumber(uint32_t blockNumber)
{
			
			eDisk_ReadBlock(buff1, blockNumber);
			uint32_t tempNextBlock = 0;
			for(int j = 0 ; j < 4 ; j ++ )
			{
					tempNextBlock |= (buff1[j] << ((3 - j) * 8));
			}
			return tempNextBlock;
}

//returns the next block integer of a given block number
uint32_t getBufferNextBlockNumber(unsigned char buff[])
{
			uint32_t tempNextBlock = 0;
			for(int j = 0 ; j < 4 ; j ++ )
			{
					tempNextBlock |= (buff[j] << ((3 - j) * 8));
			}
			return tempNextBlock;
}

//returns the number of bytes of data in block number
// max data in block: 512 - 6 = 506 in each block..
//buff[6] -> buff[511]
/*
uint16_t getBlockDataSize(uint32_t blockNumber)
{
			unsigned char buff [512];
			eDisk_ReadBlock(buff, blockNumber);
			uint16_t tempSize = 0;
			tempSize = buff[4] << 8;
			tempSize |= buff[5];
			return tempSize;
}
*/
uint16_t getBufferDataSize(unsigned char buff[])
{
			uint16_t tempSize = 0;
			tempSize = buff[4] << 8;
			tempSize |= buff[5];
			return tempSize;
}

/*
uint16_t setBlockDataSize(uint32_t blockNumber, uint16_t size)
{
			unsigned char buff [512];
			eDisk_ReadBlock(buff, blockNumber);
			buff[4] = (size>>8)&0xFF;
			buff[5] = (size) &0xFF;
			eDisk_WriteBlock(buff,blockNumber);
			return 0;
}
*/
uint16_t setBufferDataSize(unsigned char buff [], uint16_t size)
{
			buff[4] = (size>>8)&0xFF;
			buff[5] = (size) &0xFF;
			return 0;
}


//this function traverses until the end of a linked list of the passed file, and returns the last block number that points to null
uint32_t traverseUntilTheEnd(struct file * toTraverse)
{
		uint32_t nextBlockNumber = toTraverse->startingBlock;
		uint32_t currentBlockNumber;
		eDisk_ReadBlock(buff2, nextBlockNumber); //temp contains the starting block of the free space
		//WE KNOW THE NEXT BLOCK ADDRESS ARE THE FIRST FOUR BYTES
		while(nextBlockNumber != -1)
		{
			eDisk_ReadBlock(buff2, nextBlockNumber); //temp contains the starting block of the free space
			uint32_t tempNextBlock = getBufferNextBlockNumber(buff2);
			currentBlockNumber = nextBlockNumber; //before we move on. store where you are
			nextBlockNumber = tempNextBlock;			
		}
		return currentBlockNumber;
}

//this function appends the starting block a file to the given block number (e.g. last of free space to start of file)
//the starting block of the file is taken from the index of the directory passed as a parameter
void appendFirstBlockOfFileToBlock(uint32_t blockNumber, uint32_t fileIndex)
{
			eDisk_ReadBlock(buff3, blockNumber);
			buff3[0] = (Directory[fileIndex].startingBlock >> (24)) & 0xFF;
			buff3[1] = (Directory[fileIndex].startingBlock >> (16) & 0xFF);
			buff3[2] = (Directory[fileIndex].startingBlock >> (8) & 0xFF);
			buff3[3] = (Directory[fileIndex].startingBlock & 0xFF);
			eDisk_WriteBlock(buff3, blockNumber);
}

//reads block, changes the next block number, and writes it back into disk
void appendBlock(uint32_t blockNumber, uint32_t blockNumberToAdd)
{
			eDisk_ReadBlock(buff4, blockNumber);
			buff4[0] = (blockNumberToAdd >> (24)) & 0xFF;
			buff4[1] = (blockNumberToAdd >> (16)) & 0xFF;
			buff4[2] = (blockNumberToAdd >> (8)) & 0xFF;
			buff4[3] = blockNumberToAdd & 0xFF;
			eDisk_WriteBlock(buff4, blockNumber);
		
}

//get the first free space node, and point to null
//returns a free node pointing to null..
uint32_t getFreeBlock()
{
			//get next free space from Directory[0] and append it to file being created
			uint32_t nextBlock = getNextBlockNumber(Directory[0].startingBlock); //provides next block after first block of starting free space
																																					 //will be used to remove node from linked list and append next
			uint32_t temp = Directory[0].startingBlock;
			Directory[0].startingBlock = nextBlock;
			setBufferDataSize(buff5,0);
			eDisk_WriteBlock(buff5,temp);
			appendBlock(temp, -1); //point it to null
			updateDirectoryToDisk();
			return temp;
}
//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
// need to check for errors
int eFile_Format(void) // erase disk, add format
{
	readDirectoryFromDisk(); //copy from disk to runtime memory
	if(Directory[0].valid == 1) //if free space file is valid, this file system was initialized before
	{
		uint32_t lastFreeBlockNumber;
		for(int i = 1 ;  i< MAXNUMBEROFFILES ; i++ )
		{
			Directory[i].valid = 0; //kill file
			//lastFreeBlockNumber = traverseUntilTheEnd(&Directory[0]); //traverses the empty until the end
			//appendFirstBlockOfFileToBlock(lastFreeBlockNumber, i);
		}
		Directory[0].startingBlock = 1;
		for(int i = 1; i < NUMBEROFBLOCKS - 1 ; i ++)
		{
			appendBlock(i, i+1);
		}
		appendBlock(NUMBEROFBLOCKS -1, -1);

	}
	else //need to create new directory
	{
		Directory[0].valid = 1; //initialize free space
		strcpy(Directory[0].name,"free");
		Directory[0].startingBlock = 1;
		for(int i = 1; i < NUMBEROFBLOCKS - 1 ; i ++)
		{
			appendBlock(i, i+1);
		}
		appendBlock(NUMBEROFBLOCKS -1, -1);
	}
	updateDirectoryToDisk();
	return 0;
}

//this function searches for a file name in the directroy, 
//output: 0 if not found (succesful)
//				-1 if found
uint32_t findDuplicateIndex(char toSearch[])
{
	for(int j = 1; j< MAXNUMBEROFFILES; j++)
	{
		if(Directory[j].valid == 1)
		{
			if(strcmp(Directory[j].name, toSearch) == 0)
			{
				return j; //duplicate
			}
		}
	}
	return -1; //no duplicate
}

//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[])  // create new file, make it empty 
{
	//file name doesnt exist, so create
	//since format was called before this function, directory is the most recent one
	for(int i = 1; i < MAXNUMBEROFFILES; i++)
	{
		OS_Wait(&directoryMutex2);
		if(Directory[i].valid != 1) //place the file in this empty slot
		{
			if(findDuplicateIndex(name) != -1)
			{
				OS_Signal(&directoryMutex2);
				return 1;
			}
			Directory[i].valid = 1;
			OS_Signal(&directoryMutex2);
			strcpy(Directory[i].name,name);
			//get next free space from Directory[0] and append it to file being created
			Directory[i].startingBlock = getFreeBlock();
			Directory[i].size = 0; //size in bytes of data...
			//now create semaphore
			OS_InitSemaphore(&semaphorePool[i],1);
			Directory[1].semaphorePt = &semaphorePool[i];
			//now release semaphore
			return 0;
		}
		OS_Signal(&directoryMutex2);
	}
	return 1; //no empty file found
}

//will handle the global file opened
//if -1, no files are open
uint32_t fileOpenIndex = -1;
unsigned char openedFileBuffer[512];
uint32_t lastOpenedBlock;
//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a seven char ASCII word
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[])      // open a file for writing 
{
	OS_Wait(&fileMutex1); //global sema for files.. only one open at a time
	OS_Wait(&directoryMutex2);
	uint32_t fileIndex = findDuplicateIndex(name);
	OS_Signal(&directoryMutex2);
	if(fileIndex != -1 && threadIdOpened != RunPt->id) //file exists
	{
		fileOpenIndex = fileIndex;
		//read last block into openedFileBuffer
		uint32_t lastBlock = traverseUntilTheEnd(&Directory[fileOpenIndex]);
		eDisk_ReadBlock(openedFileBuffer,lastBlock);
		lastOpenedBlock = lastBlock;
		threadIdOpened = RunPt->id;
		return 0;
	}
	return 1;
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void) // close the file for writing
{
	if(fileOpenIndex == -1)
	{
		return 1;
	}
	eDisk_WriteBlock(openedFileBuffer, lastOpenedBlock);
	updateDirectoryToDisk();
	fileOpenIndex = -1;
	lastOpenedBlock = -1;
	threadIdOpened = -1;
	OS_Signal(&fileMutex1);
	return 0;
}

/* WARNING ******************************************************************
	 FIRST FOUR BYTES ARE FOR NEXT BLOCK
   NEXT TWO BYTES ARE FOR SIZE
   USABLE INDECES FOR DATA ARE buff[6] -> buff[511]
****************************************************************************/

//---------- eFile_Write-----------------
// save at end of the open file
// latest block stored in openedFileBuffer...
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data)
{
	uint16_t blockSize = getBufferDataSize(openedFileBuffer);
	if(blockSize > LASTDATAINDEX) //need to make new block
	{
		uint32_t tempNext = getFreeBlock();
		appendBlock(lastOpenedBlock,tempNext);
		eDisk_WriteBlock(openedFileBuffer, lastOpenedBlock); //save block before moving on
		lastOpenedBlock = tempNext;
		eDisk_ReadBlock(openedFileBuffer,lastOpenedBlock);
		blockSize = 0;
	}
	openedFileBuffer[ (blockSize + FIRSTDATAINDEX) ] = data;
	setBufferDataSize(openedFileBuffer, ++blockSize); 
	Directory[fileOpenIndex].size++;
	return 0;
}

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void); 


uint32_t numberOfThreadsReading;
uint32_t nextByteToReadIndex = FIRSTDATAINDEX;
uint32_t totalBytesRead = 0;
//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[])      // open a file for reading 
{
	OS_Wait(&readerMutex3);
	numberOfThreadsReading++;
	if(numberOfThreadsReading == 1)
	{
		OS_Wait(&fileMutex1);
	}

	OS_Wait(&directoryMutex2);
	uint32_t fileIndex = findDuplicateIndex(name);
	OS_Signal(&directoryMutex2);
	if(fileIndex != -1 && threadIdOpened != RunPt->id) //file exists
	{
		fileOpenIndex = fileIndex;
		//read last block into openedFileBuffer
		eDisk_ReadBlock(openedFileBuffer,Directory[fileOpenIndex].startingBlock);
		lastOpenedBlock = Directory[fileOpenIndex].startingBlock;
		threadIdOpened = RunPt->id;
		OS_Signal(&readerMutex3);
		return 0; //file exists!
	}
	OS_Signal(&readerMutex3);
	return 1; //file did not exist
}

//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void) // close the file for writing
{
	OS_Wait(&readerMutex3);
	if(fileOpenIndex == -1)
	{
		OS_Signal(&readerMutex3);
		return 1;
	}
	numberOfThreadsReading--;
	if(numberOfThreadsReading == 0)
	{
		fileOpenIndex = -1;
		lastOpenedBlock = -1;
		threadIdOpened = -1;
		nextByteToReadIndex = FIRSTDATAINDEX;
		OS_Signal(&fileMutex1);
	}
	OS_Signal(&readerMutex3);
	return 0;
}

/*
//will handle the global file opened
//if -1, no files are open
uint32_t fileOpenIndex;
unsigned char openedFileBuffer[512];
uint32_t lastOpenedBlock;
*/

//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt)       // get next byte  
{
	OS_Wait(&readingMutex4);
	if(totalBytesRead == Directory[fileOpenIndex].size)
	{
		totalBytesRead = 0;
		nextByteToReadIndex = FIRSTDATAINDEX;
		OS_Signal(&readingMutex4);
		return 1;
	}
	
	*pt = openedFileBuffer[nextByteToReadIndex++];
	totalBytesRead++;
	if(nextByteToReadIndex==512) //if read last one before
	{
		lastOpenedBlock = getBufferNextBlockNumber(openedFileBuffer);
		eDisk_ReadBlock(openedFileBuffer,lastOpenedBlock);
		nextByteToReadIndex = FIRSTDATAINDEX;
	}
	OS_Signal(&readingMutex4);
	return 0;
}
//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char))
{
	uint32_t filesPrinted = 0;
	OS_Wait(&directoryMutex2);
	for(int i = 1; i<MAXNUMBEROFFILES; i++)
	{
		if(Directory[i].valid == 1)
		{
			UART_OutChar(CR);
			UART_OutChar(LF);

			filesPrinted++;
			for(int j = 0; j<8; j++)
			{
				if(Directory[i].name[j] == '\0')
				{
					break;
				}
				fp(Directory[i].name[j]); 
			}
			fp(' ');
			UART_OutUDec(Directory[i].size);
			UART_OutChar(CR);
			UART_OutChar(LF);
		}
	}
	if(filesPrinted == 0)
	{
		UART_OutChar(CR);
		UART_OutChar(LF);
		fp('E');
		fp('M');
		fp('P');
		fp('T');
		fp('Y');
		UART_OutChar(CR);
		UART_OutChar(LF);
	}
	OS_Signal(&directoryMutex2);
	return 0;
}

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char name[])  // remove this file 
{
	OS_Wait(&directoryMutex2);
	uint32_t toDeleteIndex = findDuplicateIndex(name);
	if(toDeleteIndex == -1)
	{
		OS_Signal(&directoryMutex2);
		return 1; //file doesnt exist
	}
	Directory[toDeleteIndex].valid = 0;
	uint32_t lastFree = traverseUntilTheEnd(&Directory[0]);
	appendFirstBlockOfFileToBlock(lastFree, toDeleteIndex);
	Directory[toDeleteIndex].startingBlock = -1;
	OS_Signal(&directoryMutex2);
	return 0;
}
int StreamToFile=0; // 0=UART, 1=stream to file
//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name)
{
	 eFile_Create(name); // ignore error if file already exists
	 if(eFile_WOpen(name)) return 1; // cannot open file
	 StreamToFile = 1;
	 return 0;
}

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void)
{
 StreamToFile = 0;
 if(eFile_WClose()) return 1; // cannot close file
 return 0;
}
