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




void printKV(const CKeyValue* parent, const int tabCount)
{
	if (!parent->hasChild || parent->childCount == 0)
		return;

	int i = 0;
	if (!parent->key)
		i = 0;// parent->childCount - 10;
	for (; i < parent->childCount; i++)
	{
		CKeyValue* kv = &parent->children[i];

		for (int j = 0; j < tabCount; j++) std::cout << '\t';

		if (kv->key)
			std::cout << "\"" << kv->key << "\"";
		if (kv->hasChild)
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

	int parseTotalTime = 0;
	int deleteTotalTime = 0;
	clock_t t;
	int count = 10000;
	std::cout.flush();
	for (int i = 0; i < count; i++)
	{
		t = clock();
		CKeyValueRoot* kv = CKeyValueRoot::Parse(buf, len);
		parseTotalTime += clock() - t;
		
		//printKV(kv, 0);
		
		t = clock();
		delete kv;
		deleteTotalTime += clock() - t;
		std::cout << i << "/" << count << "\n";
	}

	delete[] buf;

	std::cout << "==times taken on " << argv[1] << "==\n";
	std::cout << "Over " << count << " parses, it took an average of\n\n";

	std::cout << (((float)parseTotalTime) / count) << " clock() ticks to parse\n\n";

	std::cout << (((float)deleteTotalTime) / count) << " clock() ticks to delete\n";

}