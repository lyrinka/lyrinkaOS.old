// Symmetrical Scheduling Core version 0.1.1 
/* Release Notes: 

		<0.1.1 > 190203 Fully Commented. 
		<0.1.0 > 190203 Initial Release. 
*/
#include <Lin.h> 
#include "Lint.h" 
#include "Sched.h" 

u32 PrevSysTime; 	// Timestamp of last scheduling. For determining when to perform TimeSlice operations. 
TASK Running; 		// Currently Running Task. (Different from Lin_CurrTask for it changes to the scheduler thread itself when scheduling. ) 
int SpinLock; 		// SpinLock flag. Locked when > 0. 

void Sched_Init(TASK MainTask){ 	// Initialization of the scheduler and main task. 
	PrevSysTime = 0xFFFFFFFF; 
	Running = NULL; 
	SpinLock = 0; 
	Lint_Init(MainTask); 
}
int Sched_Reg(TASK Task){ 	// Register for a task. Puts it in the Standby List so you might need a GenericEvent to wake it up. 
	__critical_enter(); 
	if(!Lint_IsDead(Task)){ 
		__critical_exit(); 
		return -1; 
	}
	DL_Add(Task); 
	__critical_exit(); 
	return 0; 
}
int Sched_UnReg(TASK Task){ 	// Unregister for a task. Removes it from all lists. 
	__critical_enter(); 
	if(Lint_IsDead(Task)){ 
		__critical_exit(); 
		return -1; 
	}
	if(Lint_IsNotWaiting(Task)) DL_Del(Task); 
	else PQ_Del(Task); 
	__critical_exit(); 
	return 0; 
}
void Sched_Lock(void){ 	// Apply SpinLock. 
	__critical_enter(); 
	SpinLock++; 
	__critical_exit(); 
}
void Sched_UnLock(void){ 	// Release SpinLock. 
	__critical_enter(); 
	SpinLock--; 
	__critical_exit(); 
}
void Sched_ClrLock(void){ // Force Release SpinLock. 
//__critical_enter(); 
	SpinLock = 0; 
//__critical_exit(); 
}


int DoEventCheck(TASK Task, u32 SysTime, int (*EvQuery)(void * EvRef), int isPreChk); // Checking Events for a Task. 
int TimeSliceTick(TASK Task); // Updating and checking TimeSlices for a Task. 

TASK Sched_Do(u32 SysTime, int (*EvQuery)(void * EvRef), void (*EvCycle)(void), TASK (*GetSus)(void)){ // Pick Next Task 
	// SysTime is the current ms SystemTick Time. 
	// EvQuery is for polling events. Return 0 if not found and non-zero if found. 
	// EvCycle is for marking a mass-receiving cycle. See the Biomimetic Event System for details. 
	// GetSus return tasks who suspended themselves by requests. 
	TASK Task = DL_Trav(NULL); 
	while(Task != NULL){ 	// I. Traverse through Standby List. 
		TASK NextTask = DL_Trav(Task); 
		if(DoEventCheck(Task, SysTime, EvQuery, 0)){ 
			DL_Del(Task); // Move from Stdby to Waiting 
			PQ_Add(Task); 
		}
		Task = NextTask; 
	}
	for(Task = GetSus(); Task != NULL; Task = GetSus()){ // II. Those who suspended themselves. 
		if(Lint_IsNotWaiting(Task)) continue; 
		if(DoEventCheck(Task, SysTime, EvQuery, 1)) PQ_Rot(Task); // Previously happened event 
		else{ 
			PQ_Del(Task); // Does need waiting 
			DL_Add(Task); 
		}
	}
	EvCycle(); // Symmetrical Scheduling Done. 
	if((Running != NULL) && (Lint_IsDead(Running) == 0)){ 	// Previous Cycle CPU Not Idle and Running is stil Living 
		if(SysTime != PrevSysTime) 														// If SysTick Increaced 
			if(TimeSliceTick(Running)) 													// Apply Time Slice Cost and Check Time Balance 
				if(!Lint_IsNotWaiting(Running)) PQ_Rot(Running); 	// If Time is up and Still in Waiting List, Rotate. 
	}
	else SpinLock = 0; 															// If Previous Cycle CPU Idle or Running has been Killed, Release SpinLock. 
	PrevSysTime = SysTime; 
	if(SpinLock <= 0){ 			// If SpinLock inactive 
		SpinLock = 0; 
		Running = PQ_Get(); 	// Get Next 
	}
	return Running; 
}

// Internal Functions 
int DoEventCheck(TASK Task, u32 SysTime, int (*EvQuery)(void * EvRef), int isPreChk){ 	// Checking Events for a Task. 
	int EvActive = 0; 
	
	// I. Generic Event Flag Check 
	if(Task->GenEvFlag != 0){ 	
		Task->GenEvFlag = 0; 
		if(EvActive == 0){ 
			EvActive = 1; 
			Task->WkupSrc = Src_Gen; 
		}
	}
	
	// II. Time Base Generator Check 
	Lin_ECB * ECB = Task->ECB; 
	u32 Tstamp = ECB->TimeBase_Stamp; 
	int Tmode = ECB->TimeBase_Mode; // Turn off TBG when mode < 0 
	if(Tstamp <= SysTime && Tmode >= 0){ 
		if(Tmode == 0) ECB->TimeBase_Mode = -1; // One-shot when mode = 0 
		else ECB->TimeBase_Stamp = Tmode + SysTime; // Continous when mode > 0, interval determinated by the value of mode. 
		if(EvActive == 0){ // TBG still works even GEF activates, for TBG is an individual block, although the event production and identification codes are written together here. 
			EvActive = 1; 
			Task->WkupSrc = Src_TBG; 
		}
	}
	
	// III. Event List Check 
	if(EvActive == 0){ // No need traversing the list if previously something activated the process. 
		void ** EvList = ECB->EvList; 
		int EvListN = ECB->EvListSize; 
		for(int i = 0; i < EvListN; i++){ 	
			void * CurrEv = EvList[i]; 
			if(EvQuery(CurrEv)){ 
				EvActive = 1; 
				Task->WkupSrc = Src_Ev; 
				ECB->WkupRef = CurrEv; 
				break; 
			}
		}
	}
	
	// Wakeup Method and Sources 
	if(EvActive != 0){ 
		if(isPreChk) Task->WkupMeth = Meth_Prev; 
		else Task->WkupMeth = Meth_Wait; 
	}
	else{ 
		Task->WkupMeth = Meth_None; 
		Task->WkupSrc = Src_None; 
	}
	
	return EvActive; 
}

int TimeSliceTick(TASK Task){ 	// Update and check TimeSlice. 
	if(Task->TimeSliceReload <= 0) return 0; 
	if(--Task->TimeSliceCounter <= 0){ 
		Task->TimeSliceCounter = Task->TimeSliceReload; 
		return 1; 
	}
	return 0; 
}

// End of file. 
