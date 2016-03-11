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
		EXTERN  PF3Address
		EXTERN  traverseSleep
		EXTERN  switched
		EXTERN  nextBeforeSwitch	
		EXTERN  higherPriorityAdded
		EXTERN  SchedulerPt
        EXPORT  OS_DisableInterrupts
        EXPORT  OS_EnableInterrupts
        EXPORT  PendSV_Handler
		EXPORT  SysTick_Handler
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
	LDR SP, [R2] ;load localSP
	POP {R4-R11}
	POP {R0-R3}
	POP {R12}
	POP {LR} ;trash LR
	POP {LR} ;PC on stack. the only register that is not trash. points to function that we want to run
	POP {R1} ;discards last one PSR
	CPSIE I 
	BX LR    ;LR contains the PC we want to return to
	


PendSV_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
	CPSID I
	PUSH{R4-R11} ;push remaining registers
	LDR R0, =RunPt ;old run pointer
	LDR R1, [R0] ;now R1 points the the current TCB running
				 ;this means R1 = TCB.localSP 	
	STR SP, [R1] ;store stack into local stack pointer of TCB
	;need to check if we went to sleep or killed 
	LDR R2, =switched ;8bit variable
	LDRB R3, [R2] ;loadvalue of flag
	CMP R3, #0
	BNE Switch_Routine
	LDR R2, =higherPriorityAdded
	LDRB R3, [R2] ;load value of flag... if 1, a higher priority was added and need to update runpt to beginning of list
	CMP R3, #0
	BNE Higher_Priority_Switch
	BEQ Normal_Context_Switch
Higher_Priority_Switch
	MOV R3,#0 ;constant to clear the flag
	STRB R3,[R2] ;clear the flag, flag was 1
	LDR R1, =SchedulerPt
	LDR R1, [R1] ;R1 now points to higher priority thread
	STR R1, [R0] ;update runPt
	LDR SP, [R1] ;localSp to SP
	B END_ROUTINE
Switch_Routine ;used for kill and sleep
	MOV R3,#0
	STRB R3, [R2] ;clear the flag
	LDR R1, =nextBeforeSwitch
	LDR R1, [R1] ;now points to tcb that we want to run
	STR R1, [R0] ;update runPT
	LDR SP, [R1]
	B END_ROUTINE
Normal_Context_Switch	
	LDR R1, [R1,#4] ;load nextpt
	STR R1, [R0] ;update RunPt
	LDR SP, [R1] ;R1 points to the first element of the struct we want to switch to
END_ROUTINE
	POP {R4-R11} ;pop remainining registers
	CPSIE I
	BX LR
	
	
SysTick_Handler                ; 1) Saves R0-R3,R12,LR,PC,PSR
	CPSID I
	;PUSH{R4-R11} ;push remaining registers
	;LDR R0, =RunPt ;old run pointer
	;LDR R1, [R0] ;now R1 points the the current TCB running
				 ;this means R1 = TCB.localSP 	
	;STR SP, [R1] ;store stack into local stack pointer of TCB
	;added
	PUSH{R0-R1, LR}
	BL 	traverseSleep
	POP{R0-R1, LR}
	;added
	;LDR R1, [R1,#4] ;load nextpt
	;STR R1, [R0] ;update RunPt
	;LDR SP, [R1] ;R1 points to the first element of the struct we want to switch to
	;POP {R4-R11} ;pop remainining registers
	CPSIE I
	BX LR


    ALIGN
    END
