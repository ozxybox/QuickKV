#include "quickkv.h"
#include <stdlib.h>
#include <string.h>

//you might notice some strange things in this file...
//that is because, in a mad dash for speed, I created it
//to use no recursion and I strangled any code that
//vs claimed to be a hotspot


#define POOL_MAX_COUNT_INCREMENT 16



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
	CKeyValueMemoryPool* currentPool;
	//tally up the bytes and kvs used so that we can allocate a mega block later
	size_t bytesUsed;
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



void BuildData(kvObject_t* currentKV, char* lastPointer)
{
startOfFunction:
	currentKV->lastChild = currentKV->child;

startOfLoop:
	for (kvObject_t* kvObj = currentKV->lastChild; kvObj ; kvObj = kvObj->next)
	{
		
		//copy the the key from the string to the memory block and assign the key to the address in the block
		memcpy(lastPointer, kvObj->key, kvObj->keyLength);
		lastPointer[kvObj->keyLength] = 0;

		kvObj->key = lastPointer;

		lastPointer += kvObj->keyLength + 1;

		if (kvObj->hasChildren)
		{

			//the child count is empty so let's take a shortcut
			if (kvObj->childCount == 0)
			{
				continue;
			}


			//lower the arguments by one level and jump to the top
			//cheap recursion lol
			currentKV->lastChild = kvObj->next;
			currentKV = kvObj;


			goto startOfFunction;
		}
		else
		{

			//copy the the value from the string to the memory block and assign the value to the address in the block
			memcpy(lastPointer, kvObj->value, kvObj->valueLength);
			lastPointer[kvObj->valueLength] = 0;

			kvObj->value = lastPointer;

			lastPointer += kvObj->valueLength + 1;

		}
	}


	if (currentKV->key)
	{
		//restore i, parent, and curKV
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
		for (char c = string[info.pos]; info.pos < info.length && (c < '!' || c > '~'); c = string[++info.pos]);

		//TODO error handling
		if (info.pos >= info.length)
			continue;

		switch (string[info.pos])
		{
		case '"':
			//we're in a string
			info.pos++;

			start = info.pos;

			for (; info.pos < info.length && string[info.pos] != '"'; info.pos++);

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
			}
			else
			{

				currentKv = info.NewKVObject();
				if (lastKv)
					lastKv->next = currentKv;
				else
					parent->child = currentKv;


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
				//info.status = ParseStatus::HIT_SUBKEY_IN_KEY;
				return;
			}

			continue;
		case '}':

			if (parent->key)
			{
				if(lastKv)
					lastKv->next = nullptr;
				lastKv = parent;
				parent = lastKv->parent;
				parent->childCount++;
			}
			else
			{
				//info.status = ParseStatus::HIT_END_BLOCK_ON_ROOT;
				return;
			}

			continue;

		case '/':

			if (string[++info.pos])
				for (info.pos++; info.pos < info.length && string[info.pos] != '\n'; info.pos++);

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
		}
		else
		{
			currentKv = info.NewKVObject();
			if (lastKv)
				lastKv->next = currentKv; 
			else
				parent->child = currentKv;

			//set the key
			currentKv->key = string + start;
			currentKv->keyLength = length;
		}

		info.bytesUsed += length + 1;
		
	}
	if(lastKv)
		lastKv->next = nullptr;
	return;
}



CKeyValueRoot* CKeyValueRoot::Parse(char* string, int length)
{

	CKeyValueRoot* kvr = new CKeyValueRoot();

	//convient way to share variables
	CKeyValueInfo info{ 0,length,0,0 };

	
	info.currentPool = new CKeyValueMemoryPool(POOL_MAX_COUNT_INCREMENT, 0);

	kvObject_t parent{ 0,0};
	parent.hasChildren = true;
	ParseString(string, info, &parent);

	kvr->currentPool = info.currentPool;
	
	//we've counted up all of our objects that need allocation
	//allocate them now
	kvr->memoryBlock = static_cast<char*>(malloc(info.bytesUsed));


	BuildData(&parent, kvr->memoryBlock);

	kvr->rootKV = parent;

	return kvr;
}


CKeyValueRoot::~CKeyValueRoot()
{
	free(memoryBlock);
	CKeyValueMemoryPool::Delete(currentPool);
}