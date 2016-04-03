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
		EXTERN  OS_Id
		EXTERN  OS_Kill
		EXTERN  OS_Sleep
		EXTERN  OS_Time
		EXTERN  OS_AddThread
		EXTERN  processPool
        EXTERN  RunPt            ; currently running thread
		EXTERN  traverseSleep
		EXTERN  switched
		EXTERN  nextBeforeSwitch	
		EXTERN  getProcessIdOfRunningThread
		EXTERN  higherPriorityAdded
		EXTERN  SchedulerPt
		EXTERN  getNewR9
		EXTERN currentProcess
        EXPORT  OS_DisableInterrupts
        EXPORT  OS_EnableInterrupts
        EXPORT  PendSV_Handler
		EXPORT  SysTick_Handler
		EXPORT  StartOS
		EXPORT SVC_Handler
			
			


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
	PUSH{R4-R8}
	LDR R4, =RunPt
	LDR R4,[R4]
	;LDR R5, [R4,#540] ;load Process ID 
	PUSH {R0, LR}
	BL getProcessIdOfRunningThread
	MOV R5, R0
	POP {R0,LR}
	PUSH{R6}
	LDR R6, =currentProcess ;CHECK UIF WE NEED TO LDR AGAIN
	STR R5, [R6]
	POP{R6}
	;LDR R6, =processPool
	;LDR R6, [R6]
	;MOV R8, #17
	;MUL R7, R5, R8 ;17 is size of PCB
	;ADD R7, R7, #8 ;add offset for data segment
	;LDR R7, [R6,R7]
	;LDR R7,[R7]
	PUSH{R0,LR}
	BL getNewR9
	MOV R9, R0 ;update R9
	POP{R0,LR}
	POP{R4-R8}
	;need to set R9
	CPSIE I
	BX LR
	
	
SVC_Handler
	CPSID I
	LDR R12, [SP,#24]
	LDRH R12, [R12,#-2] 
	BIC R12, #0xFF00
	LDM SP, {R0-R3}
	PUSH{LR}
	CMP R12, #0
	BEQ OS_Id_Handler
	CMP R12, #1
	BEQ OS_Kill_Handler
	CMP R12, #2
	BEQ OS_Sleep_Handler
	CMP R12, #3
	BEQ OS_Time_Handler
	CMP R12, #4
	BEQ OS_AddThread_Handler
OS_Id_Handler
	BL OS_Id
	B Finished
OS_Kill_Handler
	BL OS_Kill	
	B Finished
OS_Sleep_Handler
	BL OS_Sleep
	B Finished	
OS_Time_Handler
	BL OS_Time
	B Finished	
OS_AddThread_Handler
	BL OS_AddThread
	B Finished
Finished	
	POP{LR}
	STR R0,[SP]
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
