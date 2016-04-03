// Lab2.c
// Runs on LM4F120/TM4C123
// Real Time Operating System for Labs 2 and 3
// Lab2 Part 1: Testmain1 and Testmain2
// Lab2 Part 2: Testmain3 Testmain4  and main
// Lab3: Testmain5 Testmain6, Testmain7, and main (with SW2)

// Jonathan W. Valvano 1/31/14, valvano@mail.utexas.edu
// EE445M/EE380L.6 
// You may use, edit, run or distribute this file 
// You are free to change the syntax/organization of this file

// LED outputs to logic analyzer for OS profile 
// PF1 is preemptive thread switch
// PF2 is periodic task, samples PD3
// PF3 is SW1 task (touch PF4 button)

// Button inputs
// PF0 is SW2 task (Lab3)
// PF4 is SW1 button input

// Analog inputs
// PD3 Ain3 sampled at 2k, sequencer 3, by DAS software start in ISR
// PD2 Ain5 sampled at 250Hz, sequencer 0, by Producer, timer tigger
#include "OS.h"
#include "inc/tm4c123gh6pm.h"
#include "ST7735.h"
#include "diskio.h"
#include "ff.h"
#include "loader.h"
#include "ADC.h"
#include "UART.h"
#include "heap.h"
#include <string.h> 

//*********Prototype for FFT in cr4_fft_64_stm32.s, STMicroelectronics
void cr4_fft_64_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);
//*********Prototype for PID in PID_stm32.s, STMicroelectronics
short PID_stm32(short Error, short *Coeff);

unsigned long NumCreated;   // number of foreground threads created
unsigned long PIDWork;      // current number of PID calculations finished
unsigned long FilterWork;   // number of digital filter calculations finished
unsigned long NumSamples;   // incremented every ADC sample, in Producer
#define FS 400            // producer/consumer sampling
#define RUNLENGTH (20*FS) // display results and quit when NumSamples==RUNLENGTH
// 20-sec finite time experiment duration 

#define PERIOD TIME_500US // DAS 2kHz sampling period in system time units
long x[64],y[64];         // input and output arrays for FFT

//---------------------User debugging-----------------------
unsigned long DataLost;     // data sent by Producer, but not received by Consumer
long MaxJitter;             // largest time jitter between interrupts in usec


unsigned long JitterHistogram[JITTERSIZE]={0,};
#define PB2  (*((volatile unsigned long *)0x40005010))
#define PB3  (*((volatile unsigned long *)0x40005020))
#define PB4  (*((volatile unsigned long *)0x40005040))
#define PB5  (*((volatile unsigned long *)0x40005080))
	



void PortE_Init(void){ unsigned long volatile delay;
  SYSCTL_RCGC2_R |= 0x10;       // activate port E
  delay = SYSCTL_RCGC2_R;        
  delay = SYSCTL_RCGC2_R;         
  GPIO_PORTE_DIR_R |= 0x0F;    // make PE3-0 output heartbeats
  GPIO_PORTE_AFSEL_R &= ~0x0F;   // disable alt funct on PE3-0
  GPIO_PORTE_DEN_R |= 0x0F;     // enable digital I/O on PE3-0
  GPIO_PORTE_PCTL_R = ~0x0000FFFF;
  GPIO_PORTE_AMSEL_R &= ~0x0F;;      // disable analog functionality on PF
}

void PortF_Init(void){ volatile unsigned long delay;
  SYSCTL_RCGC2_R |= 0x20;       // activate port F
  delay = SYSCTL_RCGC2_R;        
  delay = SYSCTL_RCGC2_R;         
  GPIO_PORTF_DIR_R |= 0x0E;    // make PE3-0 output heartbeats
  GPIO_PORTF_AFSEL_R &= ~0x0E;   // disable alt funct on PE3-0
  GPIO_PORTF_DEN_R |= 0x0E;     // enable digital I/O on PE3-0
  GPIO_PORTF_PCTL_R = ~0x0000FFF0;
  GPIO_PORTF_AMSEL_R &= ~0x0E;      // disable analog functionality on PF
}

void PortB_Init(void){ unsigned long volatile delay;
  SYSCTL_RCGC2_R |= 0x02;       // activate port B
  delay = SYSCTL_RCGC2_R;        
  delay = SYSCTL_RCGC2_R;         
  GPIO_PORTB_DIR_R |= 0x3C;    // make PB5-2 output heartbeats
  GPIO_PORTB_AFSEL_R &= ~0x3C;   // disable alt funct on PB5-2
  GPIO_PORTB_DEN_R |= 0x3C;     // enable digital I/O on PB5-2
  GPIO_PORTB_PCTL_R = ~0x00FFFF00;
  GPIO_PORTB_AMSEL_R &= ~0x3C;      // disable analog functionality on PB
}



//------------------Task 2--------------------------------
// background thread executes with SW1 button
// one foreground task created with button push
// foreground treads run for 2 sec and die
// ***********ButtonWork*************
void ButtonWork(void){
unsigned long myId = OS_Id(); 
  PB3 ^= 0x08;
  ST7735_Message(1,0,"NumCreated =",NumCreated); 
  PB3 ^= 0x08;
  OS_Sleep(50);     // set this to sleep for 50msec
  ST7735_Message(1,1,"PIDWork     =",PIDWork);
  ST7735_Message(1,2,"DataLost    =",DataLost);
  ST7735_Message(1,3,"Jitter 0.1us=",MaxJitter);
  PB3 ^= 0x08;
  OS_Kill();  // done, OS does not return from a Kill
} 

//************SW1Push*************
// Called when SW1 Button pushed
// Adds another foreground task
// background threads execute once and return
void SW1Push(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddThread(&ButtonWork,100,2)){
      NumCreated++; 
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}
//************SW2Push*************
// Called when SW2 Button pushed, Lab 3 only
// Adds another foreground task
// background threads execute once and return
void SW2Push(void){
  if(OS_MsTime() > 20){ // debounce
    if(OS_AddThread(&ButtonWork,100,2)){
      NumCreated++; 
    }
    OS_ClearMsTime();  // at least 20ms between touches
  }
}
//--------------end of Task 2-----------------------------


//------------------Task 4--------------------------------
// foreground thread that runs without waiting or sleeping
// it executes a digital controller 
void IdleTask(void){ 
  for(;;){
		PB2 ^= 0x04;
	}          // idle
}
//--------------end of Task 4-----------------------------

//------------------Task 5--------------------------------
// UART background ISR performs serial input/output
// Two software fifos are used to pass I/O data to foreground
// The interpreter runs as a foreground thread
// The UART driver should call OS_Wait(&RxDataAvailable) when foreground tries to receive
// The UART ISR should call OS_Signal(&RxDataAvailable) when it receives data from Rx
// Similarly, the transmit channel waits on a semaphore in the foreground
// and the UART ISR signals this semaphore (TxRoomLeft) when getting data from fifo
// Modify your intepreter from Lab 1, adding commands to help debug 
// Interpreter is a foreground thread, accepts input from serial port, outputs to serial port
// inputs:  none
// outputs: none
#define MESSAGELENGTH 20
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}
void LCD_test(uint8_t device, char * message)
{
	ST7735_Message (device, 0, message, strlen(message));
}
void dummy(void){}; //dummy function for user task
	
	
char processName [20];
static const ELFSymbol_t symtab[] = {
{ "ST7735_Message", ST7735_Message }
};

static FATFS g_sFatFs;
FRESULT MountFresult;
void Interpreter(void)    // just a prototype, link to your interpreter
{
	//uint32_t stringSize;
	//uint32_t adcVoltage;
	//uint8_t deviceChosen;
	uint8_t taskAddedBefore = 0;
	uint8_t commandChosen = 0;
	//char message[MESSAGELENGTH] = "";
	OutCRLF();
	UART_OutString("Input Command: ");
	while(1){
		OutCRLF();
		//UART_OutString("Commands: 0 - ADC, 1 - LCD, 2 - Time");
		OutCRLF();
		commandChosen = UART_InChar();
		switch(commandChosen)
		{
			case '0':
				MountFresult = f_mount(&g_sFatFs, "", 0);
				if(MountFresult){
					//ST7735_DrawString(0, 0, "f_mount error", ST7735_Color565(0, 0, 255));
					while(1){};
				}
				//UART_OutString("Enter program name:");
				//OutCRLF();
				//UART_InString(processName,19);
				ELFEnv_t env = { symtab, 1 };
				exec_elf("Proc.axf", &env);
				break;
			case '1':
				break;
			case '2':
				if(!taskAddedBefore){
					OS_AddPeriodicThread(dummy, 5, 1);
					taskAddedBefore = 1;
				}
				OutCRLF();
				UART_OutUDec(OS_ReadPeriodicTime());
				OutCRLF();
				break;
			case '3':
				UART_OutString("NumSamples: ");
				UART_OutUDec(NumSamples);
				OutCRLF();
				break;
			case '4':
				UART_OutString("Jitter: ");
				UART_OutUDec(MaxJitter);
				OutCRLF();
				break;
			case '5':
				UART_OutString("DataLost: ");
				UART_OutUDec(DataLost);
				OutCRLF();
				break;
			case '6':
				UART_OutString("FilterWork: ");
				UART_OutUDec(FilterWork);
				OutCRLF();
				break;
			case '7':
				UART_OutString("NumCreated: ");
				UART_OutUDec(NumCreated);
				OutCRLF();
				break;
			case '8':
				for(int i = 0; i<64; i++)
				{
					UART_OutUDec(x[i]);
					OutCRLF();
				}
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

//*******************final user main DEMONTRATE THIS TO TA**********
int main(void){ 
  OS_Init();           // initialize, disable interrupts
  PortB_Init();
  DataLost = 0;        // lost data between producer and consumer
  NumSamples = 0;
  MaxJitter = 0;       // in 1us units
	Heap_Init();
//********initialize communication channels
  OS_MailBox_Init();
  OS_Fifo_Init(32);    // ***note*** 4 is not big enough*****
	ST7735_InitR(INITR_REDTAB);				   // initialize LCD
	UART_Init();              					 // initialize UART
//*******attach background tasks***********
  OS_AddSW1Task(&SW1Push,2);
  OS_AddSW2Task(&SW2Push,2);  // add this line in Lab 3
  //10 will run every .5ms... decrease in period occurss every .5 ms
	//this unit can be changed in OSLAunch
  NumCreated = 0 ;
// create initial foreground threads
  NumCreated += OS_AddThread(&Interpreter,128,2); 
  NumCreated += OS_AddThread(&IdleTask,128,7);  // Lab 3, make this lowest priority
 
  OS_Launch(TIME_2MS); // doesn't return, interrupts enabled in here
  return 0;            // this never executes
}



