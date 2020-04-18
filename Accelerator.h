/*******************************************************************
* SSD GCN Project
********************************************************************/

#ifndef __ACCELERATOR_H__
#define __ACCELERATOR_H__

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include "DRAMInterface.h"
#include "BufferInterface.h"
#include "Common.h"

using namespace std;

struct AccFlag
{
	bool a_col_req;
	bool a_row_req;
	bool x_val_req; // dummy
	bool x_col_req;
	bool x_row_req;
	bool weight_req;
	bool weight_uncompleted;
	bool mac_1;
	bool mac_2; // 사실상 !mac_1
};

struct EndFlag
{
	bool a_col_end;
	bool a_row_end;
	bool x_val_end; // dummy
	bool x_col_end;
	bool x_row_end;
};

struct Coordinate
{
	uint64_t row;
	uint64_t col;
	float val;
	uint64_t weight;
	uint64_t present_v_fold;
	uint64_t remain_mac_col;
	bool first_get;
	bool fold_start;
	bool macisready;
	bool maciszero;
	bool isend;
};

struct MACAuxFlag
{
	bool first_get;
	bool fold_start;
	bool macisready;
	bool maciszero;
};

struct TempRegister
{
	bool check;
	uint64_t w_addr;
};

class Accelerator {
public :
	Accelerator(uint64_t accdimension, DRAMInterface *dram_, BufferInterface *buffer_);
	~Accelerator();
	bool Run();
private:
	// Ini에서 고정되는 값들
	uint64_t num_of_pe; // 사용 안함 - 단순 기록용?
	uint64_t w_fold;
	uint64_t v_fold;
	uint64_t v_fold_last;
	uint64_t limit_ax_req;
	DRAMInterface *dram;
	BufferInterface *buffer;
	
	// 공통 변수
	void Reset();
	uint64_t present_w_fold;
	bool macover; // Run() 플래그
	bool programover; // Run() 플래그

	// RequestControllerRun()
	void RequestControllerRun();
	void Request(Type iswhat);
	void RequestWeight(uint64_t address);
	uint64_t a_col_addr; // Request()용
	uint64_t a_row_addr; // Request()용
	uint64_t x_val_addr; // Request()용
	uint64_t x_col_addr; // Request()용
	uint64_t x_row_addr; // Request()용
	AXBuffer cheat; // Request()용, 리퀘스트가 더 가능한지 확인하는 변수
	uint64_t remain_col_num; //현재 row의 남은 col data 갯수
	uint64_t present_col; // weight address 구하는 용도
	uint64_t present_row; // 사실상 사용 안함
	EndFlag endflag; // data 전부 읽었는지 확인용
	TempRegister temp; // weight data 처리 용도
	AccFlag flag;

	// MACControllerRun()
	void MACControllerRun();
	uint64_t *remain_mac_col;
	uint64_t present_v_fold; // v_fold 계산
	uint64_t present_mac_row;
	Coordinate *present; //MACController 처리용
	MACAuxFlag macflag;
	uint64_t parallel;
	uint64_t check_over;
	uint64_t check_row;
	uint64_t row_num;
};

#endif
