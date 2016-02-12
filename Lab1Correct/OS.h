// OS.c
// Runs on TM4C123
// Provide a function that initializes Timer1A to trigger a
// periodic software task.
// Ramon and Kapil 
// February 3, 2016

//will create a periodic user task
//input:
//task: pointer to a function
//period: 32 bit number for clock period
//priority: three bit number from 0 to 7
int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority);

//will clear global timer 
void OS_ClearPeriodicTime(void);

//Will return the current 32-bit global counter. 
//The units of this system time are the period of
//interrupt passed since initializing with OS_AddPeriodicThread or last reset.
unsigned long OS_ReadPeriodicTime(void);

//will run user task
void Timer1A_Handler(void);
