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
#define PE1  (*((volatile unsigned long *)0x40024008))
#define PE2  (*((volatile unsigned long *)0x40024010))

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
	GPIO_PORTF_DATA_R ^= 0x4;
	timerCounter++;
	//call the task set at initialization
	taskToDo();
	GPIO_PORTF_DATA_R ^= 0x4;

}
	