// Switch.c
// Runs on TMC4C123
// Use GPIO in edge time mode to request interrupts on any
// edge of PF4 and start Timer0B. In Timer0B one-shot
// interrupts, record the state of the switch once it has stopped
// bouncing.
// Daniel Valvano
// May 3, 2015

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// PF4 connected to a negative logic switch using internal pull-up (trigger on both edges)
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "Switch.h"
#include "OS.h"
#define PF4                     (*((volatile uint32_t *)0x40025040))
#define PF0                     (*((volatile unsigned long *)0x40025004))


void WaitForInterrupt(void);  // low power mode

volatile static unsigned long TouchPF4;     // true on touch
volatile static unsigned long ReleasePF4;   // true on release
volatile static unsigned long TouchPF0;     // true on touch
volatile static unsigned long ReleasePF0;   // true on release
volatile static unsigned long LastPF4;      // previous
volatile static unsigned long LastPF0;      // previous
void (*TouchTaskPF4)(void);    // user function to be executed on touch
void (*TouchTaskPF0)(void);    // user function to be executed on touch

static uint8_t taskPriority;

static void GPIOArm(uint8_t priority){
  GPIO_PORTF_ICR_R = 0x11;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x11;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF1FFFFF)| ( (priority & 7) << 21 ); // (g) priority 5 23 22 21 1010
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC  
}
// Initialize switch interface on PF4 
// Inputs:  pointer to a function to call on touch (falling edge),
//          pointer to a function to call on release (rising edge)
// Outputs: none 
void Switch_Init(void(*touchtask)(void), uint8_t priority, uint8_t switchToAssign){
  // **** general initialization ****
  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
  while((SYSCTL_PRGPIO_R & 0x00000020) == 0){};
	GPIO_PORTF_LOCK_R = GPIO_LOCK_KEY;
	GPIO_PORTF_CR_R |= (0x11);  // 2b) enable commit for PF4 and PF0

  GPIO_PORTF_DIR_R &= ~0x11;    // (c) make PF4 and 0 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x11;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x11;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x000F000F; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x11;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x11;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R |= 0x11;     //     PF4 is both edges
  GPIOArm(priority);
	if(switchToAssign == 4)
	{
		TouchTaskPF4 = touchtask;           // user function 
	}
	else
	{
		TouchTaskPF0 = touchtask;
	}
  TouchPF4 = 0;                       // allow time to finish activating
	TouchPF0 = 0;
  ReleasePF4 = 0;
	ReleasePF0 = 0;
  LastPF4 = PF4;                      // initial switch state
	LastPF0 = PF0;
	taskPriority = priority;
 }

 void DebounceTask(void){ 
	OS_Sleep(10); //wait to stabilize
	LastPF4 = PF4;
	LastPF0 = PF0;
	GPIO_PORTF_ICR_R |= 0x11;
	GPIO_PORTF_IM_R |= 0x11; //enable interrupt	
	OS_Kill();
}
 
// Interrupt on rising or falling edge of PF4 (CCP0)
void GPIOPortF_Handler(void){
  GPIO_PORTF_IM_R &= ~0x11;     // disarm interrupt on PF4 
	if(GPIO_PORTF_RIS_R == 0x01)
	{
		if(LastPF0){    // 0x10 means it was previously released
			TouchPF0 = 1;       // touch occurred
			(*TouchTaskPF0)();  // execute user task
		}
	}
	else
	{
		if(LastPF4){    // 0x10 means it was previously released
			TouchPF4 = 1;       // touch occurred
			(*TouchTaskPF4)();  // execute user task
		}
		
	}
	OS_AddThread(&DebounceTask,128,taskPriority);
	
}


// Return current value of the switch 
// Repeated calls to this function may bounce during touch and release events
// If you need to wait for the switch, use WaitPress or WaitRelease
// Inputs:  none
// Outputs: false if switch currently pressed, true if released 
unsigned long Switch_Input(void){
  return PF4 | PF0;
}
