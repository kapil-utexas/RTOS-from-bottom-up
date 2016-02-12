// UARTIntsTestMain.c
// Runs on LM4F120/TM4C123
// Tests the UART0 to implement bidirectional data transfer to and from a
// computer running HyperTerminal.  This time, interrupts and FIFOs
// are used.
// Daniel Valvano
// September 12, 2013

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2015
   Program 5.11 Section 5.6, Program 3.10

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

// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1

#include <stdint.h>
#include <string.h>
#include "PLL.h"
#include "ADC.h"
#include "UART.h"
#include "OS.h"
#include "ST7735.h"
#include "inc/tm4c123gh6pm.h"

#define MESSAGELENGTH 20

void EnableInterrupts(void);  // Enable interrupts
void dummy(void){}; //dummy function for user task

//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}
	

void LCD_test(uint8_t device, char * message)
{
	ST7735_Message (device, 0, message, strlen(message));
}

void PortF_Init(void){ unsigned long volatile delay;
  SYSCTL_RCGC2_R |= 0x20;       // activate port F
  delay = SYSCTL_RCGC2_R;        
  delay = SYSCTL_RCGC2_R;         
  GPIO_PORTF_DIR_R |= 0x04;    // make PF2 output heartbeats
  GPIO_PORTF_AFSEL_R &= ~0x04;   // disable alt funct on PE3-0
  GPIO_PORTF_DEN_R |= 0x04;     // enable digital I/O on PE3-0
  GPIO_PORTF_PCTL_R = ~0x00000F00;
  GPIO_PORTF_AMSEL_R &= ~0x04;;      // disable analog functionality on PF
}


int main()
{
	uint32_t stringSize;
	uint32_t adcVoltage;
	uint8_t deviceChosen;
	uint8_t taskAddedBefore = 0;
	char message[MESSAGELENGTH] = "";
  PLL_Init(Bus80MHz);                  // set system clock to 80 MHz
	PortF_Init();
  ST7735_InitR(INITR_REDTAB);				   // initialize LCD
	ADC0_InitTimer0ATriggerSeq3(0,800000);
	UART_Init();              					 // initialize UART
	EnableInterrupts();
	OutCRLF();
	UART_OutString("Interpreter: ");
	while(1){
		OutCRLF();
		UART_OutString("Commands: 0 - ADC, 1 - LCD, 2 - Time");
		OutCRLF();
		switch(UART_InUDec())
		{
			case 0:
				OutCRLF();
				UART_OutString("ADC Voltage = ");
				ADC_Open(2);
				adcVoltage = (ADC_In() *3300) / 4095; //convert to mV
				UART_OutUDec(adcVoltage);
				break;
			case 1:
				OutCRLF();
				UART_OutString("Enter LCD device 0 or 1: ");
				deviceChosen = UART_InUDec();
				OutCRLF();
				UART_OutString("Enter message: ");
				UART_InString(message, MESSAGELENGTH);
				OutCRLF();
				stringSize = strlen(message);
				if(stringSize > 20)
				{
					OutCRLF();
					UART_OutString("String too long, only 20 chars will be printed...");
					OutCRLF();
				}
				LCD_test(deviceChosen, message); //prints to lcd
				break;
			case 2:
				if(!taskAddedBefore){
					OS_AddPeriodicThread(dummy, 5, 1);
					taskAddedBefore = 1;
				}
				OutCRLF();
				UART_OutUDec(OS_ReadPeriodicTime());
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



//debug code for UART
int main2(void){
  char i;
  char string[20];  // global to assist in debugging
  uint32_t n;
  PLL_Init(Bus50MHz);       // set system clock to 50 MHz
  UART_Init();              // initialize UART
  OutCRLF();
  for(i='A'; i<='Z'; i=i+1){// print the uppercase alphabet
    UART_OutChar(i);
  }
  OutCRLF();
  UART_OutChar(' ');
  for(i='a'; i<='z'; i=i+1){// print the lowercase alphabet
    UART_OutChar(i);
  }
  OutCRLF();
  UART_OutChar('-');
  UART_OutChar('-');
  UART_OutChar('>');
  while(1){
    UART_OutString("InString: ");
    UART_InString(string,19);
    UART_OutString(" OutString="); UART_OutString(string); OutCRLF();

    UART_OutString("InUDec: ");  n=UART_InUDec();
    UART_OutString(" OutUDec="); UART_OutUDec(n); OutCRLF();

    UART_OutString("InUHex: ");  n=UART_InUHex();
    UART_OutString(" OutUHex="); UART_OutUHex(n); OutCRLF();

  }
}
