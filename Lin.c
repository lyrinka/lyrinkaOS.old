// Lin Architecture version 4.0.0 for lyrinka OS 
/* The Lin Architecture Framework. 
	Major changes in stack data structures 
	providing a smart and flexiable interface 
	but incompatable with older versions. 
	
	The memory operation and messaging operation still uses micro lib, 
	which may hit performance. 
	Please enable micro lib and configure sufficent heap area in startup_stm32f10x_md.s 
	
	Release notes: 
	
	<4.0.0 > 190301 ThreadExit changed to ProcessExit. 
					Minor modification on task prototype, easy access of Self pointer, now: 
						void funcTask(TASK Self, int Arg0, int Arg1, u32 Cnt); 
	<3.1.3 > 190206 Changed some debug info to unsigned. 
	<3.1.2 > 190127 Changed TCB Data Structure for Event Managements. 
									Added Event Control Block Data Structure. 
									DataStructure optimized for Operating System using Lin. 
									Updated StkInit for new data structures. New Tasks are created Dead (i.e. NULLS on all surrounding pointers) 
									Initializations of chained lists are included for Lint is an addon of Lin. 
									But OS related control information is not initialized. These are thrown directly to the operating system. 
	<3.1.1 > 181225 Added support in data structure for priority queue and waiting list chained lists. 
									Added macros for entering/exiting critical regions. 
									Code not changed. 
	<3.0.2 > 181115	Debugged and fully commented. 
	<3.0.1 > 181113	Using Message Pool & Carrier design 
									Using Single-chained Lists for Message Carriers. 
	<3.0.0 > 181112	Copies of last version 2.1.2 
*/ 

#include <stm32f10x.h> 
#include "Lin.h" 


// using micro lib 
void * 	malloc(u32 Size); 
void 		free(void * Mem); 


// Private variables & functions 
TASK 					Lin_CurrTask; 		// Indicates the Current running Task reference 
TASK 					Lin_NextTask; 		// Indicates the Next Task to be run referrence 
TASK 					Lin_MainTask; 		// Indicates the Main Task reference 
void * 				Lin_TaskLoader; 	// Storage for MSP on loading of the first task 

int Lin_DebugMemLeak; 					// Shows any allocations without deallocation 
u32 Lin_DebugMemAllocTimes; 		// Total times of memory allocations 
u32 Lin_DebugMsgOpTimes; 				// Total times of message queue operations 
u32 Lin_DebugCtxSwTimes; 				// Total times of context switching 

void 					Lin_InitMem		(u8 * MemS, u8 * MemE); 						// Initializes memory framework 
void 					Lin_InitSw		(void); 														// Initializes context switching framework 
void 					Lin_InitMsg		(u32 PoolSize); 										// Initializes message carrier pool framework 
TASK 					Lin_StkInit		(u8 * Mem, u32 Size, void * Func); 	// Initialization of a new Task 
int 					Lin_Call			(TASK Task); 												// Request for first context switch from MSP 
Lin_MsgBlk * 	Lin_MsgPoolGet(void); 														// Get carrier block from message pool 
void 					Lin_MsgPoolRet(Lin_MsgBlk * MsgBlk); 							// Return carrier block from message pool 
void 					Lin_MsgEnQ		(TASK Task, Lin_MsgBlk * MsgBlk); 	// Enqueue message carrier 
void 					Lin_MsgEnQF		(TASK Task, Lin_MsgBlk * MsgBlk); 	// Enqueue message carrier, but at the front 
Lin_MsgBlk * 	Lin_MsgDeQ		(TASK Task); 												// Dequeue message carrier 

// Exception Handlers 
void 												PendSV_Handler(void); 	// Pending Service Handler 
void 												SVC_Handler		(void); 	// System Service Call Handler 

// Macros 
#define Lin_CritEnter() u32 __Lin_IE = __get_PRIMASK(); __disable_irq() 	// Enter critical region 
#define Lin_CritExit() __set_PRIMASK(__Lin_IE) 														// Exit critical region 

// External Functions 
extern void SVC_ProxyCaller(u8 ID, u32 * StkF); 	// Other SVC Calls redirected to here. weakly defined. 


// Function Definitions. 
// ************************************************************************************ 
// System Startup: 
// Initialize the whole system including: 
/*	Memory Management 
		Task Management 
		Interrupts & Context Switching 
		Inter-Task Messaging 
*/
void Lin_Init(void){ 
	Lin_InitMem((u8 *)Lin_MemStart, (u8 *)Lin_MemEnd); 
	Lin_InitSw(); 
	Lin_InitMsg(Lin_MsgPoolSize); 
}
// End of a section. 


// ************************************************************************************ 
// Memory Managements: 
// Allocate a chunk of memory: 
/*	If failed the function return a NULL pointer. 
		Sizes are in Bytes. 
		Returned pointer is Word-Aligned. 
*/
void * Lin_MemAlloc(u32 Size){ 
	Lin_CritEnter(); 
	Lin_DebugMemLeak++; 
	Lin_DebugMemAllocTimes++; 
	void * Mem = malloc(Size); 
	Lin_CritExit(); 
	return Mem; 
}
// Free up a chunk of memory: 
/*	If input pointer is not valid 
		the function ignore the freeing operation. 
*/
void Lin_MemFree(void * Mem){ 
	Lin_CritEnter(); 
	Lin_DebugMemLeak--; 
	free(Mem); 
	Lin_CritExit(); 
}
// End of a section. 


// ************************************************************************************ 
// Task Management: 
// Create ane Initialize a new Task: 
/*	This function allocates memory. 
		StkSize in Bytes. 
		PC could be any function pointer but 
		the best would be 
		void Task(int Arg0, int Arg1, u32 Counter, TASK * Self); 
*/
TASK Lin_New(u32 StkSize, void * PC){ 
	void * Mem = Lin_MemAlloc(StkSize); 
	if(Mem == NULL) return (TASK)NULL; 
	return Lin_StkInit(Mem, StkSize, PC); 
}
// Set the arguments of a Task. 
/*	These values are read only on the 
		re-entering of a task. 
*/
void Lin_SetArgs(TASK Task, int Arg0, int Arg1){ 
	Task->Arg0 = Arg0; 
	Task->Arg1 = Arg1; 
}
// Set the entrance point of a Task. 
/*	The entrance point is not active till the
		re-entering of a task. 
*/
void Lin_SetPC(TASK Task, void * PC){ 
	Task->PC = PC; 
}
// Enter a Task from normal program. 
/*	The normal C program uses MSP. 
		The Task Framework uses separate PSPs for each Task. 
		The Task being entered first becomes the MainTask, 
		providing easy access from anywhere within the framework. 
		Like Standard C Call, 
		The task framework may return an int value. 
*/
int Lin_Enter(TASK Task){ 
	Lin_MainTask = Task; 
	return Lin_Call(Task); 
}
// Return to normal program from any Task. 
/*	Like Standard C Call, 
		The task framework may return an int value. 
		It is the normal program's responsibility 
		to release all Task Stack Memories. 
*/
__asm void Lin_Return(int retval){ 
		SVC		SVCn_LinTrigger 
		BX		LR 
}
// Delete a task by releasing all its memory. 
/*	This function does not really disabling the task. 
		It just releases all the associated memory, 
		including StackMemory and all Message Blocks(Carriers). 
		Re-entering this task may cause serious violations 
		for the stack and the TCB structure may be corrupted 
		by other allocations of the memory. 
*/
void Lin_Delete(TASK Task){ 
	Lin_CritEnter(); 
	while(Task->MsgQty) Lin_MsgGet(Task); 
	Lin_MemFree(Task->ECB); 
	Lin_CritExit(); 
}
// End of a section. 


// ************************************************************************************ 
// Context Switching: 
// Switching inbetween Task. 
__asm void Lin_Switch(TASK Task){ 
		SVC		SVCn_LinSwitch 
		BX		LR 
}
// Switching inbetween Task from ISR. 
void Lin_SwitchISR(TASK Task); 
/*	This function stays in the 
//	void SVC_Handler(void); 
//	for the optimization of speed. 
*/
// Switching to the MainTask. 
void Lin_Yield(void){ 
	Lin_Switch(Lin_MainTask); 
}
// Switching to the MainTask from ISR. 
void Lin_YieldISR(void){ 
	Lin_SwitchISR(Lin_MainTask); 
}
// End of a section. 


// ************************************************************************************ 
// Miscellaneous: 
// Get Current Executing Task. 
/*	Still applicable from ISR, 
		Indicating the interrupted Task. 
*/
TASK Lin_GetCurrTask(void){ 
	return Lin_CurrTask; 
}
// Get MainTask. 
TASK Lin_GetMainTask(void){ 
	return Lin_MainTask; 
}
// End of a section. 


// ************************************************************************************ 
// Inter-Task Messaging: 
// Enqueue Message to a Task. 
/*	If there is not enough memory for getting the Carrier Block, 
		The function returns -1, 
		Else it returns 0. 
		Same for all other functions indicated below. 
*/
int Lin_MsgPut(TASK Task, MSG Msg){ 
	Lin_MsgBlk * MsgBlk = Lin_MsgPoolGet(); 
	if(MsgBlk == NULL) return -1; 
	MsgBlk->Msg = Msg; 
	Lin_MsgEnQ(Task, MsgBlk); 
	return 0; 
}
// Enqueue Message to a Task, but at opposite front. 
/*	Note: Same as MsgPut. 
*/
int Lin_MsgPutF(TASK Task, MSG Msg){ 
	Lin_MsgBlk * MsgBlk = Lin_MsgPoolGet(); 
	if(MsgBlk == NULL) return -1; 
	MsgBlk->Msg = Msg; 
	Lin_MsgEnQF(Task, MsgBlk); 
	return 0; 
}
// Enqueue Message to MainTask. 
/*	Note: Same as MsgPut. 
*/
int Lin_MsgSubmit(MSG Msg){ 
	return Lin_MsgPut(Lin_MainTask, Msg); 
}
// Enqueue Message to MainTask, but at opposite front. 
/*	Note: Same as MsgPut. 
*/
int Lin_MsgSubmitF(MSG Msg){ 
	return Lin_MsgPutF(Lin_MainTask, Msg); 
}
// Get number in the waiting queue of the Current Task. 
/*	Still applicable from ISR, 
		Indicating the queue status of the interrupted Task. 
*/
u32 Lin_MsgQty(void){ 
		return Lin_CurrTask->MsgQty; 
}
// Dequeue Message from any Task, and releasing associated memory. 
/*	If there is no message in the queue, 
		the function returns an empty Message content. 
		Always check queue status before dequeueing the message queue. 
		Same for all other functions indicated below. 
*/
MSG Lin_MsgGet(TASK Task){ 
	Lin_MsgBlk * MsgBlk = Lin_MsgDeQ(Task); 
	MSG Msg; 
	Msg.Src = 0; 
	Msg.Cmd = 0; 
	Msg.Pld = NULL; 
	if(MsgBlk == NULL) return Msg; 
	Msg = MsgBlk->Msg; 
	Lin_MsgPoolRet(MsgBlk); 
	return Msg; 
}
// Dequeue Message from Current Task, and releasing associated memory. 
/*	Note: See MsgGet. 
*/
MSG	Lin_MsgRecv(void){ 
	return Lin_MsgGet(Lin_CurrTask); 
}
// Preview Message in the Current Task queue. 
/*	Previewing does not remove the carrier block from the queue. 
		It just snaps the value into a variable then returns it. 
		Note: See MsgGet. 
*/
MSG Lin_MsgPrvw(void){ 
	Lin_CritEnter(); 
	Lin_MsgBlk * MsgBlk = Lin_CurrTask->MsgHead; 
	MSG Msg; 
	Msg.Src = 0; 
	Msg.Cmd = 0; 
	Msg.Pld = NULL; 
	if(MsgBlk != NULL) Msg = MsgBlk->Msg; 
	Lin_CritExit(); 
	return Msg; 
}
// End of a section. 


// ************************************************************************************ 
// Private Functions: 
// Initialize the Memory Management framework. 
// Currently dummy. 
static void Lin_InitMem(u8 * MemS, u8 * MemE){ 
	Lin_DebugMemLeak = 0; 
	Lin_DebugMemAllocTimes = 0; 
}
// Initilize the Context Switching Environment. 
// 1.0.6 Framework not changed. 
static void Lin_InitSw(void){ 
	// User Functions - NVIC and Handler Initialization 
	SCB->CCR |= 1 << 9; 																	// Disable Stack DW Align 
	SCB->AIRCR = (5 << 8) | (0x05FA << 16); 							// Priority Group 5, 2bits+2bits 
	SCB->ICSR = 1 << 27; 																	// PendSV Pend Clear 
	SCB->SHCSR &= ~(1 << 15); 														// SVC Pend Clear 
	SCB->SHP[12+PendSV_IRQn] = (3 << 6) | (3 << 4) | 15; 	// PendSV Lowest Priority 
	SCB->SHP[12+SVCall_IRQn] = (3 << 6) | (2 << 4); 			// SVC Pp same as PendSV, Sp higher than PendSV 
	Lin_CurrTask = NULL; 
	Lin_NextTask = NULL; 
	Lin_MainTask = NULL; 
	Lin_TaskLoader = NULL; 
	Lin_DebugCtxSwTimes = 0; 
}
// Initilize the Messaging Framework. 
// Currently dummy. 
static void Lin_InitMsg(u32 PoolSize){ 
	Lin_DebugMsgOpTimes = 0; 
}
// Initilize the Stack of a new Task. 
// ProcessExit routine also included. 
static __asm TASK Lin_StkInit(u8 * Memory, u32 Size, void * funcPtr){ 
		ADD		R3,  R0, R1 		
		SUB		R3,  #64 
		STR		R0, [R3, #60] // SL 
		MOV		R0,  #0 
		STR		R0, [R3, #44] // RBN 
		STR		R0, [R3, #40] // LBN 
		STR		R0, [R3, #36] // Next 
		STR		R0, [R3, #32] // Prev 
		STR		R0, [R3, #28] // MsgTail 
		STR		R0, [R3, #24] // MsgHead
		STR		R0, [R3, #20] // MsgQty 
		STR		R0, [R3, #12] // Arg1 
		STR		R0, [R3, #08] // Arg0 
		MVN		R0,  R0 
		STR		R0, [R3, #16] // Cntr 
		STR		R2, [R3, #04] // PC 
		SUB		R0,  R3, #64 
		STR		R0, [R3] // SP 
		MOV		R0, #0x01000000 
		STR		R0, [R3, #-4] // xPSR 
		LDR		R0, =ProcessExit 
		STR		R0, [R3, #-8] // ThreadExit 
		MOV		R0,  R3 
		BX		LR 
		NOP 
ProcessExit 
		LDR		LR, =ProcessExit 	// Return to ProcessExit on next return 
		LDRD	R1, R2, [SP, #8] 	// Arg0->R1, Arg1->R2 
		LDR		R3, [SP, #16] 		// Load Counter 
		ADD		R3,  #1 					// Increase 
		STR		R3, [SP, #16] 		// Store back 
		MOV		R0,  SP 					// Self pointer in R0 
		LDR		PC, [SP, #4] 
}
// Request for changing the process stack and switch into a Task. 
static __asm int Lin_Call(TASK Task){ 
		SVC		SVCn_LinTrigger 
		BX		LR 
}
// Get Message Carrier Block from Pool. 
// Currently using direct memory allocation. 
static Lin_MsgBlk * Lin_MsgPoolGet(void){ 
	return (Lin_MsgBlk *)malloc(sizeof(Lin_MsgBlk)); 
}
// Returning Message Carrier Block to Pool. 
// Currently using directly memory free-up. 
static void Lin_MsgPoolRet(Lin_MsgBlk * MsgBlk){ 
	free(MsgBlk); 
}
// Enqueue the Message Carrier into a Task Message Queue. 
static void Lin_MsgEnQ(TASK Task, Lin_MsgBlk * MsgBlk){ 
	if(MsgBlk == NULL) return; 
	Lin_CritEnter(); 
	Task->MsgQty++; 
	Lin_MsgBlk * Prev = Task->MsgTail; 
	if(Prev == NULL) Task->MsgHead = MsgBlk; 
	else Prev->Next = MsgBlk; 
	Task->MsgTail = MsgBlk; 
	MsgBlk->Next = NULL; 
	Lin_DebugMsgOpTimes++; 
	Lin_CritExit(); 
}
// Enqueue the Message Carrier into a Task Message Queue, but at the front. 
static void Lin_MsgEnQF(TASK Task, Lin_MsgBlk * MsgBlk){ 
	if(MsgBlk == NULL) return; 
	Lin_CritEnter(); 
	Task->MsgQty++; 
	MsgBlk->Next = Task->MsgHead; 
	Task->MsgHead = MsgBlk; 
	if(Task->MsgTail == NULL) Task->MsgTail = MsgBlk; 
	Lin_DebugMsgOpTimes++; 
	Lin_CritExit(); 
}
// Dequeue the Message Carrier from a Task Message Queue. 
static Lin_MsgBlk * Lin_MsgDeQ(TASK Task){ 
	Lin_CritEnter(); 
	Lin_MsgBlk * MsgBlk = Task->MsgHead; 
	if(MsgBlk == NULL){ 
		Lin_CritExit(); 
		return NULL; 
	}
	Task->MsgQty--; 
	Task->MsgHead = MsgBlk->Next; 
	if(Task->MsgHead == NULL) Task->MsgTail = NULL; 
	MsgBlk->Next = NULL; 
	Lin_CritExit(); 
	Lin_DebugMsgOpTimes++; 
	return MsgBlk; 
}
// System Service Call Handler. 
// Other SVC Numbers, redirected to 
// void SVC_ProxyCaller(u8 ID, u32 * StkF); 
// SwitchISR function also located here for performance. 
// The 1.0.6 version framework not changed 
__asm void SVC_Handler(void){ 
		// SVC Call Handler 
		// Other SVC Numbers will trigger SVC_ProxyCaller(u8 ID, u32 * StkF); 
		TST		LR,  #4 
		ITE 	EQ 
		MRSEQ R1,  MSP 
		MRSNE R1,  PSP 								// Get HW Stack Frame Pointer. 
		LDR		R0, [R1, #24] 
		LDRB	R0, [R0, #-2] 					// Got HW StkF ptr in R1, SVC ID in R0. 
		CMP		R0,  #SVCn_LinSwitch 
		BNE		NotSwitch 							// Is Switch Op? 
Switch 														//  Do Switch Op 
		LDR		R0, [R1] 								//  Get Target Task Handle 
	EXPORT	Lin_SwitchISR 
Lin_SwitchISR 										//  Switches from ISR directly come here 
		LDR		R2, =Lin_NextTask 
		LDR		R1, [R2] 								//  Get Handle in Lin_NextTask 
		CMP		R0,  R1 								//  Handles Compared 
		ITTTT	NE 											//  If not equal- 
		STRNE	R0, [R2] 								//   put new handle in Lin_NextTask 
		LDRNE	R1, =0xE000ED00 
		MOVNE	R0,  #0x10000000 
		STRNE	R0, [R1, #4] 						//   PendSV Pend Set (SCB->ICSR.28) 
		BX		LR 											//  Then PendSV executes on demand 
NotSwitch 												// Not Switch Op 
		CMP		R0,  #SVCn_LinTrigger		//  Is Trigger Op? 
		BNE.W	SVC_ProxyCaller 				//   No, go to SVC_ProxyCaller 
		TST		LR,  #4 								//  Is Trigger Op Enter or Return? 
		BNE		Return 									//   Trigger Op is Enter 
		MSR		PSP, R1 								//    Set PSP as current pointer for CtxS 
		SUB		R2,  R1, #32 
		MSR		MSP, R2 								//    MSP Stack Growth for CtxS (8 words) 
		ORR 	LR,  #4 								//    Modify LR to use PSP 
	IMPORT	Lin_TaskLoader 
		LDR		R0, =Lin_TaskLoader 
		LDR		R2, =Lin_CurrTask 
		STR		R0, [R2] 								//    Set the storing handle to Lin_CurrTask 
		B			Switch 
Return 														//   Trigger Op is Return 
		BIC		LR,  #4 								//    Modify LR, use MSP 
		LDR		R3, [R1] 								//    Get return value, PensSV didn't use R3 
		LDR		R1, =Lin_TaskLoader 
		LDR		R2, =Lin_NextTask  
		STR		R1, [R2] 								//    Store the storage handle to Lin_NextTask 
		MOV		R12,  LR 								//    PendSV didn't use R12 
		BL 		__cpp(PendSV_Handler) 	//    Do CtxSw 
		MOV		LR,  R12 
		MRS		R0,  PSP 
		MSR		MSP, R0 								//    MSP Stack Shrink for CtxS 
		STR		R3, [R0] 								//    Return value stored in HWStkF 
		BX		LR 
	EXPORT	SVC_ProxyCaller		[WEAK] 
SVC_ProxyCaller 									// If SVC_ProxyCaller not present- 
		BX		LR 											// Return 
}
// Pending Service Handler. 
// Dedicated for Context Switching. 
// The 1.0.6 version framework not changed 
__asm void PendSV_Handler(void){ 
		// PendSV Handler (Context Switch Performer) 
		// Disable interrupts will prevent any context switches. 
	IMPORT	Lin_DebugCtxSwTimes 
		LDR		R1, =Lin_DebugCtxSwTimes 
		LDR		R0, [R1] 
		ADD		R0,  #1 
		STR		R0, [R1] 
		NOP 
	IMPORT	Lin_CurrTask 
	IMPORT	Lin_NextTask 
		LDR		R2, =Lin_CurrTask 
		LDR		R1, =Lin_NextTask 
		LDR		R0, [R2] 			// CurrTaskHandle in R0 
		LDR		R1, [R1] 			// NextTaskHandle in R1 
		STR		R1, [R2] 			// NextTaskHandle stored in _CurrTask 
		MRS		R2,  PSP 			// A1 Get   PSP1 from Processor 
		STMDB R2!,{R4-R11} 	// A2 Push  Context {R4-R11} to PSP1  
		STR		R2, [R0]			// A3 Store PSP1 via CurrTaskHandle->StkPtr 
		LDR		R2, [R1]			// B1 Get   PSP2 via NextTaskHandle->StkPtr 
		LDMIA R2!,{R4-R11} 	// B2 Pop   Context {R4-R11} from PSP2 
		MSR		PSP, R2 			// B3 Set   PSP2 to Processor 
		BX		LR 						// Pop HW Context {R0-R3,R12,LR,PC,PSR} 
}
// End of a section. 


// End of file. 
