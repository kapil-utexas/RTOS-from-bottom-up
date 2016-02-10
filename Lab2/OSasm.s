;/*****************************************************************************/
; OS.s: low-level OS commands, written in assembly                       */
; Runs on LM4F120/TM4C123
; OS functions.
; Ramon Villar, Kapil
; February, 9, 2016


        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread
        EXPORT  OS_DisableInterrupts
        EXPORT  OS_EnableInterrupts
        EXPORT  PendSV_Handler
		EXPORT  StartOS
			
			


OS_DisableInterrupts
        CPSID   I
        BX      LR


OS_EnableInterrupts
        CPSIE   I
        BX      LR

StartOS
	LDR R0, =RunPt
	LDR R2, [R0]
	LDR SP, [R2]
	POP {R4-R11}
	POP {R0-R3}
	POP {R12}
	POP {LR}
	POP {LR} ;PC!
	POP {R1} ;discards last one PSR
	CPSIE I
	BX LR
	


PendSV_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
	CPSID I
	;we can assume that R0-R3, R12, LR, PC, PSRX are in stack because of interrupt
	PUSH{R4-R11} ;push remaining registers
	LDR R0, =RunPt ;old run pointer
	LDR R1, [R0] ;now R1 points the the current TCB running
				 ;this means R1 = TCB.localSP 	
	STR SP, [R1] ;store stack into local stack pointer of TCB
	LDR R1, [R1,#4]	;load next to run
	STR R1, [R0] ;update RunPt
	LDR SP, [R1] ;R1 points to the first element of the struct we want to switch to
	POP {R4-R11} ;pop remainining registers
	CPSIE I
	BX LR

    ALIGN
    END
