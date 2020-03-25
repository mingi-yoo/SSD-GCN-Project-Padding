/*******************************************************************
* SSD GCN Project
********************************************************************/

#include "IniParser.h"

using namespace std;

IniParser::IniParser(string path) { ReadIni(path); }

IniParser::~IniParser() {}

bool IniParser::ReadIni(string path)
{
	ifstream openFile(path);
	if (openFile.is_open()) {
		string line;
		while (getline(openFile, line)) {
			string delimiter = " = ";
			if (string::npos == line.find(delimiter)) 
				delimiter = "=";
			string token1 = line.substr(0, line.find(delimiter));
			string token2 = line.substr(line.find(delimiter)+delimiter.length(), line.length());
			m_table[token1] = token2;
		}
		openFile.close();
		GetAXBufferSize("AXBufferSize");
		GetWeightBufferSize("WeightBufferSize");
		GetOutputBufferSize("OutputBufferSize");
		GetRAMQueueSize("RAMQueueSize");
		GetAccDimension("AccDimension");
		GetMemoryBandwidth("MemoryBandwidth");
		GetClockPeriod("ClockPeriod");
		return true;
	}
	else {
		throw invalid_argument("Cannot open inifile");
	}
}

bool IniParser::Contain(string name)
{
	if (m_table.find(name) == m_table.end()) 
		return false;
	return true;
}

bool IniParser::GetBool(string name)
{
	if (Contain(name)) {
		if (m_table[name][0] == 't' || m_table[name][0] == 'T')
			return true;
		else
			return false;
	}
	else {
		throw invalid_argument("Not exist.");
	}
}

string IniParser::GetString(string name)
{
	if (Contain(name)) {
		if (m_table[name].find("\"") == string::npos)
			return m_table[name];
		else
			return m_table[name].substr(1, m_table[name].length() - 2);
	}
	else {
		throw invalid_argument("Not exist.");
	}
}

float IniParser::GetFloat(string name)
{
	if (Contain(name))
		return stof(m_table[name]);
	else
		throw invalid_argument("Not exist.");
}

int IniParser::GetInt(string name)
{
	if (Contain(name))
		return stoi(m_table[name]);
	else
		throw invalid_argument("Not exist.");
}

uint64_t IniParser::GetUint64(string name)
{
	if (Contain(name))
		return stoull(m_table[name]);
	else
		throw invalid_argument("Not exist.");
}

void IniParser::GetAXBufferSize(string name)
{
	axbuffer = GetUint64(name);
}

void IniParser::GetWeightBufferSize(string name)
{
	weightbuffer = GetUint64(name);
}

void IniParser::GetOutputBufferSize(string name)
{
	outputbuffer = GetUint64(name);
}

void IniParser::GetRAMQueueSize(string name)
{
	ramqueue = GetUint64(name);
}

void IniParser::GetAccDimension(string name)
{
	accdimension = GetUint64(name);
}

void IniParser::GetMemoryBandwidth(string name)
{
	membandwidth = GetUint64(name);
}

void IniParser::GetClockPeriod(string name)
{
	clk_period_in_ns = GetFloat(name);
}
