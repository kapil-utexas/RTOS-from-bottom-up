// OS.c
// Runs on TM4C123
// Provide a function that initializes Timer1A to trigger a
// periodic software task.
// Ramon and Kapil 
// February 3, 2016


#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "OS.h"

#define MILLISECONDCOUNT 80000
#define STACKSIZE 256

static void(*taskToDo)(void); //function pointer which takes void argument and returns void
static uint32_t timerCounter = 0;
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts

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

struct TCB {
	uint32_t stack [STACKSIZE];
	uint32_t * SP; //local stack pointer
	struct TCB * previousTCB; //pointer to previous TCB
	struct TCB * nextTCB; //pointer to next TCB
	uint32_t id; //unique id
	uint8_t sleepState; //flag
	uint8_t priority; 
	uint8_t blockedState; //flag
};

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
	static uint8_t uniqueId=0; //make unique ids
	struct TCB newTCB = {
	.stack = {0}, //possible bug
	.SP = 0, //local stack pointer
	.previousTCB = 0, //pointer to previous TCB
	.nextTCB = 0, //pointer to next TCB
	.id = uniqueId++, //unique id
	.sleepState = 0, //flag
	.priority = priority, 
	.blockedState = 0 //flag
	};
	newTCB.SP = newTCB.stack + stackSize - 1;
	
	return 0;
}
