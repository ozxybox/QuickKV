#include "quickkv.h"
#include <stdlib.h>
#include <string.h>

//you might notice some strange things in this file...
//that is because, in a mad dash for speed, I created it
//to use no recursion and I strangled any code that
//vs claimed to be a hotspot


#define POOL_MAX_COUNT_INCREMENT 16

struct kvObject_t
{
	const char* key;
	int keyLength;

	union
	{
		kvObject_t* child;
		const char* value;
	};

	union
	{
		int childCount;
		int valueLength;
	};

	bool hasChildren;

	kvObject_t* parent;
	kvObject_t* previous;
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

inline CKeyValueMemoryPool::CKeyValueMemoryPool(int size, CKeyValueMemoryPool* last)
{
	maxObjectsInPool = size;
	memoryPool = static_cast<kvObject_t*>(malloc(size * sizeof(kvObject_t)));
	lastPool = last;
	objectCount = 0;
}

void CKeyValueMemoryPool::Delete(CKeyValueMemoryPool* current)
{
	CKeyValueMemoryPool* temp;
	int i = 0;
	for (; current->lastPool; i++)
	{
		temp = current;
		current = temp->lastPool;
		free(temp->memoryPool);
		delete temp;
	}
	free(current->memoryPool);
	delete current;
}


class CKeyValueInfo
{
public:
	kvObject_t* NewKVObject();

	int pos;
	const int length;
	int lineNumber;
	int rowStart;
	ParseStatus status;
	CKeyValueMemoryPool* currentPool;
	//tally up the bytes and kvs used so that we can allocate a mega block later
	size_t bytesUsed;
	size_t keyvaluesUsed;
};

kvObject_t* CKeyValueInfo::NewKVObject()
{
startOfFunction:
	if (currentPool->objectCount < currentPool->maxObjectsInPool)
	{
		kvObject_t* obj = currentPool->memoryPool + currentPool->objectCount;
		currentPool->objectCount++;
		obj->key = 0;
		obj->value = 0;
		return obj;
	}
	//We've maxed out our pool. Let's allocate a new one
	currentPool = new CKeyValueMemoryPool(currentPool->maxObjectsInPool + POOL_MAX_COUNT_INCREMENT, currentPool);
	goto startOfFunction;
}



void BuildData(CKeyValue* parent, kvObject_t* currentKV, char* lastPointer, CKeyValue* lastKeyValueBlock)
{
startOfFunction:
	//allocate the correct ammount of data for the children
	if (currentKV->childCount)
	{
		parent->children = lastKeyValueBlock;
		lastKeyValueBlock += currentKV->childCount;
	}
	parent->hasChild = true;

	parent->childCount = currentKV->childCount;

startOfLoop:
	size_t& i = parent->childCount;
	for (kvObject_t* kvObj = currentKV->child; i--; kvObj = kvObj->previous)
	{
		CKeyValue* currentKid = &parent->children[i];

		//copy the the key from the string to the memory block and assign the key to the address in the block
		memcpy(lastPointer, kvObj->key, kvObj->keyLength);
		lastPointer[kvObj->keyLength] = 0;

		currentKid->key = lastPointer;
		currentKid->keyLength = kvObj->keyLength;
		lastPointer += kvObj->keyLength + 1;

		if (kvObj->hasChildren)
		{
			//set the parent so we can restore it later
			currentKid->parent = parent;


			//the child count is empty so let's take a shortcut
			if (kvObj->childCount == 0)
			{
				currentKid->hasChild = true;
				currentKid->childCount = 0;
				continue;
			}


			//lower the arguments by one level and jump to the top
			//cheap recursion lol
			parent = currentKid;
			currentKV->child = kvObj->previous;
			currentKV = kvObj;


			goto startOfFunction;
		}
		else
		{

			//copy the the value from the string to the memory block and assign the value to the address in the block
			memcpy(lastPointer, kvObj->value, kvObj->valueLength);
			lastPointer[kvObj->valueLength] = 0;

			currentKid->value = lastPointer;
			currentKid->valueLength = kvObj->valueLength;

			lastPointer += kvObj->valueLength + 1;

			currentKid->hasChild = false;
		}
	}

	parent->childCount = currentKV->childCount;

	if (parent->key)
	{
		//restore i, parent, and curKV
		parent = parent->parent;
		currentKV = currentKV->parent;
		//attempt to return back to where we were
		goto startOfLoop;
	}
}

//checks if the char isn't a " } { or whitespace
inline bool IsValidQuoteless(char c)
{
	return ( c >= '!' && c <= '~' ) && c != '"' && c != '{' && c != '}';
}

void ParseString(const char* string, CKeyValueInfo& info, kvObject_t* parent)
{
startOfFunction:
	kvObject_t* currentKv = 0;
	kvObject_t* lastKv = 0;
	int start = 0, length = 0;
	for (; info.pos < info.length; info.pos++)
	{

		//skip over all whitespace
		for (char c = string[info.pos]; info.pos < info.length && (c < '!' || c > '~'); c = string[++info.pos])
		{
			if (c == '\n')
			{
				info.lineNumber++;
				info.rowStart = info.pos;
			}
		}

		//TODO error handling
		if (info.pos >= info.length)
			continue;

		switch (string[info.pos])
		{
		case '"':
			//we're in a string
			info.pos++;

			start = info.pos;

		startQuoteLoop:
			switch (string[info.pos])
			{
			case '"':
				if (string[info.pos - 1] == '\\')
				{
					info.pos++;
					goto startQuoteLoop;
				}
				break;
			case '\n':
				info.status = ParseStatus::HIT_END_LINE_IN_STRING;
				return;
			default:
				info.pos++;
				goto startQuoteLoop;
			}

			/*
			for (char c = string[info.pos]; info.pos < info.length && !((c == '"') && (c != '\\')); c = string[++info.pos])
			{
				if (c == '\n')
				{
					info.status = ParseStatus::HIT_END_LINE_IN_STRING;
					return;
				}
			}

			*/
			length = info.pos - start;

			if (currentKv && currentKv->key)
			{
				//set the value
				currentKv->value = string + start;
				currentKv->valueLength = length;

				currentKv->hasChildren = false;
				parent->childCount++;
				lastKv = currentKv;
				currentKv = 0;
				info.keyvaluesUsed++;
			}
			else
			{
				currentKv = info.NewKVObject();
				currentKv->previous = lastKv;
				//set the key
				currentKv->key = string + start;
				currentKv->keyLength = length;
			}

			info.bytesUsed += length + 1;

			continue;

		case '{':

			if (currentKv && currentKv->key)
			{
				info.pos++;
				currentKv->childCount = 0;
				currentKv->hasChildren = true;
				currentKv->parent = parent;
				parent = currentKv;
				goto startOfFunction;

			}
			else
			{
				info.status = ParseStatus::HIT_SUBKEY_IN_KEY;
				return;
			}

			continue;
		case '}':

			if (parent->key)
			{
				parent->child = lastKv;
				lastKv = parent;
				parent = lastKv->parent;
				parent->childCount++;
				info.keyvaluesUsed++;
			}
			else
			{
				info.status = ParseStatus::HIT_END_BLOCK_ON_ROOT;
				return;
			}

			continue;

		case '/':

			info.pos++;
			switch (string[info.pos])
			{
			case '/':

				//skip until endline and breakout 
				for (; info.pos < info.length && string[info.pos] != '\n'; info.pos++);

				continue;
			case '*':

				//skip until end of multiline commment
				for (; info.pos < info.length && !(string[info.pos] == '/' && string[info.pos - 1] == '*'); info.pos++);
				continue;
			}
		}

		//WOW we hit something that wasn't a space and wasn't and of our specific symbols
		//it HAS TO BE a quoteless string

		int start = info.pos;
		info.pos++;
		for (; info.pos < info.length && IsValidQuoteless(string[info.pos]); info.pos++);
		int length = info.pos - start;

		if (currentKv && currentKv->key)
		{
			//set the value
			currentKv->value = string + start;
			currentKv->valueLength = length;

			currentKv->hasChildren = false;

			parent->childCount++;

			lastKv = currentKv;
			currentKv = 0;
			info.keyvaluesUsed++;
		}
		else
		{
			currentKv = info.NewKVObject();
			currentKv->previous = lastKv;
			//set the key
			currentKv->key = string + start;
			currentKv->keyLength = length;
		}

		info.bytesUsed += length + 1;
		
	}
	parent->child = lastKv;
	return;
}



CKeyValueRoot* CKeyValueRoot::Parse(char* string, int length)
{
	//convient way to share variables
	CKeyValueInfo info{ 0,length,0,0,ParseStatus::GOOD };

	info.currentPool = new CKeyValueMemoryPool(POOL_MAX_COUNT_INCREMENT, 0);

	kvObject_t parent{ 0,0 };
	ParseString(string, info, &parent);

	//return the status somehow?
	if (info.status != ParseStatus::GOOD)
	{

		CKeyValueMemoryPool::Delete(info.currentPool);

		return nullptr;
	}

	//we've counted up all of our objects that need allocation
	//allocate them now
	char* memoryBlock = static_cast<char*>(malloc(info.bytesUsed));
	CKeyValue* kvBlock = static_cast<CKeyValue*>(malloc(info.keyvaluesUsed * sizeof(CKeyValue)));

	CKeyValueRoot* kv = new CKeyValueRoot();
	kv->key = 0;
	kv->memoryBlock = memoryBlock;
	kv->kvBlock = kvBlock;

	BuildData(kv, &parent, memoryBlock, kvBlock);

	CKeyValueMemoryPool::Delete(info.currentPool);


	return kv;
}


CKeyValueRoot::~CKeyValueRoot()
{

	free(memoryBlock);
	free(kvBlock);
}