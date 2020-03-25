/*******************************************************************
* SSD GCN Project
********************************************************************/

#ifndef __INIPARSER_H__
#define __INIPARSER_H__

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <stdexcept>

using namespace std;

class IniParser {
public:
	IniParser(string path);
	~IniParser();
	uint64_t axbuffer;
	uint64_t weightbuffer;
	uint64_t outputbuffer;
	uint64_t ramqueue;
	uint64_t accdimension;
	uint64_t membandwidth; 
	float clk_period_in_ns;
	string outputfilename;

private:
	map<string, string> m_table;
	bool Contain(string name);
	bool GetBool(string name);
	string GetString(string name);
	float GetFloat(string name);
	int GetInt(string name);
	uint64_t GetUint64(string name);
	void GetAXBufferSize(string name);
	void GetWeightBufferSize(string name);
	void GetOutputBufferSize(string name);
	void GetRAMQueueSize(string name);
	void GetAccDimension(string name);
	void GetMemoryBandwidth(string name);
	void GetClockPeriod(string name);
	bool ReadIni(string path);
};

#endif