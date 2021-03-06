// lyrinka OS version 1.0.2 header file 
#ifndef __OS_H__ 
#define __OS_H__ 

#ifdef __cplusplus 
extern "C" { 
#endif 

#include <Lin.h> 
#include <Sched.h> 
#include <Event.h> 

extern u32 TickCount; 

TASK OS_New(u32 StkSize, void * PC); 
void OS_ChgPri(TASK Task, int Priority); 
void OS_Del(TASK Task); 

void OS_GenEvent(TASK Task, u8 info); 
void OS_TBGperiod(int interval); 
void OS_TBGdelay(int time); 
void OS_TBGstop(void); 

#define OS_PreemptISR() Lin_YieldISR() 
#define OS_Preempt() Lin_Yield() 
void OS_Yield(void); 
void OS_Suspend(void); 

#define OS_Lock() Sched_Lock() 
#define OS_UnLock() Sched_UnLock() 
#define OS_ClrLock() Sched_ClrLock() 

#define OS_TxMsg(task, msg) Lin_MsgPut(task, msg) 
#define OS_RxCnt() Lin_MsgQty() 
#define OS_RxMsg() Lin_MsgRecv() 

#ifdef __cplusplus 
}
#endif 

#endif 
