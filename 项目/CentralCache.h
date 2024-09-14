#pragma once
#include"Common.h"
#include"PageCache.h"
#include"ThreadCache.h"
//单例模式,饿汉模式
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_singleton;
	}
	// 从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t massNum, size_t size);//拿n个内存对象,大小是byte_size,start和end是输出型参数

	// 从SpanList获取一个非空的span
	SpanData* GetOneSpan(SpanList& list, size_t size);

	// 将ThreadCache返回来的内存重新挂在CentralCache的span
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
private:
	SpanList _spanlist[N_FREE_LIST];//中心缓存的桶映射规则和Thread一样
	static CentralCache _singleton;
};


