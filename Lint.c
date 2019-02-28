// Linear Table for Symmetrical Scheduling Lists version 2.0.0 
/* Release Notes: 

	<2.0.0 > 190223 Fixed critical bug where Priority Queue doesn't working properly in multi-priority scheduling. 
	<1     >        Initial Release. 
*/

#include <Lin.h> 								// We only use the TASK data type and critical region codes. 
#include "Lint.h" 

TASK 	MainTask; 								// The main task which acts as a dummy block to produce cyclic topology. 
int 	Lint_WaitCount; 					// Nbr elements in the waiting list 
int 	Lint_StdbyCount; 					// Nbr elements in the standby list 
int 	Lint_DebugOpTimes; 				// Debug: Operation Times on Chained Lists 

TASK Lint_Init(TASK Task){ 			// Initializes the main task as a dummy block. 
	__critical_enter(); 
	Lint_WaitCount = 0; 
	Lint_StdbyCount = 0; 
	Lint_DebugOpTimes = 0; 
	
	Task->Prev = Task; 	// Create cyclics connection to itself 
	Task->Next = Task; 
	Task->LBN = Task; 
	Task->RBN = Task; 
	MainTask = Task; 
	__critical_exit(); 
	return Task; 
}

int Lint_nbrWaiting(void){ 	// Count elements waiting 
	return Lint_WaitCount; 
}

int Lint_nbrStandby(void){ 	// Count elements standby (suspended) 
	return Lint_StdbyCount; 
}

__forceinline void UpdateRoot_A(TASK Root, TASK New){ 
	Root->Next = New; 
	if(Root == MainTask) return; 
	TASK Temp = Root->RBN; 
	while(Temp != Root){ 
		Temp->Next = New; 
		Temp = Temp->RBN; 
	}
}
__forceinline void UpdateRoot_B(TASK Root, TASK New){ 
	Root->Prev = New; 
	if(Root == MainTask) return; 
	TASK Temp = Root->RBN; 
	while(Temp != Root){ 
		Temp->Prev = New; 
		Temp = Temp->RBN; 
	}
}

TASK PQ_Add(TASK N){ 	// Add one task to Priority Queue (Waiting List). 
	__critical_enter(); 
	Lint_WaitCount++; 
	Lint_DebugOpTimes++; 
	TASK MainBlk = MainTask; 
	int p = N->Priority; 
	TASK Task = MainBlk->Next; 
	while(Task != MainBlk){ 	// Traverse for priority slot. 
		if(Task->Priority >= p) break; 
		Task = Task->Next; 
	}
	if(Task->Priority != p || Task == MainBlk){ // Create a new slot. 
		TASK B = Task; 
		TASK A = Task->Prev; 
		UpdateRoot_A(A, N); 
		UpdateRoot_B(B, N); 
//	A->Next = N; 
//	B->Prev = N; 
		N->Prev = A; 
		N->Next = B; 
		N->LBN = N; 
		N->RBN = N; 
	}
	else{ 	// Slot already exists, connect to its tail. 
		TASK b = Task; 
		TASK a = b->LBN; 
		a->RBN = N; 
		b->LBN = N; 
		N->LBN = a; 
		N->RBN = b; 
		N->Prev = b->Prev;  
		N->Next = b->Next; 
	}
	__critical_exit(); 
	return N; 
}

TASK PQ_Del(TASK P){ 	// Remove one task from Priority Queue (Waiting List). 
	__critical_enter(); 
	Lint_WaitCount--; 
	Lint_DebugOpTimes++; 
	if(P->RBN != P){ 	// This slot is not stand-alone. 
		TASK pRoot = P->Prev->Next; 
		if(pRoot == P){ // This is the root of the slot. Rotate it first. 
//		TASK A = P->Prev; 
//		TASK B = P->Next; 
			pRoot = P->RBN; 
			UpdateRoot_A(P->Prev, pRoot); 
			UpdateRoot_B(P->Next, pRoot); 
//		A->Next = pRoot; 
//		B->Prev = pRoot; 
		}		
		TASK a = P->LBN; // Disconnect one non-root element of the slot. 
		TASK b = P->RBN; 
		a->RBN = b; 
		b->LBN = a; 
	}
	else{ 	// This slot is stand-alone. Directly disconnect. 
		TASK A = P->Prev; 
		TASK B = P->Next; 
		UpdateRoot_A(A, B); 
		UpdateRoot_B(B, A); 
//	A->Next = B; 
//	B->Prev = A; 
	}
	P->LBN = NULL; 	// Mark it dead. 
	P->RBN = NULL; 
	P->Prev = NULL; 
	P->Next = NULL; 
	__critical_exit(); 
	return P; 
}

TASK PQ_Get(void){ 	// Retrieve the top priority element. 
	__critical_enter(); 
	TASK MainBlk = MainTask; 
	TASK Task = MainBlk->Next; 
	if(Task == MainBlk) Task = NULL; 
	__critical_exit(); 
	return Task; 
}

TASK PQ_Rot(TASK Task){  // Put this task the last one in its priority slot. 
	__critical_enter(); 
	if(Task->Prev->Next->LBN != Task){ 
		Lint_DebugOpTimes++; 
//	TASK A = Task->Prev; 
//	TASK B = Task->Next; 
		TASK newHead = Task->RBN; 
		UpdateRoot_A(Task->Prev, newHead); 
		UpdateRoot_B(Task->Next, newHead); 
//	A->Next = newHead; 
//	B->Prev = newHead; 
	}
	__critical_exit(); 
	return Task; 
}


TASK DL_Add(TASK N){ 	// Add one task to the D-List (Standby List). 
	__critical_enter(); 
	Lint_StdbyCount++; 
	Lint_DebugOpTimes++; 
	TASK B = MainTask; 
	TASK A = B->LBN; 
	A->RBN = N; 
	B->LBN = N; 
	N->LBN = A; 
	N->RBN = B; 
	N->Prev = NULL; 
	N->Next = NULL; 
	__critical_exit(); 
	return N; 
}

TASK DL_Del(TASK P){ 	// Remove one task from the D-List (Standby List). 
	__critical_enter(); 
	Lint_StdbyCount--; 
	Lint_DebugOpTimes++; 
	TASK A = P->LBN; 
	TASK B = P->RBN; 
	A->RBN = B; 
	B->LBN = A; 
	P->LBN = NULL; 
	P->RBN = NULL; 
	__critical_exit(); 
	return P; 
}

TASK DL_Trav(TASK Task){ 	// Traverse in the D-List (Standby List). Incoming NULL returns the first element and outcoming NULL mentions the end. 
	TASK MainBlk = MainTask; 
	if(Task == NULL) Task = MainBlk; 
	Task = Task->RBN; 
	if(Task == MainBlk) Task = NULL; 
	return Task; 
}

// End of file. 
