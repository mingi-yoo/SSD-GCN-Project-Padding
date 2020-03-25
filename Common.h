/*******************************************************************
* SSD GCN Project
********************************************************************/

#ifndef __COMMON_H__
#define __COMMON_H__

#include <queue>
#include <vector>
#include <cassert>

using namespace std;

#define MAX_READ_BYTE 64
#define MAX_READ_INT 8
#define UNIT_INT_BYTE 8

#define A_COL_START  20000000000000
#define A_ROW_START  21000000000000
#define X_VAL_START  10000000000000
#define X_COL_START  11000000000000
#define X_ROW_START  12000000000000
#define WEIGHT_START 0
#define OUTPUT_START 30000000000000
#define OUTPUT2_START 40000000000000

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