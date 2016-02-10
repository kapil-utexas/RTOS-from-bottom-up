;/*****************************************************************************/
; OSasm.s: low-level OS commands, written in assembly                       */
; Runs on LM4F120/TM4C123
; OS functions.
; Ramon Villar, Kapil
; February, 9, 2016


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
        ;EXPORT  OS_DisableInterrupts
        ;EXPORT  OS_EnableInterrupts
        ;EXPORT  SysTick_Handler
		EXPORT PendSV_Handler
		;EXPORT Context_Switch



;SysTick_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
PendSV_Handler
	CPSID I
	;we can assume that R0-R3, R12, LR, PC, PSRX are in stack because of interrupt
	PUSH{R4-R11} ;push remaining registers
	LDR R0, =RunPt ;old run pointer
	LDR R1, [R0] ;now R1 points the the current TCB running
				 ;this means R1 = TCB.localSP 	
	STR SP, [R1] ;store stack into local stack pointer of TCB
	LDR R0, [R1,#4] 
	LDR R1, [R0] ;R1 points to the first element of the struct we want to switch to
	LDR SP, [R1] ; SP = new stack pointer
	POP {R4-R11} ;pop remainining registers
	CPSIE I
	BX LR

    ALIGN
    END
