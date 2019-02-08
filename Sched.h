// Symmetrical Scheduling Core version 0.2.0 header file 
#ifndef __Sched_H__ 
#define __Sched_H__ 

void Sched_Init(TASK MainTask); 

int  Sched_Reg(TASK Task); 
int  Sched_UnReg(TASK Task); 

void Sched_Lock(void); 
void Sched_UnLock(void); 
void Sched_ClrLock(void); 

TASK Sched_Do(u32 SysTime, int (*EvQuery)(void * EvRef), void (*EvCycle)(void), int (*GetSuspended)(TASK *)); 

#define Meth_None 0 // Standby. 
#define Meth_Wait 1 // Woke up from standby list. 
#define Meth_Prev 2 // Woke up from previously existed event, not experiencing suspention. 

#define Src_Ev    1 // Event from Event References in List. 
#define Src_None  0 // Has no Event. 
#define Src_Gen  -1 // Event from Generic Event Flag. 
#define Src_TBG  -2 // Event from Time Base Generator. 

#endif 
