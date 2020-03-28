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
#include "Common.h"

using namespace std;

class IniParser {
public:
	IniParser(string path);
	~IniParser();
	uint64_t axbuffer;
	uint64_t weightbuffer;
	uint64_t outputbuffer;
	uint64_t accdimension;
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
	void ParseIni();
	bool ReadIni(string path);
};

#endif