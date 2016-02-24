// OS.c
// Runs on TM4C123
// Provide a function that initializes Timer1A to trigger a
// periodic software task.
// Ramon and Kapil 
// February 3, 2016


#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "OS.h"
#include "PLL.h"
#include "ST7735.h"
#include "UART.h"
#include "ADC.h"
#include "Switch.h"
#include "Lab2.h"
#define MILLISECONDCOUNT 80000
#define STACKSIZE 256
#define NUMBEROFTHREADS 10

void (*PeriodicTask)(void); //function pointer which takes void argument and returns void
static uint32_t timerCounter = 0;
int32_t StartCritical(void);
void EndCritical(int32_t primask);

struct TCB * RunPt; //scheduler pointer

struct TCB * DeadPt; //dead threads pointer
struct TCB * SleepPt; //sleeping threads pointer
struct TCB * SchedulerPt; //sleeping threads pointer
//counters to keep track of how many elements are in each pool
uint32_t schedulerCount; 
uint32_t deadCount;
uint32_t sleepCount;

struct TCB threadPool[NUMBEROFTHREADS];

//OSasm definitions
void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
void StartOS(void);
// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers 
// input:  none
// output: none
void dummy(void){}
void OS_Init()
{
	uint8_t counter = 0;
	OS_DisableInterrupts();
	PLL_Init(Bus80MHz);
	//ST7735_InitR(INITR_REDTAB);				   // initialize LCD
	//ADC_Init(0);
	//UART_Init();              					 // initialize UART
	//construct linked list
	for (counter = 0; counter<NUMBEROFTHREADS - 1; counter++)
	{
		threadPool[counter].nextTCB = &threadPool[(counter + 1)]; //address of next TCB
	}
	threadPool[NUMBEROFTHREADS - 1].nextTCB = '\0'; 
	DeadPt = &threadPool[0]; //point to first element of not active threads 
	deadCount = NUMBEROFTHREADS;
	RunPt = '\0';
	SchedulerPt = '\0';
	SleepPt = '\0';
	NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7
}
//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(unsigned long theTimeSlice){
	RunPt = SchedulerPt; //make the first thread active
	NVIC_ST_RELOAD_R = theTimeSlice - 1; //timeslice is given in clock cycles 
	NVIC_ST_CTRL_R = 0x07; //enable systick
	StartOS();
	while(1);
}
uint32_t howManyTimes = 0;
static void Timer1A_Init(unsigned long period, void(*task)(void), unsigned long priority){
	volatile uint32_t delay;
	SYSCTL_RCGCTIMER_R |= 0x02;      // activate timer1
	delay = SYSCTL_RCGCTIMER_R;          // allow time to finish activating
	PeriodicTask = task;          // user function
	howManyTimes++;
	TIMER1_CTL_R = 0x00000000; // disable timer1A during setup
	TIMER1_CFG_R = 0x00000000;                // configure for 32-bit timer mode
	TIMER1_TAMR_R = 0x00000002; //activate periodic timer mode 
  TIMER1_TAILR_R = period - 1;         // start value 
	TIMER1_TAPR_R = 0;            // 5) bus clock resolution
  TIMER1_IMR_R = 0x00000001;// enable timeout (rollover) interrupt
  TIMER1_ICR_R = 0x00000001;// clear timer1A timeout flag
	priority &= 0x7; //keep the last 3 bits
//	NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF1FFF)| priority<<15 ; // bit 15, 14, 13 for Timer1A 
	NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)| priority<<15; // 8) priority 4
  NVIC_EN0_R |=	1<<21;              // enable interrupt 21 in NVIC in a friendly manner
  TIMER1_CTL_R |= 0x00000001;  // enable timer1A 32-b, periodic, interrupts
	
}



//input:
//task: pointer to a function
//period: 32 bit number for clock period
//priority: will be a three bit number from 0 to 7
//Question: uin8_t fine? priority is only 3 bits 
int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority){
	OS_DisableInterrupts();
	Timer1A_Init(period, task, priority); //converts the period from milliseconds to cycles
	OS_EnableInterrupts();
	return 0;
}

//will clear global timer 
void OS_ClearPeriodicTime(){
	timerCounter = 0;
}

//Will return the current 32-bit global counter. 
//The units of this system time are the period of
//interrupt passed since initializing with OS_AddPeriodicThread or last reset.
unsigned long OS_ReadPeriodicTime(){
	return timerCounter;
}

void Timer1A_Handler(){
	TIMER1_ICR_R = TIMER_ICR_TATOCINT;  // acknowledge timer1A timeout
	OS_DisableInterrupts();
	void (*temp)(void) = &BackgroundThread1c;
	timerCounter++;
	(*temp)();
	OS_EnableInterrupts();
}

// ******** OS_InitSemaphore ************
// initialize semaphore before launch OS
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value)
{
	(*semaPt).Value = value;
}

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt)
{
	OS_DisableInterrupts();
	while((*semaPt).Value == 0)
	{
		OS_EnableInterrupts();
		OS_DisableInterrupts();
	} 
	(*semaPt).Value--; //decrease count
	OS_EnableInterrupts();

}

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt)
{
	int32_t status;
	status = StartCritical();
	(*semaPt).Value ++;
	EndCritical(status );

}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt)
{
	OS_DisableInterrupts();
	while((*semaPt).Value == 0)
	{
		OS_EnableInterrupts();
		OS_DisableInterrupts();
	} //while someone has the semaphor
	(*semaPt).Value = 0; //take the semaphore
	OS_EnableInterrupts();
}
// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt)
{
	OS_DisableInterrupts();
	(*semaPt).Value = 1;
	OS_EnableInterrupts();
}


void SetInitialStack(struct TCB * toFix, uint32_t stackSize){
	(*toFix).localSp = (*toFix).stack + stackSize - 16; //point to the bottom of the new stack
	(*toFix).stack[stackSize - 1] = 0x01000000; //thumb bit
	//PC not pushed... will be pushed later
	(*toFix).stack[stackSize - 3] = 0x14141414; //R14 
	(*toFix).stack[stackSize - 4] = 0x12121212; //R12
	(*toFix).stack[stackSize - 5] = 0x03030303; //R3
	(*toFix).stack[stackSize - 6] = 0x02020202; //R2
	(*toFix).stack[stackSize - 7] = 0x01010101; //R1
	(*toFix).stack[stackSize - 8] = 0x00000000; //R0
	(*toFix).stack[stackSize - 9] = 0x11111111; //R11
	(*toFix).stack[stackSize - 10]= 0x10101010; //R10
	(*toFix).stack[stackSize - 11]= 0x09090909; //R9
	(*toFix).stack[stackSize - 12]= 0x08080808; //R8
	(*toFix).stack[stackSize - 13]= 0x07070707; //R7
	(*toFix).stack[stackSize - 14]= 0x06060606; //R6
	(*toFix).stack[stackSize - 15]= 0x05050505; //R5
	(*toFix).stack[stackSize - 16]= 0x04040404; //R4
}

static void addThreadToScheduler(struct TCB ** from)
{

	if(SchedulerPt == '\0')
	{
		SchedulerPt = *from;
		*from = (*(*from)).nextTCB;
		(*SchedulerPt).nextTCB = SchedulerPt;
		
	}
	else
	{
		struct TCB * temp = SchedulerPt; //sleeping threads pointer
		while((*temp).nextTCB != (SchedulerPt))
		{
			temp = (*temp).nextTCB;
		}
		//now we are at the last node of the list
		(*temp).nextTCB = *from;
		*from = (*(*from)).nextTCB;
		temp = (*temp).nextTCB; //get the element you just added to the list
		(*temp).nextTCB = SchedulerPt; // point it to the beginning of the list (for circular)
	}
	schedulerCount++;
	deadCount--;
}

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
uint8_t uniqueId=0; //make unique ids
int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority)
{
	/*
	threadPool[uniqueId].id = uniqueId; //unique id
	threadPool[uniqueId].active = 1;
	threadPool[uniqueId].sleepState = 0; //flag
	threadPool[uniqueId].priority = priority; 
	threadPool[uniqueId].blockedState = 0; //flag
	SetInitialStack(&threadPool[uniqueId], stackSize);
	threadPool[uniqueId].stack[stackSize - 2] = (uint32_t)task; //push PC
	uniqueId++;
	*/
	//we will take the first thread from the dead pool
	(DeadPt)->id = uniqueId; //unique id
	(DeadPt)->active = 1;
	(DeadPt)->sleepState = 0; //flag
	(DeadPt)->priority = priority; 
	(DeadPt)->blockedState = 0; //flag
	(DeadPt)->needToWakeUp = 0; //flag
	SetInitialStack(DeadPt, stackSize);
	(DeadPt)->stack[stackSize - 2] = (uint32_t)task; //push PC
	uniqueId++;
	addThreadToScheduler(&DeadPt);
	return 1;
}

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(){
		NVIC_INT_CTRL_R = 0x10000000; //trigger PendSV
}


uint8_t switched = 0;
struct TCB * nextBeforeSwitch;
static void threadRemover(struct TCB ** toAdd, unsigned long sleepTime)
{
	struct TCB * removed;
	struct TCB * temp;
	
	(*RunPt).active = 0;
	//remove from active 
	temp = SchedulerPt;
	
	if(RunPt == temp) //first element
	{
		if(schedulerCount == 0)
		{
			while(1){};
		}
		if(schedulerCount == 1)
		{
			removed = SchedulerPt; //store before we remove
			SchedulerPt = '\0';
			while(1){};
		}
		else //remove and fix
		{
			while((*temp).nextTCB != (SchedulerPt))
			{
				temp = (*temp).nextTCB;
			}
			//now at end of Scheduler pool
			(*temp).nextTCB = (*SchedulerPt).nextTCB;
			removed = SchedulerPt; //store before we remove
			SchedulerPt = (*SchedulerPt).nextTCB;
			nextBeforeSwitch = SchedulerPt; //hax
		}
	}
	else //not the first element, could be middle or end (theres no end in circular)
	{
			while((*temp).nextTCB != (RunPt))
			{
				temp = (*temp).nextTCB;
			}
			nextBeforeSwitch = temp->nextTCB->nextTCB; //hax
			removed = (*temp).nextTCB;
			(*temp).nextTCB = (*(*temp).nextTCB).nextTCB; //remove from linked list
	}
	
	//now we have the node we took out in the "removed" variable
	if(*toAdd == SleepPt) //if putting thread to sleep need to update flag
	{
		(*removed).sleepState = sleepTime;
		if(sleepTime == 0)
		{
			(*removed).needToWakeUp = 1;
		}
	}
	
	temp = *toAdd;
	if(temp != '\0')
	{
		while((*temp).nextTCB != '\0')
		{
			temp = (*temp).nextTCB;
		}
		(*temp).nextTCB = removed;
		temp = (*temp).nextTCB;
		(*temp).nextTCB = '\0';
	}
	else
	{
		*toAdd = removed;
		(*removed).nextTCB = '\0';
	}
	
	schedulerCount--;
}

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void)
{
	OS_DisableInterrupts();
	threadRemover(&DeadPt, 0); //parameter 0 will be ignored
	switched = 1;
	deadCount++;
	OS_EnableInterrupts();
	OS_Suspend();
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// Sleep time is a multiple of context switch time period
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime)
{
	OS_DisableInterrupts();
	threadRemover(&SleepPt, sleepTime * 2);
	switched = 1;
	sleepCount++;
	OS_EnableInterrupts();
	OS_Suspend();

}

//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), unsigned long priority)
{
		if(DeadPt == '\0')
		{
			return 0;
		}
		Switch_Init(task,priority);
		return 1;
}

static void wakeUpThread(struct TCB * thread)
{
	struct TCB * prevToDelete = SleepPt;
	(*thread).sleepState = 0;
	(*thread).needToWakeUp = 0;
	(*thread).active = 1;
	if(prevToDelete == thread) //if its the first element
	{
		if(SchedulerPt == '\0')
		{
			SchedulerPt = thread; 
			SleepPt = SleepPt->nextTCB; //remove from sleep pool
			(*SchedulerPt).nextTCB = SchedulerPt; //point to itself
		}
		else
		{
			struct TCB * temp = SchedulerPt; //sleeping threads pointer
			while((*temp).nextTCB != (SchedulerPt))
			{
				temp = (*temp).nextTCB;
			}
			//now we are at the last node of the list
			(*temp).nextTCB = thread;
			SleepPt = SleepPt->nextTCB; //remove from sleep pool
			(*thread).nextTCB = SchedulerPt; // point it to the beginning of the list (for circular)
		}
	}
	else
	{
		while((*prevToDelete).nextTCB != thread)
		{
			prevToDelete = (*prevToDelete).nextTCB;
		}
		if(SchedulerPt == '\0')
		{
			SchedulerPt = (*prevToDelete).nextTCB;
			(*prevToDelete).nextTCB = prevToDelete->nextTCB->nextTCB;
			(*SchedulerPt).nextTCB = SchedulerPt;
		}
		else
		{
			struct TCB * temp = SchedulerPt; //sleeping threads pointer
			while((*temp).nextTCB != (SchedulerPt))
			{
				temp = (*temp).nextTCB;
			}
			//now we are at the last node of the list
			(*temp).nextTCB = thread;
			(*prevToDelete).nextTCB = prevToDelete ->nextTCB->nextTCB;
			(*thread).nextTCB = SchedulerPt; // point it to the beginning of the list (for circular)
		}
	}
	schedulerCount++;
	sleepCount--;
	
}

void traverseSleep(void)
{
	struct TCB * temp;
	struct TCB * toRestore;
	temp = SleepPt;
	while(temp!= '\0')
	{
		(*temp).sleepState -= 2;
		if( (*temp).sleepState <=0  && (*temp).needToWakeUp != 1) //if sleep was given a time to wake up 
		{
			toRestore = (*temp).nextTCB; //since we are taking node out, need to restore where we were
			wakeUpThread(temp); //take node out of sleep and put into scheduler
			temp = toRestore; //restore to be able to go to next thread
		}		
		if(temp  == '\0')
			break;
		temp = (*temp).nextTCB;
	}	
}

