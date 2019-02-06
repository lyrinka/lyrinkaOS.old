// lyrinka OS version 0.1.0 
/* Release Notes 

		<0.1.1 > 190206 Added support for dynamic priorities. 
		<0.1.0 > 190203 Initial Release. This is a library for simplified programming. 
*/ 
#include <OS.h> 
#include <Lint.h> 

TASK OS_New(u32 StkSize, void * PC){ 
	TASK Task = Lin_New(StkSize, PC); 
	if(Task == NULL) return NULL; 
	Task->WkupSrc = Src_None; 
	Task->WkupMeth = Meth_None; 
	Task->GenEvFlag = 0; 
	Task->GenEvInfo = 0; 
	Task->TimeSliceCounter = 10; 
	Task->TimeSliceReload = 10; 
	Task->Priority = 0; 
	Lin_ECB * ECB = Task->ECB; 
	ECB->TimeBase_Mode = -1; 
	ECB->TimeBase_Stamp = 0; 
	ECB->WkupRef = NULL; 
	ECB->EvListSize = 0; 
	Sched_Reg(Task); 
	return Task; 
}

void OS_ChgPri(TASK Task, int Priority){ 
	if(Task == NULL) Task = Lin_GetCurrTask(); 
	if(Lint_IsDead(Task)) return; 
	__critical_enter(); 
	if(Lint_IsNotWaiting(Task)) Task->Priority = Priority; 
	else{ 
		PQ_Del(Task); 
		Task->Priority = Priority; 
		PQ_Add(Task); 
	}
	__critical_exit(); 
}

void OS_Del(TASK Task){ 
	int self = 0; 
	if(Task == NULL){ 
		self = 1; 
		Task = Lin_GetCurrTask(); 
	}
	__critical_enter(); 
	Sched_UnReg(Task); 
	Lin_Delete(Task); 
	__critical_exit(); 
	if(self != 0) OS_Suspend(); 
}

void OS_GenEvent(TASK Task, u8 info){ 
	__critical_enter(); 
	Task->GenEvInfo = info; 
	Task->GenEvFlag = 1; 
	__critical_exit(); 
}

void OS_TBGperiod(int interval){ 
	__critical_enter(); 
	Lin_ECB * ECB = Lin_GetCurrTask()->ECB; 
	ECB->TimeBase_Mode = interval; 
	ECB->TimeBase_Stamp = TickCount + interval; 
	__critical_exit();  
}

void OS_TBGdelay(int time){ 
	__critical_enter(); 
	Lin_ECB * ECB = Lin_GetCurrTask()->ECB; 
	ECB->TimeBase_Mode = 0; 
	ECB->TimeBase_Stamp = TickCount + time; 
	__critical_exit(); 
}

void OS_TBGstop(void){ 
	__critical_enter(); 
	Lin_ECB * ECB = Lin_GetCurrTask()->ECB; 
	ECB->TimeBase_Mode = -1; 
	ECB->TimeBase_Stamp = 0; 
	__critical_exit(); 
}

void OS_Suspend(void){ 
	MSG Msg; 
	Msg.Cmd = 0x0; 
	Msg.Src = 0x0; 
	Msg.Pld = Lin_GetCurrTask(); 
	Lin_MsgSubmit(Msg); 
	Lin_Yield(); 
}

// End of file. 
