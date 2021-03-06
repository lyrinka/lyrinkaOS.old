// lyrinka OS startup code version 0.2.1 
// Contains main function, scheduler thread and system timer functions 
// This piece of code is to be executed, not referenced by external code. 
/* Release Notes: 

			<0.3.0 > 190301 Minor changes adapting new Lin library. 
			<0.2.1 > 190228 Added low power option. 
			<0.2.0 > 190208 Added support for another type of suspend requests. 
			<0.1.1 > 190206 Refined error procedures on empty waiting lists. 
			<0.1.0 > 190204 Initial Release. 
*/
#include <stm32f10x.h> 
#include <OS.h> 

u32 TickCount; 

// Internal Functions 
int GetSus(TASK * Task){ 
	if(Lin_MsgQty() == 0){ 
		*Task = NULL; 
		return 0; 
	}
	MSG Msg = Lin_MsgRecv(); 
	*Task = Msg.Pld; 
	return (Msg.Cmd != 0); 
}

// Handlers & System Code 
void SysTick_Init(u32 Time){ 
	__critical_enter(); 
	TickCount = 0; 
	SysTick->CTRL = 0x0; 
	SysTick->LOAD = Time - 1; 
	SysTick->VAL = 0; 
	SCB->SHP[12+SysTick_IRQn] = 0x0; // Highest priority 
	SysTick->CTRL = 0x3; 
	__critical_exit(); 
}

void SysTick_Handler(void){ 
	TickCount++; 
	Lin_YieldISR(); 
	return; 
}

// Main Function 
extern void OS_Scheduler(TASK Self); 
int main(void){ 
	Lin_Init(); 
	TASK OS = Lin_New(1024, OS_Scheduler); 
	Lin_Enter(OS); 
	Lin_Delete(OS); 
	__disable_irq(); 	
	while(1); 
}


// System Idle Processes 
void SIP(void){ 
	static long long a = -1; 
	a++; 
#ifdef LPW 
	__WFI(); 
#endif 
}

// Scheduler Process (main task) 
extern void mainTask(TASK Self); 
void OS_Scheduler(TASK Self){ 
	Sched_Init(Self); 
	Ev_Init(); 
	TASK Task; 
	
	Task = OS_New(2048, mainTask); 
	OS_GenEvent(Task, 0); 
	
	Task = OS_New(512, SIP); 
	Task->Priority = 0x7FFFFFFF; 
	OS_GenEvent(Task, 0); 
	
	SysTick_Init(9000); 
	for(;;){ 
		Task = Sched_Do(TickCount, Ev_Query, Ev_Cycle, GetSus); 
		if(Task == NULL){ 
			__critical_enter(); 
			__BKPT(0xE8); 
			__nop(); 
		}
		Lin_Switch(Task); 
	}
}

// End of file. 
