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

#define MILLISECONDCOUNT 80000
#define STACKSIZE 256
#define NUMBEROFTHREADS 3

static void(*taskToDo)(void); //function pointer which takes void argument and returns void
static uint32_t timerCounter = 0;

struct TCB{
	uint32_t * localSp; //local stack pointer
	struct TCB * nextTCB; //pointer to next TCB
	uint8_t active;
	uint32_t stack [128];
	uint32_t id; //unique id
	uint8_t sleepState; //flag
	uint8_t priority; 
	uint8_t blockedState; //flag
};


struct TCB * RunPt;
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
void OS_Init()
{
	uint8_t counter = 0;
	OS_DisableInterrupts();
	PLL_Init(Bus80MHz);
	//ST7735_InitR(INITR_REDTAB);				   // initialize LCD
	//ADC_Init(0);
	//UART_Init();              					 // initialize UART
	//construct linked list
	for (counter = 0; counter<NUMBEROFTHREADS; counter++)
	{
		threadPool[counter].nextTCB = &threadPool[(counter + 1) % NUMBEROFTHREADS ]; //address of next TCB
	}
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
	RunPt = &threadPool[0]; //make the first thread active
	NVIC_ST_RELOAD_R = theTimeSlice - 1; //timeslice is given in clock cycles 
	NVIC_ST_CTRL_R = 0x07; //enable systick
	StartOS();
	while(1);
}

static void Timer1A_Init(unsigned long period){
	volatile uint32_t delay;
	SYSCTL_RCGCTIMER_R |= 0x02;      // activate timer1
	delay = SYSCTL_RCGCTIMER_R;          // allow time to finish activating
	TIMER1_CTL_R &= ~TIMER_CTL_TAEN; // disable timer1A during setup
	TIMER1_CFG_R = 0;                // configure for 32-bit timer mode
	TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD; //activate periodic timer mode 
  TIMER1_TAILR_R = period - 1;         // start value 
  TIMER1_IMR_R |= TIMER_IMR_TATOIM;// enable timeout (rollover) interrupt
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;// clear timer1A timeout flag
  TIMER1_CTL_R |= TIMER_CTL_TAEN;  // enable timer1A 32-b, periodic, interrupts
	
}

int millisecondsToCount(uint32_t period)
{
	return period * MILLISECONDCOUNT; 
	
}

//input:
//task: pointer to a function
//period: 32 bit number for clock period
//priority: will be a three bit number from 0 to 7
//Question: uin8_t fine? priority is only 3 bits 
int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority){
	//enable timer but not interrupts
	Timer1A_Init(millisecondsToCount(period)); //converts the period from milliseconds to cycles
	// enable interrupts for timer 
	priority &= 0x7; //keep the last 3 bits
	NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF1FFF)| priority<<15 ; // bit 15, 14, 13 for Timer1A 
  NVIC_EN0_R |=	1<<21;              // enable interrupt 21 in NVIC in a friendly manner
	//Set taskToDo global variable
	taskToDo = task;
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
	timerCounter++;
	//call the task set at initialization
	taskToDo();
}

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value)
{
	OS_DisableInterrupts(); // Disable interrupts
	(*semaPt).Value = value;
	OS_EnableInterrupts();
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
	OS_DisableInterrupts();
	(*semaPt).Value ++;
	OS_EnableInterrupts();

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
	//threadPool[uniqueId].nextTCB = &threadPool[(uniqueId + 1) % NUMBEROFTHREADS ]; //address of next TCB
	threadPool[uniqueId].id = uniqueId; //unique id
	threadPool[uniqueId].active = 1;
	threadPool[uniqueId].sleepState = 0; //flag
	threadPool[uniqueId].priority = priority; 
	threadPool[uniqueId].blockedState = 0; //flag
	SetInitialStack(&threadPool[uniqueId], stackSize);
	threadPool[uniqueId].stack[stackSize - 2] = (uint32_t)task; //push PC
	//RunPt = &threadPool[uniqueId];
	uniqueId++;
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
