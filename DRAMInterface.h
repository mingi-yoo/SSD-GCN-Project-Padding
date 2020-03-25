/*******************************************************************
* SSD GCN Project
********************************************************************/

#ifndef __DRAMINTERFACE_H__
#define __DRAMINTERFACE_H__

#include <iostream>
#include <string>
#include <DRAMSim.h>
#include "BufferInterface.h"
#include "Common.h"

using namespace std;

class DRAMInterface {
public:
	DRAMInterface(const string& configfile, 
				  const string& systemfile, 
				  const string& logfile, 
				  const string& outputfile, 
				  unsigned memorysize, 
				  float clk_period_in_ns,
				  BufferInterface *buffer_);
	~DRAMInterface();
	void UpdateCycle();
	void DRAMRequest(uint64_t address, bool is_write);
	void SetResigsterCallback(DRAMSim::TransactionCompleteCB *read_cb, DRAMSim::TransactionCompleteCB *write_cb);
private:
	void ReadCompleteCallback(unsigned id, uint64_t address, uint64_t clock_cycle);
	void WriteCompleteCallback(unsigned id, uint64_t address, uint64_t clock_cycle);
	Type WhereisItBelong(uint64_t address);
	DRAMSim::MultiChannelMemorySystem *dram;
	DRAMSim::TransactionCompleteCB *read_cb;
	DRAMSim::TransactionCompleteCB *write_cb;
	BufferInterface *buffer;
};

#endif