// KVParser.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cstring>
#include <time.h>
#include "quickkv.h"


char* ReadFile(const char* path, int& len)
{

	FILE* f;
	fopen_s(&f, path, "rb");
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	rewind(f);
	char* str = (char*)malloc(len + 1);
	str[len] = 0;
	fread(str, len, 1, f);
	fclose(f);

	return str;
}




void printKV(const kvObject_t* parent, const int tabCount)
{
	if (!parent->hasChildren || parent->childCount == 0)
		return;

	int i = 0;
	if (!parent->key)
		i = 0;// parent->childCount - 10;
	
	
	for (kvObject_t* kv = parent->child; kv; kv = kv->next)
	{
		
		for (int j = 0; j < tabCount; j++) std::cout << '\t';

		if (kv->key)
			std::cout << "\"" << kv->key << "\"";
		if (kv->hasChildren)
		{
			std::cout << "\n";
			for (int j = 0; j < tabCount; j++) std::cout << '\t';
			std::cout << "{\n";
			if(kv->childCount)
				printKV(kv, tabCount + 1);

			for (int j = 0; j < tabCount; j++) std::cout << '\t';
			std::cout << "}\n";
		}
		else
		{
			if (kv->value)
				std::cout << " \"" << kv->value << "\"\n";
		}

		
	}
}

int main(int argc, char* argv[])
{
	int len = 0;
	char* buf = ReadFile(argv[1], len);


	int bestParseTime = INT32_MAX;
	int worstParseTime = INT32_MIN;
	int bestDeleteTime = INT32_MAX;
	int worstDeleteTime = INT32_MIN;


	int parseTotalTime = 0;
	int deleteTotalTime = 0;
	clock_t t;
	int count = 1000;
	std::cout.flush();
	int time;
	for (int i = 0; i < count; i++)
	{

		char* input = new char[len];
		memcpy(input, buf, len);

		t = clock();
		CKeyValueRoot* kvr = CKeyValueRoot::Parse(input, len);
		time = clock() - t;
		parseTotalTime += time;
		if (time < bestParseTime)
			bestParseTime = time;
		else if (time > worstParseTime)
			worstParseTime = time;
		
		//printKV(&kvr->rootKV, 0);
		
		t = clock();
		delete kvr;
		time = clock() - t;

		deleteTotalTime += time;
		if (time < bestDeleteTime)
			bestDeleteTime = time;
		else if (time > worstDeleteTime)
			worstDeleteTime = time;
		
		delete[] input;

		std::cout << i << "/" << count << "\n";
	}

	delete[] buf;

	std::cout << "==times taken on " << count << " parses of " << argv[1] << "==\n";
	std::cout << "\n";
	std::cout << "Parsing\n";
	std::cout << "\tBest:" << bestParseTime << " clock() ticks\n";
	std::cout << "\tWorst:" << worstParseTime << " clock() ticks\n";
	std::cout << "\tAverage:" << (((float)parseTotalTime) / count) << " clock() ticks\n";
	std::cout << "\n\n";
	std::cout << "Deleting\n";
	std::cout << "\tBest:" << bestDeleteTime << " clock() ticks\n";
	std::cout << "\tWorst:" << worstDeleteTime << " clock() ticks\n";
	std::cout << "\tAverage:" << (((float)deleteTotalTime) / count) << " clock() ticks\n";

}