// Lin Architecture header file verion 3.1.2 for lyrinka OS 
#ifndef __Lin_H__ 
#define __Lin_H__ 

#include <stm32f10x.h> 

/* Comments: 
	DataStructure of the Memory Carrier Block: 
		... 				<- High Address 
		Pld 		- 8 						: Payload Pointer (void *) 
		Cmd 		- 4 						: Command (u32) 
		Src 		- 0 <- MSG 			: Source (int) 
		Next 		- 4 						: Next in queue 
		... 				<- Low Address 

	DataStructure of the Stack: 
		... 				<- High Address 
		StkLim 	-60 						: Stack Bottom 
		Priority-56 						: Priority. 
		<many> 	-52 						: TimeSlice related registers. 
		<many> 	-48 						: Event related registers. 
		RBN 		-44 						: Pointer RBrN. 
		LBN 		-40 						: Pointer LBrN. 
		Next 		-36 						: Pointer Next. 
		Prev 		-32 						: Pointer Prev. 
		MsgTail -28 						: Tail fo Message Queue 
		MsgHead -24 						: Head of Message Queue 
		MsgQty 	-20 						: Messages waiting in Queue 
		Counter -16 						: Counter, Initialief as 0xFFFFFFFF 
		Arg1 		-12 						: Argument 1 
		Arg0 		- 8 						: Argument 0 
		Entrance- 4 						: Entrance Point 
		StkPtr  - 0	<- TASK 		: Stack Pointer 
		xPSR 		- 4							: Initialized as 0x01000000 (Thumb State) 
		PC 			- 8 						: ThreadExit Routine(for initialization) 
		LR 			-12 						: / 
		Reg12 	-16 						: / 
		Reg3 		-20 						: (reference of itself) 
		Reg2 		-24 						: (Loop counter value) 
		Reg1 		-28 						: (Arg1) 
		Reg0 		-32 <- _HWStkF 	: (Arg0) 
		Reg11 	-36 						: / 
		Reg10 	-40 						: / 
		Reg9 		-44 						: / 
		Reg8 		-48 						: / 
		Reg7 		-52 						: / 
		Reg6 		-56 						: / 
		Reg5 		-60 						: / 
		Reg4 		-64 <- _CtxS 		: / 
		... 										: Stack Region 
														: ... 
														: ... 
		... 										: Other Region 
								<- StkBtm 	: Start of the Stack 
		...					<- Low Address 

	Task Function Params: 
   void funcTask(u32 Arg0, u32 Arg1, u32 Counter, TASK Self); 
	 Arg0: user applied argument, default 0 
	 Arg1: user applied argument, default 0 
	 Counter: the loop counter, increaced after each ThreadExit routine 
	 Self: the reference of the Task itself 
	 The function should not return anything. 
*/ 

// Configuration
#define SVCn_LinSwitch 	0x00 
#define SVCn_LinTrigger	0x01 

#define Lin_MsgPoolSize	32 
#define Lin_MemStart 	(*((u32 *)0x08000000)) 
#define Lin_MemEnd 		(Lin_MemStart + 0x5000) 

// Macros 
#define NULL 			((void *)0) 

#define __critical_alloc() int __IE 
#define __critical_enter() int __IE = __get_PRIMASK(), __disable_irq() 
#define __critical_reenter() __IE = __get_PRIMASK(), __disable_irq() 
#define __critical_exit() __set_PRIMASK(__IE) 

// Types 
// Inter-Task Message Type - MSG 
typedef struct Lin_Msg{ 
	int Src; 
	u32 Cmd; 
	void * Pld; 
}Lin_Msg, MSG; 

// Inter-Task Message Carrier Block Type !!Internal 
typedef struct Lin_MsgBlk{ 
	struct Lin_MsgBlk * Next; 
	Lin_Msg Msg; 
}Lin_MsgBlk; 

// Event Control Block - For Operating System 
typedef struct Lin_ECB{ 
	int TimeBase_Mode; 
	u32 TimeBase_Stamp; 
	void * WkupRef; 	// On what Event did it wake up? 
	int EvListSize; 
	void * EvList[]; 
}Lin_ECB; 

// Task Control Block Type - TASK 
typedef struct Lin_TCB{ 
	u8 * SP; 
	void * PC; 
	int Arg0; 
	int Arg1; 
	u32 Cntr; 
	int MsgQty; // MsgN 
	Lin_MsgBlk * MsgHead; // MsgF 
	Lin_MsgBlk * MsgTail; // MsgL 

	struct Lin_TCB * Prev; 
	struct Lin_TCB * Next; 
	struct Lin_TCB * LBN; 
	struct Lin_TCB * RBN; 
	s8 	WkupSrc; 		// Source of Waking up: EventRef or GenericEvent or TimeBase? 
	u8 	WkupMeth; 	// Method of Waking up: Is it woke up from previously fired event? 
	s8 	GenEvFlag; 	// The flag of GenericEvent. A type I Event for waking the process up. 
	u8 	GenEvInfo; 	// 8-bit data for simple IPC using GE. 
	s16 TimeSliceCounter; 
	s16 TimeSliceReload; 
	int Priority; 
	Lin_ECB * ECB; 
}Lin_TCB, * TASK; 


// Functions 
extern	void 		Lin_Init			(void); 													// Initialize Lin Framework 

extern	void * 	Lin_MemAlloc	(u32 Size); 											// Allocate memory 
extern	void 		Lin_MemFree		(void * Mem); 										// Free memory 

extern	TASK 		Lin_New				(u32 StkSize, void * PC); 				// Create a new Task 
extern	void 		Lin_SetArgs		(TASK Task, int Arg0, int Arg1); 	// Set Task argumens 
extern	void 		Lin_SetPC			(TASK Task, void * PC); 					// Set Task entrance point 
extern	int 		Lin_Enter			(TASK Task); 											// Enter MainTask from main function(MSP) 
extern	void 		Lin_Switch		(TASK Task); 											// Switch to any other Task 
extern	void 		Lin_SwitchISR	(TASK Task); 											// Switch to any other Task from ISR 
extern	void 		Lin_Yield			(void); 													// Switch to MainTask 
extern	void 		Lin_YieldISR	(void); 													// Switch to MainTask from ISR 
extern	void 		Lin_Return		(int retval); 										// Return from Task to main function(MSP) 
extern	void 		Lin_Delete		(TASK Task); 											// Delete Task and free up all the memory 

extern	TASK 		Lin_GetCurrTask(void); 													// Get CurrentTask reference 
extern	TASK 		Lin_GetMainTask(void); 													// Get MainTask reference 

extern	int 		Lin_MsgPut		(TASK Task, MSG Msg); 						// Send Message to any Task 
extern	int 		Lin_MsgPutF		(TASK Task, MSG Msg); 						// Sent priority Message to any Task 
extern	int 		Lin_MsgSubmit	(MSG Msg); 												// Send Message to MainTask 
extern	int 		Lin_MsgSubmitF(MSG Msg); 												// Send priority Message to MainTask 
extern	u32 		Lin_MsgQty		(void); 													// Get CurrentTask Message Queue pending 
extern	MSG 		Lin_MsgGet		(TASK Task); 											// Get Message from ant Task 
extern	MSG 		Lin_MsgRecv		(void); 													// Get Message from CurrentTask 
extern	MSG 		Lin_MsgPrvw		(void); 													// Preview Message from CurrentTask 

#endif 

// End of file. 
