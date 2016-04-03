//*****************************************************************************
//
// Lab4.c - user programs, File system, stream data onto disk
// Jonathan Valvano, March 16, 2011, EE345M
//     You may implement Lab 4 without the LCD display
//*****************************************************************************
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
#include <stdio.h>
#include <string.h>
#include "inc/hw_types.h"
#include "inc/tm4c123gh6pm.h"
#include "UART.h"
#include "ADC.h"
#include "OS.h"
#include "edisk.h"
#include "efile.h"

unsigned long NumCreated;   // number of foreground threads created
unsigned long NumSamples;   // incremented every sample
unsigned long DataLost;     // data sent by Producer, but not received by Consumer

int Running;                // true while robot is running

#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

#define GPIO_PF0  (*((volatile unsigned long *)0x40025004))
#define GPIO_PF1  (*((volatile unsigned long *)0x40025008))
#define GPIO_PF2  (*((volatile unsigned long *)0x40025010))
#define GPIO_PF3  (*((volatile unsigned long *)0x40025020))
#define GPIO_PG1  (*((volatile unsigned long *)0x40026008))
// PF1/IDX1 is user input select switch
// PE1/PWM5 is user input down switch 
// PF0/PWM0 is debugging output on Systick
// PF2/LED1 is debugging output 
// PF3/LED0 is debugging output 
// PG1/PWM1 is debugging output 


//******** Robot *************** 
// foreground thread, accepts data from producer
// inputs:  none
// outputs: none
void Robot(void){   
unsigned long data;      // ADC sample, 0 to 1023
unsigned long voltage;   // in mV,      0 to 3000
unsigned long time;      // in 10msec,  0 to 1000 
unsigned long t=0;
  OS_ClearMsTime();    
  DataLost = 0;          // new run with no lost data 
  //printf("Robot running...");
  eFile_RedirectToFile("Robot");
  //printf("time(sec)\tdata(volts)\n\r");
  do{
    t++;
    time=OS_MsTime();            // 10ms resolution in this OS
    data = OS_Fifo_Get();        // 1000 Hz sampling get from producer
    voltage = (300*data)/1024;   // in mV
    //printf("%0u.%02u\t%0u.%03u\n\r",time/100,time%100,voltage/1000,voltage%1000);
  }
  while(time < 1000);       // change this to mean 10 seconds
  eFile_EndRedirectToFile();
  //printf("done.\n\r");
  Running = 0;                // robot no longer running
  OS_Kill();
}
  
//************ButtonPush*************
// Called when Select Button pushed
// background threads execute once and return
void ButtonPush(void){
  if(Running==0){
    Running = 1;  // prevents you from starting two robot threads
    NumCreated += OS_AddThread(&Robot,128,1);  // start a 20 second run
  }
}
//************DownPush*************
// Called when Down Button pushed
// background threads execute once and return
void DownPush(void){

}



//******** Producer *************** 
// The Producer in this lab will be called from your ADC ISR
// A timer runs at 1 kHz, started by your ADC_Collect
// The timer triggers the ADC, creating the 1 kHz sampling
// Your ADC ISR runs when ADC data is ready
// Your ADC ISR calls this function with a 10-bit sample 
// sends data to the Robot, runs periodically at 1 kHz
// inputs:  none
// outputs: none
void Producer(unsigned short data){  
  if(Running){
    if(OS_Fifo_Put(data)){     // send to Robot
      NumSamples++;
    } else{ 
      DataLost++;
    } 
  }
}
 
//******** IdleTask  *************** 
// foreground thread, runs when no other work needed
// never blocks, never sleeps, never dies
// inputs:  none
// outputs: none
unsigned long Idlecount=0;
void IdleTask(void){ 
  while(1) { 
    Idlecount++;        // debugging 
  }
}


//******** Interpreter **************
// your intepreter from Lab 4 
// foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}

char interpreterInputString[20];
void diskError(char* errtype, unsigned long n){
  UART_OutString(errtype);
  UART_OutUDec(n);
  OS_Kill();
}
void Interpreter(void)    // just a prototype, link to your interpreter
{
	uint32_t adcVoltage;
	uint8_t commandChosen = 0;
	OutCRLF();
	UART_OutString("Input Command: ");
	while(1){
		OutCRLF();
		//UART_OutString("Commands: 0 - Create, 1 - PrintDirectory, 2 - Delete");
		OutCRLF();
		commandChosen = UART_InChar();
		switch(commandChosen)
		{
			case '0':
				OutCRLF();
				UART_OutString("Enter file name to create");
			  UART_InString(interpreterInputString,7);
				if(eFile_Create(interpreterInputString))     diskError("eFile_Create",0);
				OutCRLF();
				UART_OutString("File created");
				OutCRLF();
				break;
			case '1':
				OutCRLF();
				eFile_Directory(&UART_OutChar);
				OutCRLF();
				break;
			case '2':
				OutCRLF();
				UART_OutString("Enter file name to delete");
			  UART_InString(interpreterInputString,7);
				if(eFile_Delete(interpreterInputString))     diskError("eFile_Delete",0);
				OutCRLF();
				UART_OutString("File deleted");
				OutCRLF();
				break;
			default:
				UART_OutString("Incorrect command!");
				break;
		}

		//adcSample = ADC_In();
		//ST7735_SetCursor(0,0);
		//ST7735_OutUDec(adcSample);

	}
}
// add the following commands, remove commands that do not make sense anymore
// 1) format 
// 2) directory 
// 3) print file
// 4) delete file
// execute   eFile_Init();  after periodic interrupts have started

//*******************lab 5 main **********
int realmain(void){        // lab 5 real main
  OS_Init();           // initialize, disable interrupts
  Running = 0;         // robot not running
  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;

//********initialize communication channels
  OS_Fifo_Init(512);    // ***note*** 4 is not big enough*****
  ADC_Collect(0, 1000, &Producer); // start ADC sampling, channel 0, 1000 Hz

//*******attach background tasks***********
  OS_AddSW1Task(&ButtonPush,2);
  OS_AddSW2Task(&DownPush,3);
  OS_AddPeriodicThread(disk_timerproc,10*TIME_1MS,5);

  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,2); 
  NumCreated += OS_AddThread(&IdleTask,128,7);  // runs when nothing useful to do
 
  OS_Launch(TIMESLICE); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}


//*****************test programs*************************
unsigned char buffer[512];
#define MAXBLOCKS 100

void TestDisk(void){  DSTATUS result;  unsigned short block;  int i; unsigned long n;
  // simple test of eDisk
  //printf("\n\rEE345M/EE380L, Lab 5 eDisk test\n\r");
  result = eDisk_Init(0);  // initialize disk
  if(result) diskError("eDisk_Init",result);
  //printf("Writing blocks\n\r");
  n = 1;    // seed
  for(block = 0; block < MAXBLOCKS; block++){
    for(i=0;i<512;i++){
      n = (16807*n)%2147483647; // pseudo random sequence
      buffer[i] = 0xFF&n;        
    }
    GPIO_PF3 = 0x08;     // PF3 high for 100 block writes
    if(eDisk_WriteBlock(buffer,block))diskError("eDisk_WriteBlock",block); // save to disk
    GPIO_PF3 = 0x00;     
  }  
  //printf("Reading blocks\n\r");
  n = 1;  // reseed, start over to get the same sequence
  for(block = 0; block < MAXBLOCKS; block++){
    GPIO_PF2 = 0x04;     // PF2 high for one block read
    if(eDisk_ReadBlock(buffer,block))diskError("eDisk_ReadBlock",block); // read from disk
    GPIO_PF2 = 0x00;
    for(i=0;i<512;i++){
      n = (16807*n)%2147483647; // pseudo random sequence
      if(buffer[i] != (0xFF&n)){
        //printf("Read data not correct, block=%u, i=%u, expected %u, read %u\n\r",block,i,(0xFF&n),buffer[i]);
        OS_Kill();
      }      
    }
  }  
  //printf("Successful test of %u blocks\n\r",MAXBLOCKS);
  OS_Kill();
}
void RunTest(void){
  NumCreated += OS_AddThread(&TestDisk,128,1);  
}
//******************* test main1 **********
// SYSTICK interrupts, period established by OS_Launch
// Timer interrupts, period established by first call to OS_AddPeriodicThread
int main1(void){   // testmain1
  OS_Init();           // initialize, disable interrupts

//*******attach background tasks***********
  OS_AddPeriodicThread(&disk_timerproc,10*TIME_1MS,0);   // time out routines for disk
  OS_AddSW1Task(&RunTest,2);
  
  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&TestDisk,128,1);  
  NumCreated += OS_AddThread(&IdleTask,128,3); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}

void TestFile(void){   int i; char data; 
  UART_OutString("\n\rEE345M/EE380L, Lab 5 eFile test\n\r");
  // simple test of eFile
  if(eFile_Init())              diskError("eFile_Init",0); 
	UART_OutString("Initialized\n\r");
  if(eFile_Format())            diskError("eFile_Format",0); 
	UART_OutString("Formatted\n\r");
  eFile_Directory(&UART_OutChar);
  if(eFile_Create("file1"))     diskError("eFile_Create",0);
	if(eFile_Create("file2"))     diskError("eFile_Create",0);
	if(eFile_Create("file3"))     diskError("eFile_Create",0);
  if(eFile_WOpen("file1"))      diskError("eFile_WOpen",0);
  for(i=0;i<2000;i++){
    if(eFile_Write('a'+i%26))   diskError("eFile_Write",i);
    //if(i%52==51){
    //  if(eFile_Write('\n'))     diskError("eFile_Write",i);  
    //  if(eFile_Write('\r'))     diskError("eFile_Write",i);
    //}
  }
  if(eFile_WClose())            diskError("eFile_Close",0);
  if(eFile_WOpen("file2"))      diskError("eFile_WOpen",0);
  for(i=0;i<1000;i++){
    if(eFile_Write('a'+i%26))   diskError("eFile_Write",i);
    //if(i%52==51){
    //  if(eFile_Write('\n'))     diskError("eFile_Write",i);  
    //  if(eFile_Write('\r'))     diskError("eFile_Write",i);
    //}
  }
  if(eFile_WClose())            diskError("eFile_Close",0);
  if(eFile_WOpen("file3"))      diskError("eFile_WOpen",0);
  for(i=0;i<1000;i++){
    if(eFile_Write('a'+i%26))   diskError("eFile_Write",i);
    //if(i%52==51){
    //  if(eFile_Write('\n'))     diskError("eFile_Write",i);  
    //  if(eFile_Write('\r'))     diskError("eFile_Write",i);
    //}
  }
  if(eFile_WClose())            diskError("eFile_Close",0);
  eFile_Directory(&UART_OutChar);
  if(eFile_ROpen("file1"))      diskError("eFile_ROpen",0);
  for(i=0;i<2000;i++){
    if(eFile_ReadNext(&data))   diskError("eFile_ReadNext",i);
    UART_OutChar(data);
  }
  if(eFile_ROpen("file1"))      diskError("eFile_ROpen",25);
  for(i=0;i<2000;i++){
    if(eFile_ReadNext(&data))   diskError("eFile_ReadNext",i);
    UART_OutChar(data);
  }
	
	eFile_RClose();
	
  if(eFile_ROpen("file2"))      diskError("eFile_ROpen",0);
  for(i=0;i<1000;i++){
    if(eFile_ReadNext(&data))   diskError("eFile_ReadNext",i);
    UART_OutChar(data);
  }
	eFile_RClose();
  if(eFile_ROpen("file3"))      diskError("eFile_ROpen",0);
  for(i=0;i<1000;i++){
    if(eFile_ReadNext(&data))   diskError("eFile_ReadNext",i);
    UART_OutChar(data);
  }
	eFile_RClose();
  if(eFile_Delete("file1"))     diskError("eFile_Delete",0);
	if(eFile_Delete("file2"))     diskError("eFile_Delete",0);
	if(eFile_Delete("file3"))     diskError("eFile_Delete",0);
  eFile_Directory(&UART_OutChar);
  UART_OutString("Successful test of creating a file\n\r");
  OS_Kill();
}

//******************* test main2 **********
// SYSTICK interrupts, period established by OS_Launch
// Timer interrupts, period established by first call to OS_AddPeriodicThread
int main(void){ 
  OS_Init();           // initialize, disable interrupts

//*******attach background tasks***********
  OS_AddPeriodicThread(&disk_timerproc,20,0);   // time out routines for disk
  if(eFile_Init())              diskError("eFile_Init",0); 
	UART_OutString("Initialized\n\r");
  if(eFile_Format())            diskError("eFile_Format",0); 
  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,1);  
  NumCreated += OS_AddThread(&IdleTask,128,3); 
 
  OS_Launch(10*TIME_1MS); // doesn't return, interrupts enabled in here
  return 0;               // this never executes
}
