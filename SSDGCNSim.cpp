/*******************************************************************
* SSD GCN Project
********************************************************************/

#include "SSDGCNSim.h"

using namespace std;

SSDGCNSim::SSDGCNSim(IniParser *iniparser, DataReader *datareader) {
    buffer = new BufferInterface(iniparser->axbuffer,
                                 iniparser->weightbuffer,
                                 iniparser->outputbuffer,
                                 datareader);

  	dram = new DRAMInterface("ini/DDR3_micron_32M_8B_x8_sg15.ini", 
                             "system.ini", 
                             "./DRAMSim2/", 
                             "SSDGCNSim", 
                              16384,
                              iniparser->clk_period_in_ns,
                              buffer);

    acc = new Accelerator(iniparser->accdimension, dram, buffer);
    
    cycle = 0;
  }

SSDGCNSim::~SSDGCNSim() {
  delete buffer;
  delete dram;
  delete acc;
}

void SSDGCNSim::RunSimulator()
{
  cout<<"SSDGCNSim Start..."<<endl;
  while (acc->Run())
  {
    cycle++;
    dram->UpdateCycle();
  }
  cout<<"End... Total Cycle: "<<cycle<<endl;
}
