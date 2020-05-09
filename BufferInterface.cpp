/*******************************************************************
* SSD GCN Project
********************************************************************/

#include "BufferInterface.h"

using namespace std;

BufferInterface::BufferInterface(uint64_t axbuffersize, 
								 uint64_t weightbuffersize, 
								 uint64_t outputbuffersize,
								 DataReader *data_)
{
	axbuffer.size = axbuffersize;
	axbuffer.remain_space = axbuffersize;
	axbuffer.valindex = -1;
	axbuffer.colindex = -1;
	axbuffer.rowindex = -1;

	aux_axbuffer.size = axbuffersize;
	aux_axbuffer.remain_space = axbuffersize;
	aux_axbuffer.valindex = 0;
	aux_axbuffer.colindex = 0;
	aux_axbuffer.rowindex = 0;

	weightbuffer.remain_space = weightbuffersize; // dummy
	outputbuffer.remain_space = outputbuffersize; // dummy
	isA = false;
	data = data_;

	weightsize.tuple[0] = data_->weight_h;
	weightsize.tuple[1] = data_->weight_w;

	this->axbuffersize = axbuffersize;
	this->weightbuffersize = weightbuffersize;

	mac1_count = data_->ifrowindex.size() - 1;
	mac2_count = data_->adjrowindex.size() - 1;

	present_ax_req = 0;
	present_w_req = 0;

	flag = {false, false, false, false, false};
	aux_flag = {false, false, false, false, false};
	present = {0, 0, 0};
	aux_present = {0, 0, 0};
}

BufferInterface::~BufferInterface() {}

void BufferInterface::FillBuffer(uint64_t address, Type iswhat)
{
	switch (iswhat)
	{
		case A_COL:
			if (data->adjcolindex.size() - axbuffer.colindex -1 < MAX_READ_INT)
				axbuffer.colindex = data->adjcolindex.size() - 1;
			else
				axbuffer.colindex += MAX_READ_INT;
			axbuffer.remain_space -= MAX_READ_BYTE;
			flag.a_col = true;
			break;
		case A_ROW:
			if (data->adjrowindex.size() - axbuffer.rowindex -1 < MAX_READ_INT)
				axbuffer.rowindex = data->adjrowindex.size() - 1;
			else
				axbuffer.rowindex += MAX_READ_INT;
			axbuffer.remain_space -= MAX_READ_BYTE;
			flag.a_row = true;
			break;
		case X_VAL:
			if (data->ifvalue.size() - axbuffer.valindex -1 < MAX_READ_INT)
				axbuffer.valindex = data->ifvalue.size() - 1;
			else
				axbuffer.valindex += MAX_READ_INT;
			axbuffer.remain_space -= MAX_READ_BYTE;
			flag.x_val = true;
			break;
		case X_COL:
			if (data->ifcolindex.size() - axbuffer.colindex -1 < MAX_READ_INT)
				axbuffer.colindex = data->ifcolindex.size() - 1;
			else
				axbuffer.colindex += MAX_READ_INT;
			axbuffer.remain_space -= MAX_READ_BYTE;
			flag.x_col = true;
			break;
		case X_ROW:
			if (data->ifrowindex.size() - axbuffer.rowindex -1 < MAX_READ_INT)
				axbuffer.rowindex = data->ifrowindex.size() - 1;
			else
				axbuffer.rowindex += MAX_READ_INT;
			axbuffer.remain_space -= MAX_READ_BYTE;
			flag.x_row = true;
			break;
		case WEIGHT:
			vector<WB_Data>::iterator iter;
			bool isWork = false; // 정상적으로 작동했는지 확인용

			for(iter = weightbuffer.request.begin(); iter != weightbuffer.request.end(); iter++)
			{
				if(iter->address == address)
				{
					WB_Data t = {address, iter->req};
					weightbuffer.active.push_back(t);
					weightbuffer.request.erase(iter);
					isWork = true;
					break;
				}
			}
			assert(isWork);
			break;
	}
}

bool BufferInterface::IsFilled(Type iswhat)
{
	bool ret;

	switch (iswhat)
	{
		case A_COL:
			ret = flag.a_col;
			break;
		case A_ROW:
			ret = flag.a_row;
			break;
		case X_VAL:
			ret = flag.x_val;
			break;
		case X_COL:
			ret = flag.x_col;
			break;
		case X_ROW:
			ret = flag.x_row;
			break;
		default:
			ret = false;
	}

	return ret;
}

bool BufferInterface::AuxIsFilled(Type iswhat)
{
	bool ret;

	switch (iswhat)
	{
		case A_COL:
			ret = aux_flag.a_col;
			break;
		case A_ROW:
			ret = aux_flag.a_row;
			break;
		case X_VAL:
			ret = aux_flag.x_val;
			break;
		case X_COL:
			ret = aux_flag.x_col;
			break;
		case X_ROW:
			ret = aux_flag.x_row;
			break;
		case WEIGHT:
			if (aux_axbuffer.w_addr.size() != 0)
				ret = true;
			else
				ret = false;
			break;
	}

	return ret;
}

bool BufferInterface::AuxIsFulled()
{
	if (!isA)
		return (aux_axbuffer.remain_space < 5 * UNIT_INT_BYTE);
	else
		return (aux_axbuffer.remain_space < 4 * UNIT_INT_BYTE);
}

void BufferInterface::Reset()
{
	axbuffer.size = axbuffersize;
	axbuffer.remain_space = axbuffersize;
	axbuffer.valindex = -1;
	axbuffer.colindex = -1;
	axbuffer.rowindex = -1;

	aux_axbuffer.size = axbuffersize;
	aux_axbuffer.remain_space = axbuffersize;
	aux_axbuffer.valindex = 0;
	aux_axbuffer.colindex = 0;
	aux_axbuffer.rowindex = 0;

	present_ax_req = 0;
	
	weightbuffer.remain_space = weightbuffersize;
	weightbuffer.active.clear();
	weightbuffer.expire.clear();
	weightbuffer.request.clear();
	present_w_req = 0;

	flag = {false, false, false, false, false};
	aux_flag = {false, false, false, false, false};
	present = {0, 0, 0};
	aux_present = {0, 0, 0};

}

uint64_t BufferInterface::PopData(Type iswhat)
{
	uint64_t ret;

	switch (iswhat)
	{
		case A_COL:
			aux_axbuffer.colindex = present.colindex;
			ret = data->adjcolindex[present.colindex];
			cout << "A_COL Popped (value : " <<dec<< ret << ")" << endl;
			axbuffer.remain_space += UNIT_INT_BYTE;
			aux_axbuffer.remain_space -= UNIT_INT_BYTE;
			present.colindex++;
			aux_flag.a_col = true;
			if (present.colindex > axbuffer.colindex)
				flag.a_col = false;
			break;
		case A_ROW:
			if (present.rowindex == 0)
				present.rowindex++;
			aux_axbuffer.rowindex = present.rowindex;
			ret = data->adjrowindex[present.rowindex] - data->adjrowindex[present.rowindex-1];
			cout << "A_ROW Popped (value : " <<dec<< ret << ")" << endl;
			axbuffer.remain_space += UNIT_INT_BYTE;
			aux_axbuffer.remain_space -= UNIT_INT_BYTE;
			present.rowindex++;
			aux_flag.a_row = true;
			if (present.rowindex > axbuffer.rowindex)
				flag.a_row = false;
			break;
		case X_COL:
			aux_axbuffer.colindex = present.colindex;
			ret = data->ifcolindex[present.colindex];
			cout << "X_COL Popped (value : " <<dec<<ret << ")" << endl;
			axbuffer.remain_space += UNIT_INT_BYTE;
			aux_axbuffer.remain_space -= UNIT_INT_BYTE;
			present.colindex++;
			aux_flag.x_col = true;
			if (present.colindex > axbuffer.colindex)
				flag.x_col = false;
			break;
		case X_ROW:
			if (present.rowindex == 0)
				present.rowindex++;
			aux_axbuffer.rowindex = present.rowindex;
			ret = data->ifrowindex[present.rowindex] - data->ifrowindex[present.rowindex-1];
			cout << "X_ROW Popped (value : " <<dec<< ret << ")" << endl;
			axbuffer.remain_space += UNIT_INT_BYTE;
			aux_axbuffer.remain_space -= UNIT_INT_BYTE;
			present.rowindex++;
			aux_flag.x_row = true;
			if (present.rowindex > axbuffer.rowindex)
				flag.x_row = false;
			break;
	}
	
	return ret;
}
float BufferInterface::PopValData()
{
	float ret;

	aux_axbuffer.valindex = present.valindex;
	ret = data->ifvalue[present.valindex];
	axbuffer.remain_space += UNIT_INT_BYTE;
	aux_axbuffer.remain_space -= UNIT_INT_BYTE;
	present.valindex++;
	aux_flag.x_val = true;
	if (present.valindex > axbuffer.valindex)
		flag.x_val = false;

	return ret;
}
uint64_t BufferInterface::ReadMACData(Type iswhat)
{
	uint64_t ret;

	switch (iswhat) 
	{
		case A_COL:
			ret = data->adjcolindex[aux_present.colindex];
			cout << "Aux A_COL Popped (value : "<<dec<< ret << ")" << endl;
			aux_present.colindex++;
			aux_axbuffer.remain_space += UNIT_INT_BYTE;
			if (aux_present.colindex > aux_axbuffer.colindex)
				aux_flag.a_col = false;
			break;
		case A_ROW:
			if (aux_present.rowindex == 0)
				aux_present.rowindex++;
			ret = data->adjrowindex[aux_present.rowindex] - data->adjrowindex[aux_present.rowindex-1];
			cout << "Aux A_ROW Popped (value : "<<dec<< ret << ")" << endl;
			aux_present.rowindex++;
			aux_axbuffer.remain_space += UNIT_INT_BYTE;
			if (aux_present.rowindex > aux_axbuffer.rowindex)
				aux_flag.a_row = false;
			break;			
		case X_COL:
			ret = data->ifcolindex[aux_present.colindex];
			cout << "Aux X_COL Popped (value : "<<dec<< ret << ")" << endl;
			aux_present.colindex++;
			aux_axbuffer.remain_space += UNIT_INT_BYTE;
			if (aux_present.colindex > aux_axbuffer.colindex)
				aux_flag.x_col = false;
			break;
		case X_ROW:
			if (aux_present.rowindex == 0)
				aux_present.rowindex++;
			ret = data->ifrowindex[aux_present.rowindex] - data->ifrowindex[aux_present.rowindex-1];
			cout << "Aux X_ROW Popped (value : "<<dec<< ret << ")" << endl;
			aux_present.rowindex++;
			aux_axbuffer.remain_space += UNIT_INT_BYTE;
			if (aux_present.rowindex > aux_axbuffer.rowindex)
				aux_flag.x_row = false;
			break;
	}

	return ret;
}

uint64_t BufferInterface::HowMuchAXFilled()
{
	return (axbuffer.size - axbuffer.remain_space);
}

float BufferInterface::ReadValMACData()
{
	float ret;

	ret = data->ifvalue[aux_present.valindex];
	aux_present.valindex++;
	aux_axbuffer.remain_space += UNIT_INT_BYTE;
	if (aux_present.valindex > aux_axbuffer.valindex)
		aux_flag.x_val = false;
	
	return ret;
}

bool BufferInterface::XEnd()
{
	// TODO
	if (!isA && 
		present.rowindex >= data->ifrowindex.size() && 
		present.colindex >= data->ifcolindex.size() &&
		present.valindex >= data->ifvalue.size() &&
		aux_present.rowindex >= data->ifrowindex.size() &&
		aux_present.colindex >= data->ifcolindex.size() &&
		aux_present.valindex >= data->ifvalue.size())
	{
		return true;
	}
	else
		return false; // Dummy
}

bool BufferInterface::AEnd()
{
	// TODO
	if (isA && 
		present.rowindex >= data->adjrowindex.size() && 
		present.colindex >= data->adjcolindex.size() &&
		aux_present.rowindex >= data->adjrowindex.size() &&
		aux_present.colindex >= data->adjcolindex.size())
		return true;
	else
		return false; // Dummy
}

bool BufferInterface::XRowEnd()
{
	//cout<<present.rowindex<<" .... "<<data->ifrowindex.size()<<endl;
	if (!isA && present.rowindex >= data->ifrowindex.size())
		return true;
	else
		return false;
}

bool BufferInterface::XColEnd()
{
	if (!isA && present.colindex >= data->ifcolindex.size())
		return true;
	else
		return false;
}

bool BufferInterface::XValEnd()
{
	if (!isA && present.valindex >= data->ifvalue.size())
		return true;
	else
		return false;
}

bool BufferInterface::ARowEnd()
{
	if (isA && present.rowindex >= data->adjrowindex.size())
		return true;
	else
	{
		return false;
	}
}

bool BufferInterface::AColEnd()
{
	if (isA && present.colindex >= data->adjcolindex.size())
		return true;
	else
		return false;
}

/* weight buffer 를 위한 함수들
*
* canRequest() : request를 보낼 수 있는 상태인지 확인
* Request() : request 보낸 데이터들 목록 관리
* Requested() : 아직 버퍼에는 없지만 이미 request요청이 간 데이터인지 확인 (중복 request 방지)
* IsExist() : weight data를 request하기 전에 해당 data가 버퍼내부에 있는지 확인
* expire() : active내의 데이터 중 특정 address 의 data만 expire시키고 싶을 때 쓸 수 있는 함수
*
*/
bool BufferInterface::isReady(uint64_t address)
{
	vector<WB_Data>::iterator iter;
	for(iter = weightbuffer.active.begin(); iter != weightbuffer.active.end(); iter++)
	{
	    if(iter->address == address)
	        return true;
	}
	return false;
}
bool BufferInterface::canRequest(bool istwo)
{
	if (!istwo)
	{
		if(weightbuffersize - present_w_req >= MAX_READ_BYTE) // 버퍼가 아직 다 안채워진 경우 -> 가능
		{
			return true;
		}
		else if(!weightbuffer.expire.empty()) // 버퍼 내에 삭제 가능한 데이터가 있는 경우 -> 가능
		{
			weightbuffer.expire.erase(weightbuffer.expire.begin());
			present_w_req -= MAX_READ_BYTE;
			return true;
		}
		else // 모든 데이터가 사용중이므로 request 불가?
		{
			return false;
		}
	}
	else
	{
		if(weightbuffersize - present_w_req >= MAX_READ_BYTE * 2) // 버퍼가 아직 다 안채워진 경우 -> 가능
		{
			return true;
		}
		else if(weightbuffer.expire.size() >= 2) // 버퍼 내에 삭제 가능한 데이터가 있는 경우 -> 가능
		{
			weightbuffer.expire.erase(weightbuffer.expire.begin(), weightbuffer.expire.begin() + 2);
			present_w_req -= MAX_READ_BYTE * 2;
			return true;
		}
		else // 모든 데이터가 사용중이므로 request 불가?
		{
			return false;
		}
	}
}

void BufferInterface::Request(uint64_t address)
{
	WB_Data t = {address, 1};
	weightbuffer.request.push_back(t);
	present_w_req += MAX_READ_BYTE;
}

bool BufferInterface::Requested(uint64_t address)
{
	if(!weightbuffer.request.empty())
	{
		vector<WB_Data>::iterator iter;
		for(iter = weightbuffer.request.begin(); iter != weightbuffer.request.end(); iter++)
		{
			if(iter->address == address) // already requested
			{
				iter->req += 1;
				return true;
			}
		}
	}
	
	return false;
}

bool BufferInterface::isExist(uint64_t address) // for weight address
{
	vector<WB_Data>::iterator iter;
	if(!weightbuffer.active.empty())
	{
		for(iter = weightbuffer.active.begin(); iter != weightbuffer.active.end(); iter++)
		{
			if (iter->address == address) // hit
			{
				iter->req += 1;
				return true;
			}
		}
	}
	if(!weightbuffer.expire.empty())
	{
		for(iter = weightbuffer.expire.begin(); iter != weightbuffer.expire.end(); iter++) {
			if (iter->address == address) // hit
			{
				WB_Data t = {address, 1};
				weightbuffer.active.push_back(t);
				weightbuffer.expire.erase(iter);
				return true;
			}
		}
	}

	// buffer 안에 없음
	return false;
}

bool BufferInterface::Expire(uint64_t address) // 특정 address만 expire하기 위한 용도
{
	if(!weightbuffer.active.empty())
	{
		vector<WB_Data>::iterator iter;
		for(iter = weightbuffer.active.begin(); iter != weightbuffer.active.end(); iter++) {
			if (iter->address == address)
			{
				if(iter->req <= 1) // expire 됨
				{
					WB_Data t = {address, 0};
					weightbuffer.expire.push_back(t);
					weightbuffer.active.erase(iter);
				}
				else // 아직 남은 request 존재
				{
					iter->req -= 1;
				}
				return true;
			}
		}
	}

	return false;
}