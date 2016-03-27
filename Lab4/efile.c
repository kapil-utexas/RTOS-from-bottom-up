// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk 
// Jonathan W. Valvano 3/16/11

#include "efile.h"
#include "edisk.h"
#include "string.h"
#define MAXNUMBEROFFILES 10
#define NUMBEROFBLOCKS 100
//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating
int eFile_Init(void) // initialize file system
{
	return eDisk_Init(0);	
}
struct file Directory [MAXNUMBEROFFILES];
//ASSUMPTIONS
//Directory[0] will contain the free space available
//First four bytes of each block will contain next address. EXCEPT FOR BLOCK 0 BECAUSE THAT'S THE DIRECTORY (NOT A FILE!)
//This function will fill the directory structure with what it finds on the disk
//-1 will be the same thing as null ((reached end of block))
void readDirectoryFromDisk()
{
		unsigned char buff[512];
		eDisk_ReadBlock(buff, 0); //read directory in block 0
		for(int i = 0; i < MAXNUMBEROFFILES; i++)
		{
			Directory[i].valid = buff[i * 17];
			//Construct file name
			for(int j = 0 ; j < 8 ; j ++ )
			{
				Directory[i].name[j] = buff[((i*17) + 1 ) + j];
			}
			uint32_t tempStartingBlock = 0;
			for(int j = 0 ; j < 3 ; j ++ )
			{
				tempStartingBlock = (buff[((i*17) + 9 ) + j] << ((3 - j) * 8));
			}
			Directory[i].startingBlock = tempStartingBlock;
			
			uint32_t tempSize = 0;
			for(int j = 0 ; j < 3 ; j ++ )
			{
				tempSize = (buff[((i*17) + 13 ) + j] << ((3 - j) * 8));
			}			
			Directory[i].size = tempSize;
		}
}

void updateDirectoryToDisk()
{
		unsigned char buff[512];
		for(int i = 0; i < MAXNUMBEROFFILES; i++)
		{
			buff[i * 17] = Directory[i].valid;
			//Construct file name
			for(int j = 0 ; j < 8 ; j ++ )
			{
				 buff[((i*17) + 1 ) + j] = Directory[i].name[j] ;
			}
			//this writes starting block into the buffer
			buff[9 + i* 17] = (Directory[i].startingBlock >> (24)) & 0xFF;
			buff[10 + i* 17] = (Directory[i].startingBlock >> (16) & 0xFF);
			buff[11 + i* 17] = (Directory[i].startingBlock >> (8) & 0xFF);
			buff[12 + i* 17] = (Directory[i].startingBlock & 0xFF);
			//this writes the size of each file
			buff[13 + i* 17] = (Directory[i].size >> (24)) & 0xFF;
			buff[14 + i* 17] = (Directory[i].size >> (16) & 0xFF);
			buff[15 + i* 17] = (Directory[i].size >> (8) & 0xFF);
			buff[16 + i* 17] = (Directory[i].size & 0xFF);				
		}
		eDisk_WriteBlock(buff, 0);
}
//this function traverses until the end of a linked list of the passed file, and returns the last node that points to null
uint32_t traverseUntilTheEnd(struct file * toTraverse)
{
		uint32_t nextBlockNumber = toTraverse->startingBlock;
		uint32_t currentBlockNumber;
		unsigned char buff[512];
		eDisk_ReadBlock(buff, nextBlockNumber); //temp contains the starting block of the free space
		//WE KNOW THE NEXT BLOCK ADDRESS ARE THE FIRST FOUR BYTES
		while(nextBlockNumber != -1)
		{
			eDisk_ReadBlock(buff, nextBlockNumber); //temp contains the starting block of the free space
			uint32_t tempNextBlock = 0;
			for(int j = 0 ; j < 3 ; j ++ )
			{
					tempNextBlock = (buff[j] << ((3 - j) * 8));
			}
			currentBlockNumber = nextBlockNumber; //before we move on. store where you are
			nextBlockNumber = tempNextBlock;			
		}
		return currentBlockNumber;
}

//this function appends the starting block a file to the given block number (e.g. last of free space to start of file)
//the starting block of the file is taken from the index of the directory passed as a parameter
void appendFileToLastBlock(uint32_t blockNumber, uint32_t fileIndex)
{			unsigned char buff[512];
			eDisk_ReadBlock(buff, blockNumber);
			buff[0] = (Directory[fileIndex].startingBlock >> (24)) & 0xFF;
			buff[1] = (Directory[fileIndex].startingBlock >> (16) & 0xFF);
			buff[2] = (Directory[fileIndex].startingBlock >> (8) & 0xFF);
			buff[3] = (Directory[fileIndex].startingBlock & 0xFF);
			eDisk_WriteBlock(buff, blockNumber);
}

void appendBlock(uint32_t blockNumber, uint32_t blockNumberToAdd)
{
			unsigned char buff[512];
			buff[0] = (blockNumberToAdd >> (24)) & 0xFF;
			buff[1] = (blockNumberToAdd >> (16)) & 0xFF;
			buff[2] = (blockNumberToAdd >> (8)) & 0xFF;
			buff[3] = blockNumberToAdd & 0xFF;
			eDisk_WriteBlock(buff, blockNumber);
		
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
			lastFreeBlockNumber = traverseUntilTheEnd(&Directory[0]); //traverses the empty until the end
			appendFileToLastBlock(lastFreeBlockNumber, i);
		}
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
//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[]);  // create new file, make it empty 


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[]);      // open a file for writing 

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data);  

//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void); 


//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void); // close the file for writing

//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[]);      // open a file for reading 
   
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt);       // get next byte 
                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void); // close the file for writing

//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char));   

//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char name[]);  // remove this file 

//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name);

//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void);
