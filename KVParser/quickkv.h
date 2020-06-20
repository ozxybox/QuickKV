#pragma once

#include <stddef.h>


struct kvObject_t
{
	char* key;
	int keyLength;

	union
	{
		kvObject_t* child;
		char* value;
	};

	union
	{
		int childCount;
		int valueLength;
	};

	kvObject_t* lastChild;
	bool hasChildren;

	kvObject_t* parent;
	kvObject_t* next = 0;

};


class CKeyValueMemoryPool
{
public:
	inline CKeyValueMemoryPool(int size, CKeyValueMemoryPool* last);
	static void Delete(CKeyValueMemoryPool* last);

	int maxObjectsInPool;
	int objectCount;
	kvObject_t* memoryPool;
	CKeyValueMemoryPool* lastPool;
};

class CKeyValueRoot
{
public:
	static CKeyValueRoot* Parse(char* string, int length);

	~CKeyValueRoot();

	kvObject_t rootKV;
private:
	
	CKeyValueMemoryPool* currentPool;

};



