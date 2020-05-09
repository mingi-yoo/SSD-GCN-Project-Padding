/*******************************************************************
* SSD GCN Project
********************************************************************/

#ifndef __BUFFERINTERFACE_H__
#define __BUFFERINTERFACE_H__

#include <iostream>
#include <string>
#include "DataReader.h"
#include "Common.h"

using namespace std;

struct BufFlag
{
	bool a_col;
	bool a_row;
	bool x_val;
	bool x_col;
	bool x_row;
};

struct BufIndex
{
	uint64_t valindex;
	uint64_t colindex;
	uint64_t rowindex;
};

class BufferInterface {
public:
	DataReader *data; // graph data // Acc에서 cheat용으로 사용
	Tuple weightsize;
	uint64_t axbuffersize;
	uint64_t weightbuffersize;
	uint64_t present_ax_req; //현재 accelerator가 리퀘스트 한 정도
	uint64_t mac1_count;
	uint64_t mac2_count;
	uint64_t num_of_xrow;
	uint64_t num_of_arow;
	bool isA;
	BufferInterface(uint64_t axbuffersize, 
					uint64_t weightbuffersize, 
					uint64_t outputbuffersize,
					DataReader *data_);
	~BufferInterface();
	void FillBuffer(uint64_t address, Type iswhat);
	bool IsFilled(Type iswhat);
	bool AuxIsFilled(Type iswhat);
	bool AuxIsFulled();
	bool XEnd(); //MAC2로 넘어갈 준비가 되었는가
	bool AEnd(); //모든 MAC이 끝났는가
	bool XRowEnd();
	bool XColEnd();
	bool XValEnd();
	bool ARowEnd();
	bool AColEnd();
	bool isReady(uint64_t address);
	void Reset(); //다시 채우는 함수
	uint64_t HowMuchAXFilled();
	uint64_t PopData(Type iswhat);
	float PopValData();
	uint64_t ReadMACData(Type iswhat);
	float ReadValMACData();
	//weight용
	bool canRequest(bool istwo);
	void Request(uint64_t address);
	bool Requested(uint64_t address); // RequestController 에서만 사용되어야 함
	bool isExist(uint64_t address); //weight 값 존재? (Request controller에서만 사용되어야 함)
	bool Expire(uint64_t address);

private:
	//buffer
	AXBuffer axbuffer; //처음 DRAM에서 읽어올 때 저장되는 버퍼
	AXBuffer aux_axbuffer; //첫번째 PopData후 데이터가 옮겨지는 버퍼
	WeightBuffer weightbuffer;
	uint64_t present_w_req;
	OutputBuffer outputbuffer; //dummy

	BufFlag flag;
	BufFlag aux_flag;
	BufIndex present; //첫 번째 버퍼의 위치를 가리키는 인덱스
	BufIndex aux_present; //두 번째 버퍼의 위치를 가리키는 인덱스
	
};

#endif
