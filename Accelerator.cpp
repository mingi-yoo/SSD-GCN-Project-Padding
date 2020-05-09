/*******************************************************************
* SSD GCN Project
********************************************************************/

#include "Accelerator.h"

using namespace std;

extern uint64_t cycle;

Accelerator::Accelerator(uint64_t accdimension, DRAMInterface *dram_, BufferInterface *buffer_) 
{
	num_of_pe = accdimension;
	a_col_addr = A_COL_START;
	a_row_addr = A_ROW_START;
	x_val_addr = X_VAL_START;
	x_col_addr = X_COL_START;
	x_row_addr = X_ROW_START;

	w_fold = buffer_->weightsize.tuple[1]/MAX_READ_INT;
	if (buffer_->weightsize.tuple[1] == w_fold * MAX_READ_BYTE && w_fold > 0)
		w_fold--;
	v_fold = MAX_READ_INT/accdimension;
	if (MAX_READ_INT == v_fold * accdimension && v_fold > 1)
		v_fold--;
	v_fold_last = (buffer_->weightsize.tuple[1] - w_fold * MAX_READ_INT)/accdimension;
	if ((buffer_->weightsize.tuple[1] - w_fold * MAX_READ_INT) == v_fold_last * accdimension && v_fold_last > 0)
		v_fold_last--;
	present_w_fold = 0;
	present_v_fold = 0;
	present_mac_row = -1;
	present_row = -1;

	dram = dram_;
	buffer = buffer_;
	buffer->mac1_count = buffer->mac1_count * (w_fold + 1);
	buffer->mac2_count = buffer->mac2_count * (w_fold + 1);

	remain_col_num = 0;
	remain_mac_col = 0;
	limit_ax_req = buffer->axbuffersize;

	cheat.valindex = 0;
	cheat.colindex = 0;
	cheat.rowindex = 0;
	flag = {false, false, false, false, true, false, false, true, false};
	endflag = {false, false, false, false, false};
	macflag = {true, false, false, false};
	macover = false;
	programover = false;
}

Accelerator::~Accelerator() {}

//controller 나눌것
//보내는 것 한 사이클
//컨트롤러 동시접근 -> bank문제

bool Accelerator::Run()
{
	uint64_t address;
	Tuple tuple; 
	uint64_t req_w_col;
	bool ret;

	ret = true;

	RequestControllerRun();
	MACControllerRun();

	if (flag.mac_1 && macover && !programover)
	{
		present_w_fold++;
		buffer->Reset();
		Reset();
		flag.x_val_req = false; // dummy
		flag.x_row_req = true;
		flag.x_col_req = false;
		flag.weight_req = false;
		endflag.x_row_end = false;
		endflag.x_col_end = false;
		endflag.x_val_end = false;
		present_v_fold = 0;
		if (present_w_fold > w_fold)
		{
			programover = true;
		}
	}
	if (flag.mac_1 && programover && buffer->mac1_count == 0)
	{
		present_w_fold = 0;
		flag.x_row_req = false;
		flag.a_row_req = true;
		flag.mac_1 = false;
		flag.mac_2 = true;
		buffer->isA = true;
		programover = false;
		cout<<"MAC1 End...."<<endl;
	}
	if (flag.mac_2 && macover && !programover)
	{
		present_w_fold++;
		buffer->Reset();
		Reset();
		flag.a_row_req = true;
		flag.a_col_req = false;
		flag.weight_req = false;
		endflag.a_row_end = false;
		endflag.a_col_end = false;
		present_v_fold = 0;
		if (present_w_fold > w_fold)
		{
			programover = true;			
		}
	}
	if (flag.mac_2 && programover && buffer->mac2_count == 0)
	{
		ret = false;
	}
	
	return ret;
}

void Accelerator::RequestControllerRun()
{
	if (programover)
		return;
	if (flag.mac_1)
	{
		if (buffer->XRowEnd() && buffer->XColEnd() && buffer->XValEnd() && !flag.weight_uncompleted)
			return;
		if ((buffer->IsFilled(X_ROW) && buffer->IsFilled(X_COL) && buffer->IsFilled(X_VAL) && !flag.weight_uncompleted) ||
			(remain_col_num != 0 && buffer->IsFilled(X_COL) && buffer->IsFilled(X_VAL) && !flag.weight_uncompleted))
		{
			flag.x_row_req = false;
			flag.x_col_req = false;
			flag.weight_req = true;
			//cout<<"1"<<endl;
		}
		if (flag.x_row_req && !endflag.x_row_end && ((buffer->present_ax_req + buffer->HowMuchAXFilled()) <= (limit_ax_req - MAX_READ_BYTE)))
		{
			Request(X_ROW);
			buffer->present_ax_req += MAX_READ_BYTE;
			if (!endflag.x_col_end)
			{
				flag.x_row_req = false;
				flag.x_col_req = true;
				//cout<<"2"<<endl;
			}
		}
		else if (flag.x_col_req && !endflag.x_col_end && ((buffer->present_ax_req + buffer->HowMuchAXFilled()) <= (limit_ax_req - 2 * MAX_READ_BYTE)))
		{
			Request(X_COL);
			Request(X_VAL);
			buffer->present_ax_req += 2 * MAX_READ_BYTE;
			if (buffer->IsFilled(X_ROW) && buffer->IsFilled(X_COL) && buffer->IsFilled(X_VAL))
			{
				flag.x_col_req = false;
				flag.weight_req = true;
				//cout<<"3"<<endl;
			}
		}
		else if (flag.weight_req && !buffer->AuxIsFulled())
		{
			if (remain_col_num == 0)
			{
				remain_col_num = buffer->PopData(X_ROW);
				present_row++;
				//cout<<"4"<<endl;
				if (remain_col_num == 0)
				{
					//cout<<"5"<<endl;
					return;
				}
			}
			//cout<<"6"<<endl;
			present_col = buffer->PopData(X_COL);
			buffer->PopValData();
			remain_col_num--;
			temp.w_addr = WEIGHT_START + (present_col * (w_fold + 1) + present_w_fold ) * MAX_READ_BYTE;
			if (!buffer->isExist(temp.w_addr) && !buffer->Requested(temp.w_addr))
				temp.check = true;
			else
				temp.check = false;
			if (temp.check && buffer->canRequest(true))
			{
				RequestWeight(temp.w_addr);
			}
			else
			{
				if (temp.check)
				{
					flag.weight_req = false;
					flag.weight_uncompleted = true;
					//cout<<"7"<<endl;
					return;
				}
			}
			if (remain_col_num == 0 && !buffer->IsFilled(X_ROW) && !endflag.x_row_end)
			{
				flag.weight_req = false;
				flag.x_row_req = true;
				//cout<<"8"<<endl;
			}
			else if (!buffer->IsFilled(X_COL) && 
					 !buffer->IsFilled(X_VAL) &&
					 !endflag.x_col_end &&
					 !endflag.x_row_end)
			{
				flag.weight_req = false;
				flag.x_col_req = true;
				//cout<<"9"<<endl;
			}
			else if ((remain_col_num == 0 && 
					 !buffer->IsFilled(X_ROW) && 
					 endflag.x_row_end &&
					 buffer->IsFilled(X_COL) &&
					 buffer->IsFilled(X_VAL)) ||
					 (buffer->XRowEnd() &&
					 buffer->XColEnd() &&
					 buffer->XValEnd()))
			{
				flag.weight_req = false;
				//cout<<"10"<<endl;
			}
		}
		else if (flag.weight_uncompleted)
		{
			if (temp.check && buffer->canRequest(true))
			{
				RequestWeight(temp.w_addr);
				flag.weight_uncompleted = false;
				temp.check = false;
			}
			if (!flag.weight_uncompleted)
			{
				if (remain_col_num == 0 && !buffer->IsFilled(X_ROW) && !endflag.x_row_end)
				{
					flag.weight_req = false;
					flag.x_row_req = true;
					//cout<<"11"<<endl;
				}
				else if (!buffer->IsFilled(X_COL) && 
						 !buffer->IsFilled(X_VAL) &&
						 !endflag.x_col_end &&
						 !endflag.x_row_end)
				{
					flag.weight_req = false;
					flag.x_col_req = true;
					//cout<<"12"<<endl;
				}
				else if ((remain_col_num == 0 && 
						 !buffer->IsFilled(X_ROW) && 
						 endflag.x_row_end &&
						 buffer->IsFilled(X_COL) &&
						 buffer->IsFilled(X_VAL)) ||
						 (buffer->XRowEnd() &&
						 buffer->XColEnd() &&
						 buffer->XValEnd()))
				{
					flag.weight_req = false;
					//cout<<"13"<<endl;
				}
			}
		}
	}
	else
	{
		if (buffer->ARowEnd() && buffer->AColEnd() && !flag.weight_uncompleted)
			return;
		if ((buffer->IsFilled(A_ROW) && buffer->IsFilled(A_COL) && !flag.weight_uncompleted) ||
			(remain_col_num != 0 && buffer->IsFilled(A_COL) && !flag.weight_uncompleted))
		{
			flag.a_row_req = false;
			flag.a_col_req = false;
			flag.weight_req = true;
		}
		if (flag.a_row_req && !endflag.a_row_end && ((buffer->present_ax_req + buffer->HowMuchAXFilled()) <= (limit_ax_req - MAX_READ_BYTE)))
		{
			Request(A_ROW);
			buffer->present_ax_req += MAX_READ_BYTE;
			if (!endflag.a_col_end)
			{
				flag.a_row_req = false;
				flag.a_col_req = true;
			}
		}
		else if (flag.a_col_req && !endflag.a_col_end && ((buffer->present_ax_req + buffer->HowMuchAXFilled()) <= (limit_ax_req - MAX_READ_BYTE)))
		{
			Request(A_COL);
			buffer->present_ax_req += MAX_READ_BYTE;
			if (buffer->IsFilled(A_ROW) && buffer->IsFilled(A_COL))
			{
				flag.a_col_req = false;
				flag.weight_req = true;
			}
		}
		else if (flag.weight_req && !buffer->AuxIsFulled())
		{
			if (remain_col_num == 0)
			{
				remain_col_num = buffer->PopData(A_ROW);
				present_row++;
				if (remain_col_num == 0)
				{
					return;
				}
			}
			present_col = buffer->PopData(A_COL);
			remain_col_num--;
			temp.w_addr = OUTPUT_START + (present_col * (w_fold + 1) + present_w_fold) * MAX_READ_BYTE;
			if (!buffer->isExist(temp.w_addr) && !buffer->Requested(temp.w_addr))
				temp.check = true;
			else
				temp.check = false;
			if (temp.check && buffer->canRequest(true))
			{
				RequestWeight(temp.w_addr);
			}
			else
			{
				if (temp.check)
				{
					flag.weight_req = false;
					flag.weight_uncompleted = true;
					return;
				}
			}
			if (remain_col_num == 0 && !buffer->IsFilled(A_ROW) && !endflag.a_row_end)
			{
				flag.weight_req = false;
				flag.a_row_req = true;
			}
			else if (!buffer->IsFilled(A_COL) && 
					 !endflag.a_col_end)
			{
				flag.weight_req = false;
				flag.a_col_req = true;
			}
			else if ((remain_col_num == 0 && 
					 !buffer->IsFilled(A_ROW) && 
					 endflag.a_row_end &&
					 buffer->IsFilled(A_COL)) ||
					 (buffer->ARowEnd() &&
					 buffer->AColEnd()))
			{
				flag.weight_req = false;
			}
		}
		else if (flag.weight_uncompleted)
		{
			if (temp.check && buffer->canRequest(true))
			{
				RequestWeight(temp.w_addr);
				flag.weight_uncompleted = false;
				temp.check = false;
			}
			if (!flag.weight_uncompleted)
			{
				if (remain_col_num == 0 && !buffer->IsFilled(A_ROW) && !endflag.a_row_end)
				{
					flag.weight_req = false;
					flag.a_row_req = true;
				}
				else if (!buffer->IsFilled(A_COL) && 
						 !endflag.a_col_end)
				{
					flag.weight_req = false;
					flag.a_col_req = true;
				}
				else if ((remain_col_num == 0 && 
						 !buffer->IsFilled(A_ROW) && 
						 endflag.x_row_end &&
						 buffer->IsFilled(A_COL)) ||
						 (buffer->ARowEnd() &&
					 	 buffer->AColEnd()))
				{
					flag.weight_req = false;
				}
			}
		}
	}
	
}

void Accelerator::MACControllerRun()
{
	uint64_t address;

	if (programover)
		return;
	if (flag.mac_1)
	{
		if (!macflag.macisready) //맨 처음 상태 
		{
			if (buffer->AuxIsFilled(X_ROW) && macflag.first_get) //present라는 변수안에 처리해야할 데이터 집어넣음
			{
				remain_mac_col = buffer->ReadMACData(X_ROW);
				present_mac_row++;
				present.row = present_mac_row;
				macflag.first_get = false;
				macflag.fold_start = true;
			}
			if (remain_mac_col != 0 && buffer->AuxIsFilled(X_COL) && buffer->AuxIsFilled(X_VAL) && macflag.fold_start) //만일 zero row가 아니면
			{
				present.col = buffer->ReadMACData(X_COL);
				present.val = buffer->ReadValMACData();
				macflag.fold_start = false;
				present.weight =  WEIGHT_START + (present.col * (w_fold + 1) + present_w_fold) * MAX_READ_BYTE;
				if (buffer->isReady(present.weight)) //준비가 되었는가?
					macflag.macisready = true;
			}
			else if (remain_mac_col == 0 && macflag.fold_start) //만일 zero row이면 maciszero 켜기
			{
				macflag.maciszero = true;
			}
			else if (!macflag.first_get && !macflag.fold_start) //준비 되었는가?
			{
				if (buffer->isReady(present.weight))
					macflag.macisready = true;
			}
		}
		if (macflag.macisready) // 준비됐으면 계산
		{
			cout<<"MAC1 Running... v_fold: "<<dec<<present_v_fold<<
			", w_fold: "<<dec<<present_w_fold<<
			", Row: "<<dec<<present.row<<
			", Col: "<<dec<<present.col<<
			", Val: "<<present.val<<endl;
			present_v_fold++;
			if ((present_v_fold > v_fold && present_w_fold < w_fold) 
				|| (present_v_fold > v_fold_last && present_w_fold == w_fold))
			{
				present_v_fold = 0;
				remain_mac_col--;
				macflag.fold_start = true;
				buffer->Expire(present.weight);
				macflag.macisready = false;
				if (remain_mac_col == 0)
				{
					macflag.first_get = true;
					macflag.macisready = false;
					macflag.fold_start = false;
					cout<<"Row "<<dec<<present.row<<" is Complete."<<endl;
					address = OUTPUT_START + (present.row * (w_fold + 1) + present_w_fold ) * MAX_READ_BYTE;
					dram->DRAMRequest(address, true);
					if (buffer->XEnd())
						macover = true;
				}
			}
		}
		else if (macflag.maciszero) // zero인 경우 계산 
		{
			macflag.maciszero = false;
			macflag.first_get = true;
			address = OUTPUT_START + (present.row * (w_fold + 1) + present_w_fold) * MAX_READ_BYTE;
			cout<<"MAC1 Running... Row: "<<dec<<present.row<<" is zero row...."<<endl;
			dram->DRAMRequest(address, true);
			if (buffer->XEnd())
				macover = true;
		}
	}
	else
	{
		if (!macflag.macisready)
		{
			if (buffer->AuxIsFilled(A_ROW) && macflag.first_get)
			{
				remain_mac_col = buffer->ReadMACData(A_ROW);
				present_mac_row++;
				present.row = present_mac_row;
				macflag.first_get = false;
				macflag.fold_start = true;
			}
			if (remain_mac_col != 0 && buffer->AuxIsFilled(A_COL) && macflag.fold_start)
			{
				present.col = buffer->ReadMACData(A_COL);
				macflag.fold_start = false;
				present.weight =  OUTPUT_START + (present.col * (w_fold + 1) + present_w_fold) * MAX_READ_BYTE;
				if (buffer->isReady(present.weight))
					macflag.macisready = true;
			}
			else if (remain_mac_col == 0 && macflag.fold_start)
			{
				macflag.maciszero = true;
			}
			else if (!macflag.first_get && !macflag.fold_start)
			{
				if (buffer->isReady(present.weight))
					macflag.macisready = true;
			}
		}
		if (macflag.macisready)
		{
			cout<<"MAC2 Running... v_fold: "<<dec<<present_v_fold<<
			", w_fold: "<<dec<<present_w_fold<<
			", Row: "<<dec<<present.row<<
			", Col: "<<dec<<present.col<<endl;
			present_v_fold++;
			if ((present_v_fold > v_fold && present_w_fold < w_fold) 
				|| (present_v_fold > v_fold_last && present_w_fold == w_fold))
			{
				present_v_fold = 0;
				remain_mac_col--;
				macflag.fold_start = true;
				buffer->Expire(present.weight);
				macflag.macisready = false;
				if (remain_mac_col == 0)
				{
					macflag.first_get = true;
					macflag.macisready = false;
					macflag.fold_start = false;
					cout<<"Row "<<dec<<present.row<<" is Complete."<<endl;
					address = OUTPUT_START2 + (present.row * (w_fold + 1) + present_w_fold) * MAX_READ_BYTE;
					dram->DRAMRequest(address, true);
					if (buffer->AEnd())
						macover = true;
				}
			}
		}
		else if (macflag.maciszero)
		{
			macflag.maciszero = false;
			macflag.first_get = true;
			address = OUTPUT_START2 + (present.row * (w_fold + 1) + present_w_fold) * MAX_READ_BYTE;
			cout<<"MAC2 Running... Row: "<<dec<<present.row<<" is zero row...."<<endl;
			dram->DRAMRequest(address, true);
			if (buffer->AEnd())
				macover = true;
		}
	}
}

void Accelerator::Request(Type iswhat)
{
	uint64_t address;

	switch (iswhat)
	{
		case A_COL:
			address = a_col_addr;
			a_col_addr += MAX_READ_BYTE;
			cheat.colindex += MAX_READ_INT;
			if (cheat.colindex >= buffer->data->adjcolindex.size())
				endflag.a_col_end = true;
			dram->DRAMRequest(address, false);
			cout<<"Request A_COLUMN. Address: "<<hex<<address<<endl;
			break;
		case A_ROW:
			address = a_row_addr;
			a_row_addr += MAX_READ_BYTE;
			cheat.rowindex += MAX_READ_INT;
			if (cheat.rowindex >= buffer->data->adjrowindex.size())
				endflag.a_row_end = true;;
			dram->DRAMRequest(address, false);
			cout<<"Request A_ROW. Address: "<<hex<<address<<endl;
			break;
		case X_VAL:
			address = x_val_addr;
			x_val_addr += MAX_READ_BYTE;
			cheat.valindex += MAX_READ_INT;
			if (cheat.valindex >= buffer->data->ifvalue.size())
				endflag.x_val_end = true;
			dram->DRAMRequest(address, false);
			cout<<"Request X_VALUE. Address: "<<hex<<address<<endl;
			break;
		case X_COL:
			address = x_col_addr;
			x_col_addr += MAX_READ_BYTE;
			cheat.colindex += MAX_READ_INT;
			if (cheat.colindex >= buffer->data->ifcolindex.size())
				endflag.x_col_end = true;
			dram->DRAMRequest(address, false);
			cout<<"Request X_COLUMN. Address: "<<hex<<address<<endl;
			break;
		case X_ROW:
			address = x_row_addr;
			x_row_addr += MAX_READ_BYTE;
			cheat.rowindex += MAX_READ_INT;
			if (cheat.rowindex >= buffer->data->ifrowindex.size())
				endflag.x_row_end = true;
			dram->DRAMRequest(address, false);
			cout<<"Request X_ROW. Address: "<<hex<<address<<endl;
			break;
	}
}

void Accelerator::RequestWeight(uint64_t address)
{
	dram->DRAMRequest(address, false);
	buffer->Request(address);
	cout<<"Request WEIGHT. Address: "<<hex<<address<<endl;
}

void Accelerator::Reset()
{
	a_col_addr = A_COL_START;
	a_row_addr = A_ROW_START;
	x_val_addr = X_VAL_START;
	x_col_addr = X_COL_START;
	x_row_addr = X_ROW_START;
	present_row = -1;
	present_mac_row = -1;
	cheat.rowindex = 0;
	cheat.colindex = 0;
	cheat.valindex = 0;
	macflag = {true, false, false, false};
	macover = false;
}
