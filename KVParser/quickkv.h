#pragma once

#include <stddef.h>




class CKeyValue
{
public:

	char* key;
	size_t keyLength;

	union
	{
		char* value;
		CKeyValue* children;
	};

	union
	{
		size_t childCount;
		size_t valueLength;
	};

	bool hasChild;

	CKeyValue* parent;
};

class CKeyValueRoot : public CKeyValue
{
public:
	static CKeyValueRoot* Parse(char* string, int length);

	~CKeyValueRoot();

private:
	
	char* memoryBlock;
	CKeyValue* kvBlock;

};



