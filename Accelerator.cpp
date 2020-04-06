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
	limit_ax_req = buffer_->axbuffersize;
	limit_w_req = buffer_->weightbuffersize;
	mask = (0xffffffffffffffff - MAX_READ_BYTE + 1);

	cheat.valindex = 0;
	cheat.colindex = 0;
	cheat.rowindex = 0;
	flag = {false, false, false, false, true, false, false, false, true, false, true, true, true};
	endflag = {false, false, false, false, false};
	macflag = {true, false, false, false, false, false};
	reqflag = {false, false, false, false};
	macover = false;
	programover = false;
	stop = false;
	jc1 = false;
	jc2 = false;
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
	/*
	cout << buffer->ARowEnd()<<" "<<buffer->AColEnd()<<endl;
	cout << buffer->AuxARowEnd()<<" "<<buffer->AuxAColEnd()<<endl;
	cout<<"......................"<<endl;
	*/
	RequestControllerRun();
	MACControllerRun();

	if (flag.mac_1 && macover && !programover)
	{
		present_w_fold++;
		buffer->Reset();
		Reset();
		flag.x_row_req = true;
		flag.x_val_req = false;
		flag.x_col_req = false;
		flag.weight_req = false;
		endflag.x_row_end = false;
		endflag.x_col_end = false;
		endflag.x_val_end = false;
		present_v_fold = 0;
		buffer->present_ax_req = 0;
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
	if (buffer->present_ax_req >= limit_ax_req)
	{
		flag.ax_req_ok = false;
	}
	else
	{
		flag.ax_req_ok = true;
	}
	if (!buffer->canRequest())
	{
		flag.w_req_ok = false;
	}
	else
	{
		flag.w_req_ok = true;
	}
	if (flag.a_row_req && !buffer->IsFilled(A_ROW) && flag.ax_req_ok)
	{
		Request(A_ROW);
		buffer->present_ax_req += MAX_READ_BYTE;
		if (!endflag.a_col_end)
		{
			flag.a_row_req = false;
			flag.a_col_req = true;
		}
	}
	else if (flag.a_col_req && flag.ax_req_ok)
	{
		if (stop)
		{
			if (!buffer->AuxIsFulled(false))
			{
				remain_col_num = buffer->PopData(A_ROW);
				present_row++;
				flag.a_col_req = false;
				stop = false;
				return;
			}
			else
			{
				return;
			}
		}
		Request(A_COL);
		buffer->present_ax_req += MAX_READ_BYTE;
		if (buffer->IsFilled(A_ROW) && buffer->IsFilled(A_COL) && flag.ax_req_ok)
		{
			if (remain_col_num == 0)
			{
				if (!buffer->AuxIsFulled(false))
				{
					remain_col_num = buffer->PopData(A_ROW);
					present_row++;
					flag.a_col_req = false;
				}
				else
				{
					stop = true;
					return;
				}
			}
			else
			{
				flag.weight_req = true;
				flag.a_col_req = false;
			}
		}
	}
	else if (flag.x_row_req && !buffer->IsFilled(X_ROW) && flag.ax_req_ok)
	{
		Request(X_ROW);
		buffer->present_ax_req += MAX_READ_BYTE;
		if (!endflag.x_col_end && !endflag.x_val_end)
		{
			flag.x_row_req = false;
			flag.x_col_req = true;
			flag.x_val_req = true;
		}
	}
	else if (flag.x_col_req && flag.x_val_req && flag.ax_req_ok)
	{
		if (stop)
		{
			if (!buffer->AuxIsFulled(false))
			{
				remain_col_num = buffer->PopData(X_ROW);
				present_row++;
				flag.x_col_req = false;
				flag.x_val_req = false;
				stop = false;
				return;
			}
			else
			{
				return;
			}
		}
		Request(X_COL);
		Request(X_VAL);
		buffer->present_ax_req += MAX_READ_BYTE;
		buffer->present_ax_req += MAX_READ_BYTE;
		if (buffer->IsFilled(X_ROW) && buffer->IsFilled(X_COL) && buffer->IsFilled(X_VAL))
		{
			if (remain_col_num == 0)
			{
				if (!buffer->AuxIsFulled(false))
				{
					remain_col_num = buffer->PopData(X_ROW);
					present_row++;
					flag.x_col_req = false;
					flag.x_val_req = false;
				}
				else
				{
					stop = true;
					return;
				}
			}
			else
			{
				flag.weight_req = true;
				flag.x_col_req = false;
				flag.x_val_req = false;
			}
		}
	}
	else if (flag.weight_req && flag.w_req_ok)
	{
		uint64_t address;
		uint64_t w_start_addr;
		uint64_t w_end_addr;

		if (flag.mac_1)
		{
			if (stop)
			{
				if (!buffer->AuxIsFulled(false) && reqflag.label1)
				{
					present_col = buffer->PopData(X_COL);
					remain_col_num--;
					reqflag.label1 = false;
					stop = false;
					goto label1;
				}
				else if (!buffer->AuxIsFulled(false) && reqflag.label2)
				{
					buffer->PopValData();
					reqflag.label2 = false;
					stop = false;
					goto label2;
				}
				else if (!buffer->AuxIsFulled(true) && reqflag.label3)
				{
					address = WEIGHT_START + (present_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
					w_start_addr = address & mask;
					w_end_addr = (address + 63) & mask;
					buffer->PassWeightAddress(w_start_addr, w_end_addr);
					reqflag.label3 = false;
					stop = false;
					goto label3;
				}
				else if (!buffer->AuxIsFulled(false) && reqflag.label4)
				{
					remain_col_num = buffer->PopData(X_ROW);
					present_row++;
					reqflag.label4 = false;
					stop = false;
					goto label4;
				}
				else
				{
					return;
				}
			}	
			if (!buffer->AuxIsFulled(false))
			{
				present_col = buffer->PopData(X_COL);
				remain_col_num--;
			}
			else
			{
				stop = true;
				reqflag.label1 = true;
				return;
			}
		label1:
			if (!buffer->AuxIsFulled(false))
			{
				buffer->PopValData();
			}
			else
			{
				stop = true;
				reqflag.label2 = true;
				return;
			}
		label2:
			if (!buffer->AuxIsFulled(true))
			{
				address = WEIGHT_START + (present_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
				w_start_addr = address & mask;
				w_end_addr = (address + 63) & mask;
				buffer->PassWeightAddress(w_start_addr, w_end_addr);
			}
			else
			{
				stop = true;
				reqflag.label3 = true;
				return;
			}
		label3:
			if (!buffer->isExist(w_start_addr) && !buffer->Requested(w_start_addr))
			{
				RequestWeight(w_start_addr);
			}
			if (!buffer->isExist(w_end_addr) && !buffer->Requested(w_end_addr))
			{
				RequestWeight(w_end_addr);
			}
			if (!buffer->XColEnd() && !buffer->XValEnd() && remain_col_num == 0)
			{
				if (buffer->IsFilled(X_ROW))
				{
					if (!buffer->AuxIsFulled(false))
					{
						remain_col_num = buffer->PopData(X_ROW);
						present_row++;
					}
					else
					{
						stop = true;
						reqflag.label4 = true;
						return;
					}
				label4:
					if (remain_col_num == 0 && buffer->IsFilled(X_ROW))
					{
						flag.weight_req = false;	
					}
					else if (remain_col_num == 0 && !buffer->IsFilled(X_ROW) && !endflag.x_row_end)
					{
						flag.x_row_req = true;
						flag.weight_req = false;
					}
				}
				else if (!endflag.x_row_end)
				{
					flag.x_row_req = true;
					flag.weight_req = false;
				}
				else
				{
					flag.weight_req = false;
				}
			}
			else if (!buffer->IsFilled(X_COL) && !buffer->IsFilled(X_VAL) && !endflag.x_col_end && !endflag.x_val_end)
			{
				flag.x_col_req = true;
				flag.x_val_req = true;
				flag.weight_req = false;
			}
			else if (buffer->XColEnd() && buffer->XValEnd() && remain_col_num == 0)
			{
				flag.weight_req = false;
				if (!buffer->XRowEnd())
				{
					if (!buffer->AuxIsFulled(false))
					{
						buffer->PopData(X_ROW);
						present_row++;
					}
					else
					{
						stop = true;
						return;
					}
				}
			}
		}
		else
		{
			if (stop)
			{
				if (!buffer->AuxIsFulled(false) && reqflag.label1)
				{
					present_col = buffer->PopData(A_COL);
					remain_col_num--;
					reqflag.label1 = false;
					stop = false;
					goto label5;
				}
				else if (!buffer->AuxIsFulled(true) && reqflag.label2)
				{
					address = OUTPUT_START + (present_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
					w_start_addr = address & mask;
					w_end_addr = (address + 63) & mask;
					buffer->PassWeightAddress(w_start_addr, w_end_addr);
					reqflag.label2 = false;
					stop = false;
					goto label6;
				}
				else if (!buffer->AuxIsFulled(false) && reqflag.label3)
				{
					remain_col_num = buffer->PopData(A_ROW);
					present_row++;
					reqflag.label3 = false;
					stop = false;
					goto label7;
				}
				else
				{
					return;
				}
			}
			if (!buffer->AuxIsFulled(false))
			{
				present_col = buffer->PopData(A_COL);
				remain_col_num--;
			}
			else
			{
				stop = true;
				reqflag.label1 = true;
				return;
			}
		label5:
			if (!buffer->AuxIsFulled(true))
			{
				address = OUTPUT_START + (present_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
				w_start_addr = address & mask;
				w_end_addr = (address + 63) & mask;
				buffer->PassWeightAddress(w_start_addr, w_end_addr);
			}
			else
			{
				stop = true;
				reqflag.label2 = true;
				return;
			}
		label6:
			if (!buffer->isExist(w_start_addr) && !buffer->Requested(w_start_addr))
			{
				RequestWeight(w_start_addr);
			}
			if (!buffer->isExist(w_end_addr) && !buffer->Requested(w_end_addr))
			{
				RequestWeight(w_end_addr);
			}
			if (!buffer->AColEnd() && remain_col_num == 0)
			{
				if (buffer->IsFilled(A_ROW))
				{
					if (!buffer->AuxIsFulled(false))
					{
						remain_col_num = buffer->PopData(A_ROW);
						present_row++;
					}
					else
					{
						stop = true;
						reqflag.label3 = true;
						return;
					}
				label7:
					if (remain_col_num == 0 && buffer->IsFilled(A_ROW))
						flag.weight_req = false;
					else if (remain_col_num == 0 && !buffer->IsFilled(A_ROW) && !endflag.a_row_end)
					{
						flag.a_row_req = true;
						flag.weight_req = false;
					}
				}
				else if (!endflag.a_row_end)
				{
					flag.a_row_req = true;
					flag.weight_req = false;
				}
				else
				{
					flag.weight_req = false;
				}
			}
			else if (!buffer->IsFilled(A_COL) && !endflag.a_col_end)
			{
				flag.a_col_req = true;
				flag.weight_req = false;
			}
			else if (buffer->AColEnd() && remain_col_num == 0) 
			{
				flag.weight_req = false;
				if (!buffer->ARowEnd())
				{
					if (!buffer->AuxIsFulled(false))
					{
						buffer->PopData(A_ROW);
						present_row++;
					}
					else
					{
						stop = true;
					}
				}
			}
		}
	}
	else
	{
		if (flag.mac_1)
		{
			if (stop)
			{
				if (!buffer->AuxIsFulled(false) && jc1)
				{
					remain_col_num = buffer->PopData(X_ROW);
					present_row++;
					if (remain_col_num != 0)
					{
						flag.weight_req = true;
						flag.x_col_req = false;
						flag.x_val_req = false;
					}
					stop = false;
					jc1 = false;
					return;
				}
				else if (!buffer->AuxIsFulled(false) && jc2)
				{
					remain_col_num = buffer->PopData(X_ROW);
					present_row++;
					stop = false;
					jc2 = false;
					return;
				}
				else
				{
					return;
				}
			}
			if (buffer->IsFilled(X_ROW) && buffer->IsFilled(X_COL) && buffer->IsFilled(X_VAL))
			{
				if (remain_col_num == 0)
				{
					if (!buffer->AuxIsFulled(false))
					{
						remain_col_num = buffer->PopData(X_ROW);
						present_row++;
						if (remain_col_num != 0)
						{
							flag.weight_req = true;
							flag.x_col_req = false;
							flag.x_val_req = false;
						}
					}
					else
					{
						stop = true;
						jc1 = true;
					}
				}
				else
				{
					flag.weight_req = true;
					flag.x_col_req = false;
					flag.x_val_req = false;
				}
					
			}
			else if (!buffer->XRowEnd() && buffer->XColEnd() && buffer->XValEnd())
			{
				if (!buffer->AuxIsFulled(false))
				{
					remain_col_num = buffer->PopData(X_ROW);
					present_row++;
				}
				else
				{
					stop = true;
					jc2 = true;
				}
			}
			else if (!buffer->IsFilled(X_ROW) && !endflag.x_row_end)
			{
				flag.x_row_req = true;
			}
			else if (!buffer->IsFilled(X_COL) && !buffer->IsFilled(X_VAL) && !endflag.x_col_end && !endflag.x_val_end)
			{
				flag.x_col_req = true;
				flag.x_val_req = true;
			}
		}
		else
		{
			if (stop)
			{
				if (!buffer->AuxIsFulled(false) && jc1)
				{
					remain_col_num = buffer->PopData(A_ROW);
					present_row++;
					if (remain_col_num != 0)
					{
						flag.weight_req = true;
						flag.a_col_req = false;
					}
					stop = false;
					jc1 = false;
					return;
				}
				else if (!buffer->AuxIsFulled(false) && jc2)
				{
					remain_col_num = buffer->PopData(A_ROW);
					present_row++;
					stop = false;
					jc2 = false;
					return;
				}
				else
				{
					return;
				}
			}
			if (buffer->IsFilled(A_ROW) && buffer->IsFilled(A_COL) && flag.ax_req_ok)
			{	
				if (remain_col_num == 0)
				{
					if (!buffer->AuxIsFulled(false))
					{
						remain_col_num = buffer->PopData(A_ROW);
						present_row++;
						if (remain_col_num != 0)
						{
							flag.weight_req = true;
							flag.a_col_req = false;
						}
					}
					else
					{
						stop = true;
						jc1 = true;
					}
				}
				else
				{
					flag.weight_req = true;
					flag.a_col_req = false;
				}
			}
			else if (!buffer->ARowEnd() && buffer->AColEnd())
			{
				if (!buffer->AuxIsFulled(false))
				{
					remain_col_num = buffer->PopData(A_ROW);
					present_row++;
				}
				else
				{
					stop = true;
					jc2 = true;
				}
			}
			else if (!buffer->IsFilled(A_ROW) && !endflag.a_row_end)
			{
				flag.a_row_req = true;
			}
			else if (!buffer->IsFilled(A_COL) && !endflag.a_col_end)
			{
				flag.a_col_req = true;
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
				macflag.second_get = true;
				if (remain_mac_col == 0)
				{
					macflag.maciszero = true;
				}
			}
			if (remain_mac_col != 0 && 
				buffer->AuxIsFilled(X_COL) && 
				buffer->AuxIsFilled(X_VAL) && 
				macflag.second_get &&
				!macflag.maciszero) //만일 zero row가 아니면
			{
				present.col = buffer->ReadMACData(X_COL);
				present.val = buffer->ReadValMACData();
				macflag.second_get = false;
				macflag.fold_start = true;
			}
			if (buffer->AuxIsFilled(WEIGHT) && macflag.fold_start && !macflag.maciszero)
			{
				macflag.fold_start = false;
				present.weight = buffer->ReadWeightTuple();
				if (buffer->isReady(present.weight.tuple[0]) && buffer->isReady(present.weight.tuple[1])) //준비가 되었는가?
					macflag.macisready = true;
			}
			else if (!macflag.first_get && !macflag.second_get && !macflag.fold_start && !macflag.maciszero) //준비 되었는가?
			{
				if (buffer->isReady(present.weight.tuple[0]) && buffer->isReady(present.weight.tuple[1]))
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
				macflag.v_fold_over = true;
				macflag.second_get = true;
				buffer->Expire(present.weight.tuple[0]);
				buffer->Expire(present.weight.tuple[1]);
				macflag.macisready = false;
				if (remain_mac_col == 0)
				{
					macflag.first_get = true;
					macflag.macisready = false;
					macflag.fold_start = false;
					cout<<"Row "<<dec<<present.row<<" is Complete."<<endl;
					address = OUTPUT_START + (present.row * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
					dram->DRAMRequest(address, true);
					if (buffer->XEnd())
						macover = true;
				}
			}
		}
		else if (macflag.maciszero) // zero인 경우 계산 
		{
			macflag.v_fold_over = true;
			macflag.maciszero = false;
			macflag.first_get = true;
			address = OUTPUT_START + (present.row * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
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
				macflag.second_get = true;
				if (remain_mac_col == 0)
				{
					macflag.maciszero = true;
				}
			}
			if (remain_mac_col != 0 && 
				buffer->AuxIsFilled(A_COL) &&
				macflag.second_get &&
				!macflag.maciszero)
			{
				present.col = buffer->ReadMACData(A_COL);
				macflag.second_get = false;
				macflag.fold_start = true;
			}
			if (buffer->AuxIsFilled(WEIGHT) && macflag.fold_start && !macflag.maciszero)
			{
				macflag.fold_start = false;
				present.weight = buffer->ReadWeightTuple();
				if (buffer->isReady(present.weight.tuple[0]) && buffer->isReady(present.weight.tuple[1]))
					macflag.macisready = true;
			}
			else if (!macflag.first_get && !macflag.second_get && !macflag.fold_start && !macflag.maciszero)
			{
				if (buffer->isReady(present.weight.tuple[0]) && buffer->isReady(present.weight.tuple[1]))
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
				macflag.v_fold_over = true;
				macflag.second_get = true;
				buffer->Expire(present.weight.tuple[0]);
				buffer->Expire(present.weight.tuple[1]);
				macflag.macisready = false;
				if (remain_mac_col == 0)
				{
					macflag.first_get = true;
					macflag.macisready = false;
					macflag.fold_start = false;
					cout<<"Row "<<dec<<present.row<<" is Complete."<<endl;
					address = OUTPUT_START + (present.row * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
					dram->DRAMRequest(address, true);
					if (buffer->AEnd())
						macover = true;
				}
			}
		}
		else if (macflag.maciszero)
		{
			macflag.v_fold_over = true;
			macflag.maciszero = false;
			macflag.first_get = true;
			address = OUTPUT_START + (present.row * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
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
			{
				flag.a_col_req = false;
				endflag.a_col_end = true;
			}
			dram->DRAMRequest(address, false);
			cout<<"Request A_COLUMN. Address: "<<hex<<address<<endl;
			break;
		case A_ROW:
			address = a_row_addr;
			a_row_addr += MAX_READ_BYTE;
			cheat.rowindex += MAX_READ_INT;
			if (cheat.rowindex >= buffer->data->adjrowindex.size())
			{
				flag.a_row_req = false;
				endflag.a_row_end = true;;
			}
			dram->DRAMRequest(address, false);
			cout<<"Request A_ROW. Address: "<<hex<<address<<endl;
			break;
		case X_VAL:
			address = x_val_addr;
			x_val_addr += MAX_READ_BYTE;
			cheat.valindex += MAX_READ_INT;
			if (cheat.valindex >= buffer->data->ifvalue.size())
			{
				flag.x_val_req = false;
				endflag.x_val_end = true;
			}
			dram->DRAMRequest(address, false);
			cout<<"Request X_VALUE. Address: "<<hex<<address<<endl;
			break;
		case X_COL:
			address = x_col_addr;
			x_col_addr += MAX_READ_BYTE;
			cheat.colindex += MAX_READ_INT;
			if (cheat.colindex >= buffer->data->ifcolindex.size())
			{
				flag.x_col_req = false;
				endflag.x_col_end = true;
			}
			dram->DRAMRequest(address, false);
			cout<<"Request X_COLUMN. Address: "<<hex<<address<<endl;
			break;
		case X_ROW:
			address = x_row_addr;
			x_row_addr += MAX_READ_BYTE;
			cheat.rowindex += MAX_READ_INT;
			if (cheat.rowindex >= buffer->data->ifrowindex.size())
			{
				flag.x_row_req = false;
				endflag.x_row_end = true;
			}
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
	macflag = {true, false, false, false, false, false};
	macover = false;
}
