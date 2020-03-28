/*******************************************************************
* SSD GCN Project
********************************************************************/

#ifndef __COMMON_H__
#define __COMMON_H__

#include <queue>
#include <vector>
#include <cassert>

using namespace std;

extern uint64_t MAX_READ_BYTE; 
extern uint64_t MAX_READ_INT; 
extern uint64_t UNIT_INT_BYTE; 

extern uint64_t A_COL_START;  
extern uint64_t A_ROW_START;  
extern uint64_t X_VAL_START;  
extern uint64_t X_COL_START;  
extern uint64_t X_ROW_START;  
extern uint64_t WEIGHT_START; 
extern uint64_t OUTPUT_START;

enum Type {A_COL, A_ROW, X_VAL, X_COL, X_ROW, WEIGHT, OUTPUT};

//{Row, Col} or {weight_h, weight_w}
struct Tuple {
	uint64_t tuple[2];
};

struct AXBuffer {
	uint64_t size;
	uint64_t remain_space;
	uint64_t valindex;
	uint64_t colindex;
	uint64_t rowindex;
};

struct WeightBuffer {
	uint64_t remain_space;
	vector<Tuple> active;
	vector<Tuple> expire;
};

struct OutputBuffer {
	uint64_t remain_space;
	queue<uint64_t> address; 
};

#endif


//퍼포먼스 카운터