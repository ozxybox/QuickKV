#ifndef QUICKKV_H
#define QUICKKV_H
#ifdef WIN32
#pragma once
#endif


enum class ParseStatus
{
	GOOD = 0,
	BROKEN,
	HIT_END_BLOCK_ON_ROOT,
	HIT_END_LINE_IN_STRING,
	HIT_SUBKEY_IN_KEY,
	HIT_UNEXPECTED_END_BLOCK,

};


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


#endif

