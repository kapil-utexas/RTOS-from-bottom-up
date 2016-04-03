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
#include "heap.h"
#include "ADC.h"
#include "Switch.h"
#include "Lab2.h"
#define MILLISECONDCOUNT 80000
#define STACKSIZE 256
#define NUMBEROFTHREADS 40
#define NUMBEROFPROCESSES NUMBEROFTHREADS/10 // each process gets 5 threads hardcoded
#define NUMBEROFPERIODICTHREADS 3
void (*PeriodicTask)(void); //function pointer which takes void argument and returns void
 uint32_t timerCounter = 0;
 uint32_t timerMsCounter = 0;
int32_t StartCritical(void);
void EndCritical(int32_t primask);
uint32_t currentProcess;
struct TCB * RunPt; //scheduler pointer

struct TCB * DeadPt; //dead threads pointer
struct PCB * DeadProcessPt; //dead process pointer
struct TCB * SleepPt; //sleeping threads pointer
struct TCB * SchedulerPt; //sleeping threads pointer
struct PeriodicThread periodicPool[NUMBEROFPERIODICTHREADS];
struct PeriodicThread * DeadPeriodicPt = '\0';
struct PeriodicThread * PeriodPt;
//counters to keep track of how many elements are in each pool
uint32_t schedulerCount;
uint32_t periodicCount;
uint32_t deadCount;
uint32_t deadPeriodicCount;
uint32_t sleepCount;
uint8_t switched = 0;
struct TCB threadPool[NUMBEROFTHREADS];
struct PCB processPool[NUMBEROFPROCESSES];
void Timer2_Init(unsigned long value);
static void threadRemover(struct TCB ** toAdd, unsigned long sleepTime);
struct TCB * nextBeforeSwitch;
static void addDeadToScheduler(struct TCB ** from);
static void addSleepToScheduler(struct TCB * thread);
//OSasm definitions
void OS_DisableInterrupts(void); // Disable interrupts
void OS_EnableInterrupts(void);  // Enable interrupts
void StartOS(void);

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers 
// input:  none
// output: none
void OS_Init()
{
	uint8_t counter = 0;
	OS_DisableInterrupts();
	PLL_Init(Bus80MHz);
	UART_Init();              					 // initialize UART
	//ADC_Init(0);
	//UART_Init();              					 // initialize UART
	
	//construct linked list
	for (counter = 0; counter<NUMBEROFTHREADS - 1; counter++)
	{
		threadPool[counter].nextTCB = &threadPool[(counter + 1)]; //address of next TCB
	}
	threadPool[NUMBEROFTHREADS - 1].nextTCB = '\0'; 
	//lab3 linked list for periodic threads
	for (counter = 0; counter<NUMBEROFPERIODICTHREADS - 1; counter++)
	{
		periodicPool[counter].nextPeriodicThread = &periodicPool[(counter + 1)]; //address of next TCB
	}
	periodicPool[NUMBEROFPERIODICTHREADS - 1].nextPeriodicThread = '\0'; 
	
	for(int i = 0; i< NUMBEROFTHREADS; i++)
	{
		threadPool[i].id = i;
		if(threadPool[i].id >=0 && threadPool[i].id<10)
		{
			threadPool[i].processId = 0;
		}
		else if(threadPool[i].id>=10 && threadPool[i].id<20)
		{
			threadPool[i].processId = 1;
		}
		else if(threadPool[i].id>=20 && threadPool[i].id<30)
		{
			threadPool[i].processId = 2;
		}
		else 
		{
			threadPool[i].processId = 3;
		}
	}
	for (counter = 1; counter<NUMBEROFPROCESSES - 1; counter++)
	{
		processPool[counter].nextProcess = &processPool[(counter + 1)]; //address of next TCB
	}
	processPool[NUMBEROFPROCESSES - 1].nextProcess = '\0'; 
	for(int i =0; i<NUMBEROFPROCESSES; i++)
	{
		processPool[i].processId = 	i;
	}
	
	
	DeadPt = &threadPool[0]; //point to first element of not active threads 
	DeadProcessPt = &processPool[1];
	DeadPeriodicPt = &periodicPool[0];
	deadCount = NUMBEROFTHREADS;
	deadPeriodicCount = NUMBEROFPERIODICTHREADS;
	RunPt = '\0';
	SchedulerPt = '\0';
	SleepPt = '\0';
	PeriodPt = '\0';
	//Timer2_Init(20000); //1 ms period for taking time!!!!!!
	NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000; // priority 7 on systick
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R & 0xFF1FFFFF) |  0x00E00000; //priority 7 on pendsv
}

uint32_t howManyTimes = 0;
static void Timer1A_Init(unsigned long period){
	volatile uint32_t delay;
	SYSCTL_RCGCTIMER_R |= 0x02;      // activate timer1
	delay = SYSCTL_RCGCTIMER_R;          // allow time to finish activating
	howManyTimes++;
	TIMER1_CTL_R = 0x00000000; // disable timer1A during setup
	TIMER1_CFG_R = 0x00000000;                // configure for 32-bit timer mode
	TIMER1_TAMR_R = 0x00000002; //activate periodic timer mode 
  TIMER1_TAILR_R = period - 1;         // start value 
	TIMER1_TAPR_R = 0;            // 5) bus clock resolution
  TIMER1_IMR_R = 0x00000001;// enable timeout (rollover) interrupt
  TIMER1_ICR_R = 0x00000001;// clear timer1A timeout flag
	NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)| 1<<15; // 8) priority 4 // bit 15, 14, 13 for Timer1A 
  NVIC_EN0_R |=	1<<21;              // enable interrupt 21 in NVIC in a friendly manner
  TIMER1_CTL_R |= 0x00000001;  // enable timer1A 32-b, periodic, interrupts
	
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
	Timer2_Init(80);
	Timer1A_Init(40000); 
	RunPt = SchedulerPt; //make the first thread active
	NVIC_ST_RELOAD_R = theTimeSlice - 1; //timeslice is given in clock cycles 
	NVIC_ST_CTRL_R = 0x07; //enable systick
	StartOS();
	while(1);
}






int millisecondsToCount(uint32_t period)
{
	return period * MILLISECONDCOUNT; 
	
}

//input:
//task: pointer to a function
//period: 32 bit number for clock period
//priority: will be a three bit number from 0 to 7


void removeAndAddToSingleList(struct PeriodicThread ** from, unsigned long  period, void(*task)(void), unsigned long priority)
{
	if( PeriodPt == '\0') //no periodic threads are running
	{
		PeriodPt = *from; //this gets the pointer to the dead periodic pool 
		*from = (*(*from)).nextPeriodicThread; //remove node from the linked list
		//(*PeriodPt).nextPeriodicThread = PeriodPt; 
		(*PeriodPt).nextPeriodicThread = '\0'; //make it not circular
		//now need to setup node added
		(*PeriodPt).period = period;
		(*PeriodPt).task = task;
		(*PeriodPt).timeLeft = period;
		(*PeriodPt).priority = priority;
	}
	else //at least one periodic thread is running
	{
		struct PeriodicThread * temp = PeriodPt; // pointer to traverse
		//if priority is higher than first element, insert easily
		if(priority < (*PeriodPt).priority) //gonna be added before first element 
		{
			temp = (*(*from)).nextPeriodicThread ; //save before removing which node should be the first after removal
			(*(*from)).nextPeriodicThread = PeriodPt; //point node to first element of where it will be added
			PeriodPt = (*from); //move pointer backwards (finishes addition)
			*from = temp; //move pointer to the right (finished deletion)
			(*PeriodPt).period = period;
			(*PeriodPt).task = task;
			(*PeriodPt).timeLeft = period;
			(*PeriodPt).priority = priority;
		}
		else //gonna be added after first element
		{
			while( (*temp).nextPeriodicThread != '\0' &&  (priority >= (*(*temp).nextPeriodicThread).priority)         ) 
			{
				temp = (*temp).nextPeriodicThread;
			}
			//now we are at the priority where we want to add to
			(*temp).nextPeriodicThread = *from; //make last node point to the one we want to add
			*from = (*(*from)).nextPeriodicThread; //remove node from linked list
			temp = (*temp).nextPeriodicThread; //go to element just added to the list
			(*temp).nextPeriodicThread = '\0'; // make it non circular
			//now need to setup node added
			(*temp).period = period;
			(*temp).task = task;
			(*temp).timeLeft = period;
			(*temp).priority = priority;
		}
	}
	periodicCount++;
	deadPeriodicCount--;
}

int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority){
	int32_t status;
	status = StartCritical();
	//now need to add to linked list
	if(DeadPeriodicPt == '\0') //cant add this thread
	{
		OS_DisableInterrupts();
		while(1){};
	}
	removeAndAddToSingleList(&DeadPeriodicPt, period, task, priority);
	EndCritical(status );
	return 0; //added sucesfully
}



//will clear global timer 
void OS_ClearMsTime(){
	timerMsCounter = 0;
}

//Will return the current 32-bit global counter. 
//The units of this system time are the period of
//interrupt passed since initializing with OS_AddPeriodicThread or last reset.
unsigned long OS_ReadPeriodicTime(){
	return timerCounter;
}

//this will be run by the time set in OS_Launch
//Right now initialized to 1ms
//ASSUMING .5 MS UNITS FOR PERIODIC THREADS
void Timer1A_Handler(){
	TIMER1_ICR_R = TIMER_ICR_TATOCINT;  // acknowledge timer1A timeout
	int32_t status;
	status = StartCritical();
	//need to traverse linked list of peridoc threads
	struct PeriodicThread * tempTraversal = PeriodPt;
	if(PeriodPt != '\0') //at least one periodic thread has been added
	{
		while(tempTraversal != '\0')
		{
			(*tempTraversal).timeLeft --; //decrease 1ms
			if((*tempTraversal).timeLeft == 0) //need to run!
			{
				(*tempTraversal).timeLeft = (*tempTraversal).period; //reset time left
				(*(*tempTraversal).task)(); //run the task
			}
			tempTraversal = (*tempTraversal).nextPeriodicThread; //finished running or checking, go to next node
		}
		
	}

	EndCritical(status);	
}

void addThreadToBlocked(struct TCB ** toAdd)
{
	struct TCB * removed;
	struct TCB * temp;
	
	(*RunPt).active = 0;
	(*RunPt).blockedState = 1;
	//remove from active 
	temp = SchedulerPt;
	
	if(RunPt == temp) //first element
	{
		if(schedulerCount == 0)
		{
			OS_DisableInterrupts();
			while(1){};
		}
		if(schedulerCount == 1)
		{
			OS_DisableInterrupts();
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
	
	temp = *toAdd;
	if(temp == '\0')
	{
		*toAdd = removed;
		(*removed).nextTCB = '\0';
	}
	else
	{
		struct TCB * temp = (*toAdd); 
		if((*removed).priority < (*(*toAdd)).priority) // gonna be added at beginning
		{
				temp = (*(*toAdd)).nextTCB; //save before removing which node should be the first after removal
				(*toAdd) = removed;
				(*removed).nextTCB = temp; //point node to first element of where it will be added
		}
		else
		{
			while( ((*temp).nextTCB != '\0') &&  ((*removed).priority >= (*(*temp).nextTCB).priority))
			{
				temp = (*temp).nextTCB;
			}
				//now we are at the priority where we want to add to
			struct TCB * nextBeforeAdding = (*temp).nextTCB; //save before rerouting to restore at end
			(*temp).nextTCB = removed;
			temp = (*temp).nextTCB; //temp now equals thread
			(*temp).nextTCB = nextBeforeAdding;  //restore connection
		}
	}
	schedulerCount--;
}

// ******** OS_Block ************
// place this thread into a blocked state
// input:  none
// output: none
// OS_Sleep(0) implements cooperative multitasking
void OS_Block(Sema4Type *semaPt)
{
	addThreadToBlocked(&(*semaPt).blockedThreads);
	switched = 1;
}

// ******** OS_InitSemaphore ************
// initialize semaphore before launch OS
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value)
{
	(*semaPt).Value = value;
	(*semaPt).blockedThreads = '\0';
}

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt)
{
	uint32_t status;
	status = StartCritical();
	(*semaPt).Value--; //decrease count
	if((*semaPt).Value < 0)
	{
		OS_Block(semaPt); //block and put into semaphore blocked list
		OS_Suspend();
		OS_EnableInterrupts();
		OS_DisableInterrupts();
	} 
	EndCritical(status);
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
	if((*semaPt).Value <= 0) 
	{
		(*(*semaPt).blockedThreads).active = 1;
		(*(*semaPt).blockedThreads).blockedState = 0;
		addDeadToScheduler(&(*semaPt).blockedThreads); //addDead is similar to what we want to do. Just takes first element of linked list and adds to scheduler
	}
	EndCritical(status );

}

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt)
{
	uint32_t status;
	status = StartCritical();
	(*semaPt).Value--; //decrease count
	if((*semaPt).Value < 0)
	{
		OS_Block(semaPt); //block and put into semaphore blocked list
		OS_Suspend();
		OS_EnableInterrupts();
		OS_DisableInterrupts();
	} 
	EndCritical(status);
}
// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt)
{
	int32_t status;
	status = StartCritical();
	(*semaPt).Value ++;
	if((*semaPt).Value <= 0) 
	{
		(*(*semaPt).blockedThreads).active = 1;
		(*(*semaPt).blockedThreads).blockedState = 0;
		addDeadToScheduler(&(*semaPt).blockedThreads); //addDead is similar to what we want to do. Just takes first element of linked list and adds to scheduler
	}
	EndCritical(status );
}

uint8_t higherPriorityAdded = 0;
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
//adds dead to scheduler
//will add first node of a list to the scheduler
static void addDeadToScheduler(struct TCB ** from)
{
	//if scheduler is empty, just add at the first location
	if(SchedulerPt == '\0')
	{
		SchedulerPt = *from;
		*from = (*(*from)).nextTCB;
		(*SchedulerPt).nextTCB = SchedulerPt;
		
	}
	else //scheduler not empty
	{
		struct TCB * finalNode = SchedulerPt; //sleeping threads pointer
		while((*finalNode).nextTCB != (SchedulerPt))
		{
			finalNode = (*finalNode).nextTCB;
		}
		//now finalNode points to the last element of the scheduler that wraps around

			struct TCB * temp = SchedulerPt; // pointer to traverse
			//if priority is higher than first element, insert easily
			if( (*(*from)).priority < (*SchedulerPt).priority) //gonna be added before first element 
			{
				temp = (*(*from)).nextTCB; //save before removing which node should be the first after removal
				(*(*from)).nextTCB = SchedulerPt; //point node to first element of where it will be added
				SchedulerPt = (*from); //move pointer backwards (almost finishes addition, need to wrap)
				(*finalNode).nextTCB = SchedulerPt; //wrap around to what we just added
				*from = temp; //move pointer to the right (finished deletion)
				//need to context switch here
				higherPriorityAdded = 1; //flag
				
			}
			else //gonna be added after first element
			{
				while( ((*temp).nextTCB != (SchedulerPt)) &&  ((*(*from)).priority >= (*(*temp).nextTCB).priority)         ) 
				{
					temp = (*temp).nextTCB;
				}
				//now we are at the priority where we want to add to
				struct TCB * nextBeforeAdding = (*temp).nextTCB; //save before rerouting to restore at end
				(*temp).nextTCB = *from; //make last node point to the one we want to add
				*from = (*(*from)).nextTCB; //remove node from linked list
				temp = (*temp).nextTCB; //go to element just added to the list
				(*temp).nextTCB = nextBeforeAdding; 

			}
		}
	schedulerCount++;
	deadCount--;
}


static void addDeadNotFirstToScheduler(struct TCB * thread)
{
	struct TCB * prevToDelete = DeadPt;
	(*thread).sleepState = 0;
	(*thread).needToWakeUp = 0;
	(*thread).active = 1;
	if(prevToDelete == thread) //if its the first element
	{
		
		if(SchedulerPt == '\0')
		{
			SchedulerPt = thread; 
			DeadPt = DeadPt->nextTCB; //remove from sleep pool
			(*SchedulerPt).nextTCB = SchedulerPt; //point to itself
		}
		else //scheduler not empty
		{
			struct TCB * finalNode = SchedulerPt; //sleeping threads pointer
			while((*finalNode).nextTCB != (SchedulerPt))
			{
				finalNode = (*finalNode).nextTCB;
			}
			//now finalNode points to the last element of the scheduler that wraps around
			struct TCB * temp = SchedulerPt; 
			if((*thread).priority < (*SchedulerPt).priority) //gonna be added before first element
			{
				temp = (*thread).nextTCB; //save before removing which node should be the first after removal
				(*thread).nextTCB = SchedulerPt; //point node to first element of where it will be added
				SchedulerPt = (thread); //move pointer backwards (almost finishes addition, need to wrap)
				(*finalNode).nextTCB = SchedulerPt; //wrap around to what we just added
				DeadPt = temp; //move pointer to the right (finished deletion)
				//need to context switch here
				higherPriorityAdded = 1; //flag				
			}
			else
			{
				while( ((*temp).nextTCB != (SchedulerPt)) &&  ((*thread).priority >= (*(*temp).nextTCB).priority))
				{
					temp = (*temp).nextTCB;
				}
				//now we are at the priority where we want to add to
				struct TCB * nextBeforeAdding = (*temp).nextTCB; //save before rerouting to restore at end
				(*temp).nextTCB = thread;
				DeadPt = DeadPt->nextTCB; //remove from sleep pool
				temp = (*temp).nextTCB; //temp now equals thread
				(*temp).nextTCB = nextBeforeAdding;  //restore connection
			}
		}
	}
	else
	{
		while((*prevToDelete).nextTCB != thread)
		{
			prevToDelete = (*prevToDelete).nextTCB;
		}
		struct TCB * finalNode = SchedulerPt; 
		while((*finalNode).nextTCB != (SchedulerPt))
		{
			finalNode = (*finalNode).nextTCB;
		}
		if(SchedulerPt == '\0') //if scheduler empty
		{
			struct TCB * nextBeforeDeletion = (*thread).nextTCB;; //active threads pointer
			SchedulerPt = (*prevToDelete).nextTCB; //should add thread passed as parameter
			(*prevToDelete).nextTCB = nextBeforeDeletion;
			(*SchedulerPt).nextTCB = SchedulerPt;
		}
		else //scheduler not empty
		{
			struct TCB * temp = SchedulerPt; //active threads pointer
			if((*thread).priority < (*SchedulerPt).priority) //will add at first element
			{
				temp = (*thread).nextTCB; //save before removing which node should be the first after removal
				(*thread).nextTCB = SchedulerPt; //point node to first element of where it will be added
				SchedulerPt = (thread); //move pointer backwards (almost finishes addition, need to wrap)
				(*finalNode).nextTCB = SchedulerPt; //wrap around to what we just added
				(*prevToDelete).nextTCB = temp;
				//need to context switch here
				higherPriorityAdded = 1; //flag					
			}
			else
			{
				while( ((*temp).nextTCB != (SchedulerPt)) &&  ((*thread).priority >= (*(*temp).nextTCB).priority))
				{
					temp = (*temp).nextTCB;
				}
				//now we are at the priority where we want to add to
				struct TCB * nextBeforeDeletion = (*thread).nextTCB;; //active threads pointer
				struct TCB * nextBeforeAdding = (*temp).nextTCB; //save before rerouting to restore at end
				(*temp).nextTCB = thread; //temp points to where we want to add
				(*prevToDelete).nextTCB = nextBeforeDeletion;
				temp = (*temp).nextTCB; //temp now equals thread
				(*temp).nextTCB = nextBeforeAdding;  //restore connection
			}
		}
	}
	schedulerCount++;
	sleepCount--;
	
}
uint32_t justAddedId = 0;
//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields

int OS_AddThread(void(*task)(void), unsigned long stackSize, unsigned long priority)
{
	uint32_t status;
	status = StartCritical();
	uint32_t tempProcessRunning;
	//we will take the first thread from the dead pool
	if(DeadPt == '\0')
	{
		OS_DisableInterrupts();
		while(1){};
	}
	if(justAddedId!=0)
	{
		tempProcessRunning = currentProcess;
		currentProcess = justAddedId;
	}
	struct TCB * DeadPoolTemp = DeadPt;
	while((DeadPoolTemp->id < currentProcess * 10) || (DeadPoolTemp->id > currentProcess * 10 + 9))
	{
		DeadPoolTemp = DeadPoolTemp->nextTCB;
	}
	//now DeadPoolTemp points to what we want to add
	
	if((DeadPoolTemp)->id >=0 && (DeadPoolTemp)->id<10)
	{
		processPool[0].threadsAlive++;
	}
	else if((DeadPoolTemp)->id>=10 && (DeadPoolTemp)->id<20)
	{
		processPool[1].threadsAlive++;
	}
	else if((DeadPoolTemp)->id>=20 && (DeadPoolTemp)->id<30)
	{
		processPool[2].threadsAlive++;
	}
	else 
	{
		processPool[3].threadsAlive++;
	}

	if(justAddedId!=0)
	{
		currentProcess = tempProcessRunning;
		justAddedId = 0;
	}
	
	(DeadPoolTemp)->active = 1;
	(DeadPoolTemp)->sleepState = 0; //flag
	(DeadPoolTemp)->priority = priority; 
	(DeadPoolTemp)->blockedState = 0; //flag
	(DeadPoolTemp)->needToWakeUp = 0; //flag
	SetInitialStack(DeadPoolTemp, stackSize);
	(DeadPoolTemp)->stack[stackSize - 2] = (uint32_t)task; //push PC
	addDeadNotFirstToScheduler(DeadPoolTemp);
	//addDeadToScheduler(&DeadPoolTemp);
	if(higherPriorityAdded == 1)
	{
		OS_Suspend();
	}
	EndCritical(status);
	
	return 1;
}


int  OS_AddProcess(void(*entry)(void), void *text, void *data, unsigned long stackSize, unsigned long priority)
{
	uint32_t status;
	status = StartCritical();
	//we will take the first thread from the dead pool
	//allocate memory for one PCB
	//Modify that PCB
	struct PCB * temp = DeadProcessPt;
	while(temp != '\0' && temp->threadsAlive > 0)
	{
		temp = temp->nextProcess;
	}
	if (temp == '\0')
	{
		
		//dealloc memory
		if( Heap_Free(text) != HEAP_OK)
		{
			while(1){};
		}
		if( Heap_Free(data) != HEAP_OK)
		{
			while(1){};
		}
		EndCritical(status);
		return 0;
	}
	(temp)->startingAddress = entry;
	(temp)->codeSegment= text;
	(temp)->dataSegment= data; 
	 justAddedId = temp->processId;
	 OS_AddThread(entry, stackSize, priority);
	 EndCritical(status);
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
			OS_DisableInterrupts();
			while(1){};
		}
		if(schedulerCount == 1)
		{
			OS_DisableInterrupts();
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
	uint32_t processLinkedTo;
	if((RunPt)->id >=0 && (RunPt)->id<10)
	{
		processLinkedTo = 0;
	}
	else if((RunPt)->id>=10 && (RunPt)->id<20)
	{
		processLinkedTo = 1;
	}
	else if((RunPt)->id>=20 && (RunPt)->id<30)
	{
		processLinkedTo = 2;
	}
	else 
	{
		processLinkedTo = 3;
	}
	processPool[processLinkedTo].threadsAlive--;
	if(processPool[processLinkedTo].threadsAlive == 0)
	{
		//dealloc memory
		if( Heap_Free(processPool[processLinkedTo].codeSegment) != HEAP_OK)
		{
			while(1){};
		}
		if( Heap_Free(processPool[processLinkedTo].dataSegment) != HEAP_OK)
		{
			while(1){};
		}
		
	}
	threadRemover(&DeadPt, 0); //parameter 0 will be ignored
	deadCount++;
	switched = 1;
	OS_Suspend();
	OS_EnableInterrupts();

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
	sleepCount++;
	switched = 1;
	OS_Suspend();
	OS_EnableInterrupts();
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
		Switch_Init(task,priority, 4);
		return 1;
}

//******** OS_AddSW2Task *************** 
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
int OS_AddSW2Task(void(*task)(void), unsigned long priority)
{
		Switch_Init(task,priority, 0);
		return 1;
}


static void addSleepToScheduler(struct TCB * thread)
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
		else //scheduler not empty
		{
			struct TCB * finalNode = SchedulerPt; //sleeping threads pointer
			while((*finalNode).nextTCB != (SchedulerPt))
			{
				finalNode = (*finalNode).nextTCB;
			}
			//now finalNode points to the last element of the scheduler that wraps around
			struct TCB * temp = SchedulerPt; 
			if((*thread).priority < (*SchedulerPt).priority) //gonna be added before first element
			{
				temp = (*thread).nextTCB; //save before removing which node should be the first after removal
				(*thread).nextTCB = SchedulerPt; //point node to first element of where it will be added
				SchedulerPt = (thread); //move pointer backwards (almost finishes addition, need to wrap)
				(*finalNode).nextTCB = SchedulerPt; //wrap around to what we just added
				SleepPt = temp; //move pointer to the right (finished deletion)
				//need to context switch here
				higherPriorityAdded = 1; //flag				
			}
			else
			{
				while( ((*temp).nextTCB != (SchedulerPt)) &&  ((*thread).priority >= (*(*temp).nextTCB).priority))
				{
					temp = (*temp).nextTCB;
				}
				//now we are at the priority where we want to add to
				struct TCB * nextBeforeAdding = (*temp).nextTCB; //save before rerouting to restore at end
				(*temp).nextTCB = thread;
				SleepPt = SleepPt->nextTCB; //remove from sleep pool
				temp = (*temp).nextTCB; //temp now equals thread
				(*temp).nextTCB = nextBeforeAdding;  //restore connection
			}
		}
	}
	else
	{
		while((*prevToDelete).nextTCB != thread)
		{
			prevToDelete = (*prevToDelete).nextTCB;
		}
		struct TCB * finalNode = SchedulerPt; 
		while((*finalNode).nextTCB != (SchedulerPt))
		{
			finalNode = (*finalNode).nextTCB;
		}
		if(SchedulerPt == '\0') //if scheduler empty
		{
			struct TCB * nextBeforeDeletion = (*thread).nextTCB;; //active threads pointer
			SchedulerPt = (*prevToDelete).nextTCB; //should add thread passed as parameter
			(*prevToDelete).nextTCB = nextBeforeDeletion;
			(*SchedulerPt).nextTCB = SchedulerPt;
		}
		else //scheduler not empty
		{
			struct TCB * temp = SchedulerPt; //active threads pointer
			if((*thread).priority < (*SchedulerPt).priority) //will add at first element
			{
				temp = (*thread).nextTCB; //save before removing which node should be the first after removal
				(*thread).nextTCB = SchedulerPt; //point node to first element of where it will be added
				SchedulerPt = (thread); //move pointer backwards (almost finishes addition, need to wrap)
				(*finalNode).nextTCB = SchedulerPt; //wrap around to what we just added
				(*prevToDelete).nextTCB = temp;
				//need to context switch here
				higherPriorityAdded = 1; //flag					
			}
			else
			{
				while( ((*temp).nextTCB != (SchedulerPt)) &&  ((*thread).priority >= (*(*temp).nextTCB).priority))
				{
					temp = (*temp).nextTCB;
				}
				//now we are at the priority where we want to add to
				struct TCB * nextBeforeDeletion = (*thread).nextTCB;; //active threads pointer
				struct TCB * nextBeforeAdding = (*temp).nextTCB; //save before rerouting to restore at end
				(*temp).nextTCB = thread; //temp points to where we want to add
				(*prevToDelete).nextTCB = nextBeforeDeletion;
				temp = (*temp).nextTCB; //temp now equals thread
				(*temp).nextTCB = nextBeforeAdding;  //restore connection
			}
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
			addSleepToScheduler(temp); //take node out of sleep and put into scheduler
			temp = toRestore; //restore to be able to go to next thread
		}		
		if(temp  == '\0')
			break;
		temp = (*temp).nextTCB;
	}	
	if(higherPriorityAdded == 1)
	{
		OS_Suspend();
	}
}

//Fifo stuff
// Two-index implementation of the transmit FIFO
// can hold 0 to TXFIFOSIZE elements
#define TXFIFOSIZE 32 // must be a power of 2
#define TXFIFOSUCCESS 1
#define TXFIFOFAIL    0
typedef unsigned long txDataType;
unsigned long volatile OS_TxPutI;// put next
unsigned long volatile OS_TxGetI;// get next
txDataType TxFifo[TXFIFOSIZE];

Sema4Type mutex;        // set in background
Sema4Type roomLeft;        // set in background
Sema4Type dataAvailable;        // set in background
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(unsigned long size)
{ 
  OS_TxPutI = OS_TxGetI = 0;  // Empty
  OS_InitSemaphore(&mutex,1);
	//OS_InitSemaphore(&roomLeft,TXFIFOSIZE);
	OS_InitSemaphore(&dataAvailable,0);
}

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data)
{		
		//OS_Wait(&roomLeft);
		//OS_bWait(&mutex);
		if((OS_TxPutI-OS_TxGetI) & ~(TXFIFOSIZE-1))
		{
			//OS_bSignal(&mutex);
			return TXFIFOFAIL;
		}
		TxFifo[OS_TxPutI&(TXFIFOSIZE-1)] = data; // put
		OS_TxPutI++;  // Success, update
		//OS_bSignal(&mutex);
		OS_Signal(&dataAvailable);
		return(TXFIFOSUCCESS);
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t samplesConsumed;
unsigned long OS_Fifo_Get(void)
{
		OS_Wait(&dataAvailable);
		OS_bWait(&mutex);
		unsigned long toReturn;
		toReturn = TxFifo[OS_TxGetI&(TXFIFOSIZE-1)];
		OS_TxGetI++;  // Success, update
		samplesConsumed++;
		OS_bSignal(&mutex);
		//OS_Signal(&roomLeft);
		return toReturn;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void)
{
		OS_bWait(&mutex);
		long toReturn =  (OS_TxPutI-OS_TxGetI);
		OS_bSignal(&mutex);
		return toReturn;
}

//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
unsigned long OS_Id(void)
{
	return (*RunPt).id;
}

// ***************** Timer2_Init ****************
// Activate Timer2 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
// Outputs: none
void Timer2_Init(unsigned long period){
  SYSCTL_RCGCTIMER_R |= 0x04;   // 0) activate timer2
  TIMER2_CTL_R = 0x00000000;    // 1) disable timer2A during setup
  TIMER2_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER2_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER2_TAILR_R = period-1;    // 4) reload value
  TIMER2_TAPR_R = 0;            // 5) bus clock resolution
  TIMER2_ICR_R = 0x00000001;    // 6) clear timer2A timeout flag
  TIMER2_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|0x80000000; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 39, interrupt number 23
  NVIC_EN0_R = 1<<23;           // 9) enable IRQ 23 in NVIC
  TIMER2_CTL_R = 0x00000001;    // 10) enable timer2A
}

void Timer2A_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER2A timeout
  timerCounter+=1;
	timerMsCounter += 1;
}


// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// It is ok to make the resolution to match the first call to OS_AddPeriodicThread

unsigned long OS_MsTime(void)
{
	return timerMsCounter * 10000; //hardcoded
}

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void)
{
	return (timerCounter) * (80);
}

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long start, unsigned long stop)
{
	return stop - start;
}

uint32_t dataInMailBox;
uint8_t flagMailBox;
Sema4Type mailBoxFree;        // set in background
Sema4Type dataValid;        // set in background
// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void)
{
	OS_InitSemaphore(&mailBoxFree,1);
	OS_InitSemaphore(&dataValid,0);
	volatile uint32_t dataInMailBox = 0;
	
}


// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(unsigned long data)
{
	OS_bWait(&mailBoxFree); 
	dataInMailBox = data;
	OS_bSignal(&dataValid);
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
unsigned long OS_MailBox_Recv(void)
{
	unsigned long toReturn;
	OS_bWait(&dataValid);
	toReturn = dataInMailBox;
	OS_bSignal(&mailBoxFree);
	return toReturn;
}

long SW1MaxJitter;             // largest time jitter between interrupts in usec

unsigned long const SW1JitterSize=JITTERSIZE;
unsigned long SW1JitterHistogram[JITTERSIZE]={0,};
void OS_SW1Jitter(uint32_t period)
{
	unsigned static long LastTime;  // time at previous ADC sample
	unsigned long thisTime;         // time at current ADC sample
	long jitter;                    // time between measured and expected, in us
	uint8_t ignore = 0;

	thisTime = OS_Time();       // current time, 12.5 ns

	ignore++;        // calculation finished
	if(ignore>1){    // ignore timing of first interrupt
		unsigned long diff = OS_TimeDifference(LastTime,thisTime);
		if(diff>period){
			jitter = (diff-period+4)/8;  // in 0.1 usec
		}else{
			jitter = (period-diff+4)/8;  // in 0.1 usec
		}
		if(jitter > SW1MaxJitter){
			SW1MaxJitter = jitter; // in usec
		}       // jitter should be 0
		if(jitter >= SW1JitterSize){
			jitter = JITTERSIZE-1;
		}
		SW1JitterHistogram[jitter]++; 
	}
	LastTime = thisTime;
}
unsigned long const SW2JitterSize=JITTERSIZE;
long SW2MaxJitter;             // largest time jitter between interrupts in usec
unsigned long SW2JitterHistogram[JITTERSIZE]={0,};
void OS_SW2Jitter(uint32_t period)
{
	unsigned static long LastTime;  // time at previous ADC sample
	unsigned long thisTime;         // time at current ADC sample
	long jitter;                    // time between measured and expected, in us
	uint8_t ignore = 0;

	thisTime = OS_Time();       // current time, 12.5 ns

	ignore++;        // calculation finished
	if(ignore>1){    // ignore timing of first interrupt
		unsigned long diff = OS_TimeDifference(LastTime,thisTime);
		if(diff>period){
			jitter = (diff-period+4)/8;  // in 0.1 usec
		}else{
			jitter = (period-diff+4)/8;  // in 0.1 usec
		}
		if(jitter > SW2MaxJitter){
			SW2MaxJitter = jitter; // in usec
		}       // jitter should be 0
		if(jitter >= SW2JitterSize){
			jitter = JITTERSIZE-1;
		}
		SW2JitterHistogram[jitter]++; 
	}
	LastTime = thisTime;
}

uint32_t getProcessIdOfRunningThread()
{
	return RunPt->processId;
}

void * getNewR9()
{
	return processPool[currentProcess].dataSegment;
}
