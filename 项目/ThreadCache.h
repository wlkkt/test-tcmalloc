#pragma once
#include"Common.h"
#include"PageCache.h"
#include"CentralCache.h"

class ThreadCache
{
public:
	void* Allocate(size_t size);


	void Deallocate(void* ptr, size_t size);

	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);

	// 释放对象时，链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeList[N_FREE_LIST];
};

//TLS无锁技术
static _declspec(thread) ThreadCache* ptc = nullptr;//每一个线程内部都有一个独立的全局对象变量

