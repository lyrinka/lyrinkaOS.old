// // Linear Table for Symmetrical Scheduling Lists version 1 header file 
#ifndef __Lint_H__ 
#define __Lint_H__ 

TASK Lint_Init(TASK); 

int  Lint_nbrWaiting(void); 
int  Lint_nbrStandby(void); 

TASK PQ_Add(TASK); 
TASK PQ_Del(TASK); 
TASK PQ_Get(void); 
TASK PQ_Rot(TASK); 

TASK DL_Add(TASK); 
TASK DL_Del(TASK); 
TASK DL_Trav(TASK); 

#define Lint_IsNotWaiting(Task) (Task->Prev == NULL) // Standby or Dead 
#define Lint_IsDead(Task) (Task->LBN == NULL) // Dead 

#endif 

