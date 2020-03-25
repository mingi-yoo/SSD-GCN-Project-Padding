/*******************************************************************
* SSD GCN Project
********************************************************************/

#include "Accelerator.h"

using namespace std;

Accelerator::Accelerator(uint64_t accdimension, DRAMInterface *dram_, BufferInterface *buffer_) 
{
	num_of_pe = accdimension;
	a_col_addr = A_COL_START;
	a_row_addr = A_ROW_START;
	x_val_addr = X_VAL_START;
	x_col_addr = X_COL_START;
	x_row_addr = X_ROW_START;

	w_fold = buffer_->weightsize.tuple[1]/MAX_READ_INT;
	v_fold = MAX_READ_INT/accdimension;
	v_fold_last = (buffer_->weightsize.tuple[1] - w_fold * MAX_READ_INT)/accdimension;
	present_w_fold = 0;
	present_v_fold = 0;

	dram = dram_;
	buffer = buffer_;

	remain_col_num = 0;
	remain_mac_col = 0;
	limit_ax_req = buffer_->axbuffersize;
	limit_w_req = buffer_->weightbuffersize;

	fold_start = false;
	v_fold_over = false;

	cheat.valindex = 0;
	cheat.colindex = 0;
	cheat.rowindex = 0;
	flag = {false, false, false, false, true, false, false, false, true, false, true, true, true};
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

	if (buffer->XEnd() && v_fold_over)
	{
		present_w_fold++;
		buffer->Reset();
		Reset();
		if (present_w_fold > w_fold)
		{
			present_w_fold = 0;
			flag.mac_1 = false;
			flag.mac_2 = true;
		}
	}
	if (buffer->AEnd() && v_fold_over)
	{
		present_w_fold++;
		buffer->Reset();
		Reset();
		if (present_w_fold > w_fold)
		{
			ret = false;
		}
	}
	RequestControllerRun();
	MACControllerRun();

	return ret;
}

void Accelerator::RequestControllerRun()
{
	if (buffer->present_ax_req >= limit_ax_req)
	{
		flag.ax_req_ok = false;
	}
	if (!buffer->canRequest())
	{
		flag.w_req_ok = false;
	}
	if (flag.a_row_req && !buffer->IsFilled(A_ROW) && flag.ax_req_ok)
	{
		Request(A_ROW);
		buffer->present_ax_req += MAX_READ_BYTE;
		flag.a_row_req = false;
		flag.a_col_req = true;
	}
	else if (flag.a_col_req)
	{
		Request(A_COL);
		buffer->present_ax_req += MAX_READ_BYTE;
		if (buffer->IsFilled(A_ROW) && buffer->IsFilled(A_COL) && flag.ax_req_ok)
		{
			while (remain_col_num == 0)
			{
				remain_col_num = buffer->PopData(A_ROW);
			}
			flag.weight_req = true;
			flag.a_col_req = false;
		}
	}
	else if (flag.x_row_req && !buffer->IsFilled(X_ROW) && flag.ax_req_ok)
	{
		Request(X_ROW);
		buffer->present_ax_req += MAX_READ_BYTE;
		flag.x_row_req = false;
		flag.x_col_req = true;
		flag.x_val_req = true;
	}
	else if (flag.x_col_req && flag.x_val_req && flag.ax_req_ok)
	{
		Request(X_COL);
		Request(X_VAL);
		buffer->present_ax_req += MAX_READ_BYTE;
		buffer->present_ax_req += MAX_READ_BYTE;
		if (buffer->IsFilled(X_ROW) && buffer->IsFilled(X_COL) && buffer->IsFilled(X_VAL))
		{
			while (remain_col_num == 0)
			{
				remain_col_num = buffer->PopData(X_ROW);
			}
			flag.weight_req = true;
			flag.x_col_req = false;
			flag.x_val_req = false;
		}
	}
	else if (flag.weight_req && flag.w_req_ok)
	{
		uint64_t address;
		if (flag.mac_1)
		{
			present_col = buffer->PopData(X_COL);
			remain_col_num--;
			buffer->PopValData();
			address = WEIGHT_START + (present_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
			if (!buffer->isExist(address))
				dram->DRAMRequest(address, false);
			if (remain_col_num == 0)
			{
				if (!buffer->IsFilled(X_ROW))
				{
					while (remain_col_num == 0)
					{
						remain_col_num = buffer->PopData(X_ROW);
					}
					if (!buffer->IsFilled(X_COL) && !buffer->IsFilled(X_VAL))
					{
						flag.x_col_req = true;
						flag.x_val_req = true;
					}
				}
				else
				{
					flag.x_row_req = true;
				}
				flag.weight_req = false;
			}
		}
		else
		{
			present_col = buffer->PopData(A_COL);
			remain_col_num--;
			address = OUTPUT_START + (present_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
			if (!buffer->isExist(address))
				dram->DRAMRequest(address, false);
			if (remain_col_num == 0)
			{
				if (!buffer->IsFilled(A_ROW))
				{
					while (remain_col_num == 0)
					{
						remain_col_num = buffer->PopData(A_ROW);
					}
					if (!buffer->IsFilled(A_COL))
					{
						flag.a_col_req = true;
					}
				}
				else
				{
					flag.a_row_req = true;
				}
				flag.weight_req = false;
			}
		}
		buffer->present_w_req += MAX_READ_BYTE;	
	}
	else
	{
		if (flag.mac_1)
		{
			if (buffer->IsFilled(X_ROW) && buffer->IsFilled(X_COL) && buffer->IsFilled(X_VAL))
			{
				while (remain_col_num == 0)
				{
					remain_col_num = buffer->PopData(X_ROW);
				}
				flag.weight_req = true;
				flag.x_col_req = false;
				flag.x_val_req = false;
			}
		}
		else
		{
			if (buffer->IsFilled(A_ROW) && buffer->IsFilled(A_COL) && flag.ax_req_ok)
			{	
				while (remain_col_num == 0)
				{
					remain_col_num = buffer->PopData(A_ROW);
				}
				flag.weight_req = true;
				flag.a_col_req = false;
			}
		}
	}
}

void Accelerator::MACControllerRun()
{
	uint64_t address;

	if (buffer->isready)
	{
		buffer->isready =false;
		if (flag.mac_1)
		{
			present_mac_val = buffer->ReadValMACData();
			if (present_v_fold == 0)
			{
				v_fold_over = false;
				while (remain_mac_col != 0)
				{
					remain_mac_col = buffer->ReadMACData(X_ROW);
					present_mac_row++;
				}
				present_mac_col = buffer->ReadMACData(X_COL);
			}
			cout<<"MAC1 Running... v_fold: "<< present_v_fold<<
			" w_fold"<<present_w_fold<<
			", Row: "<<present_mac_row<<
			", Column: "<<present_mac_col<<
			", Value: "<<present_mac_val<<endl;
			present_v_fold++;
			if ((present_v_fold > v_fold && present_w_fold < w_fold) 
				|| (present_v_fold > v_fold_last && present_w_fold == w_fold))
			{
				present_v_fold = 0;
				remain_mac_col--;
				v_fold_over = true;
				address = WEIGHT_START + (present_mac_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
				buffer->Expire(address);
				buffer->isready = false;
				if (remain_mac_col == 0)
				{
					cout<<"Row "<<present_mac_row<<" is Complete."<<endl;
					address = OUTPUT_START + (present_mac_row * buffer->weightsize.tuple[1] + present_v_fold * MAX_READ_INT) * UNIT_INT_BYTE;
					dram->DRAMRequest(address, true);				
				}
			}
		}
		else
		{
			if (present_v_fold == 0)
			{
				v_fold_over = false;
				while (remain_mac_col != 0)
				{
					remain_mac_col = buffer->ReadMACData(A_ROW);
					present_mac_row++;
				}
				present_mac_col = buffer->ReadMACData(A_COL);
			}
			cout<<"MAC2 Running... v_fold: "<< present_v_fold<<
			" w_fold"<<present_w_fold<<
			", Row: "<<present_mac_row<<
			", Column: "<<present_mac_col<<endl;
			present_v_fold++;
			if ((present_v_fold > v_fold && present_w_fold < w_fold) 
				|| (present_v_fold > v_fold_last && present_w_fold == w_fold))
			{
				present_v_fold = 0;
				remain_mac_col--;
				v_fold_over = true;
				address = OUTPUT_START + (present_mac_col * buffer->weightsize.tuple[1] + present_w_fold * MAX_READ_INT) * UNIT_INT_BYTE;
				buffer->Expire(address);
				buffer->isready = false;
				if (remain_mac_col == 0)
				{
					cout<<"Row "<<present_mac_row<<" is Complete."<<endl;
					address = OUTPUT2_START + (present_mac_row * buffer->weightsize.tuple[1] + present_v_fold * MAX_READ_INT) * UNIT_INT_BYTE;
					dram->DRAMRequest(address, true);				
				}
			}
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
				flag.a_col_req = false;
			dram->DRAMRequest(address, false);
			cout<<"Request A_COLUMN. Address: "<<address<<endl;
			break;
		case A_ROW:
			address = a_row_addr;
			a_row_addr += MAX_READ_BYTE;
			cheat.rowindex += MAX_READ_INT;
			if (cheat.rowindex >= buffer->data->adjrowindex.size())
				flag.a_row_req = false;
			dram->DRAMRequest(address, false);
			cout<<"Request A_ROW. Address: "<<address<<endl;
			break;
		case X_VAL:
			address = x_val_addr;
			x_val_addr += MAX_READ_BYTE;
			cheat.valindex += MAX_READ_INT;
			if (cheat.valindex >= buffer->data->ifvalue.size())
				flag.x_val_req = false;
			dram->DRAMRequest(address, false);
			cout<<"Request X_VALUE. Address: "<<address<<endl;
			break;
		case X_COL:
			address = x_col_addr;
			x_col_addr += MAX_READ_BYTE;
			cheat.colindex += MAX_READ_INT;
			if (cheat.colindex >= buffer->data->ifcolindex.size())
				flag.x_col_req = false;
			dram->DRAMRequest(address, false);
			cout<<"Request X_COLUMN. Address: "<<address<<endl;
			break;
		case X_ROW:
			address = x_row_addr;
			x_row_addr += MAX_READ_BYTE;
			cheat.rowindex += MAX_READ_INT;
			if (cheat.rowindex >= buffer->data->ifrowindex.size())
				flag.x_row_req = false;
			dram->DRAMRequest(address, false);
			cout<<"Request X_ROW. Address: "<<address<<endl;
			break;
	}
}

void Accelerator::Reset()
{
	a_col_addr = A_COL_START;
	a_row_addr = A_ROW_START;
	x_val_addr = X_VAL_START;
	x_col_addr = X_COL_START;
	x_row_addr = X_ROW_START;
}