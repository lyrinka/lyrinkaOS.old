// Dummy Event System 
int Event_DebugEventQurtyCnt; 
int Event_DebugEvCycleStamp; 

void Ev_Init(void){ 
	Event_DebugEvCycleStamp = 0; 
	Event_DebugEventQurtyCnt = 0; 
}

int Ev_Query(void * EvRef){ 
	Event_DebugEventQurtyCnt++; 
	return 0; 
}

void Ev_Cycle(void){ 
	Event_DebugEvCycleStamp++; 
}

// End of file. 
