#pragma once
#include"Common.h"
#include"CentralCache.h"
#include"ThreadCache.h"
#include"ObjectPool.h"
#include"PageMap.h"

//单例模式
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_singleton;
	}
	//获取一个K页的span
	SpanData* NewSpan(size_t k);
	std::mutex _mtx;//pagecache不能用桶锁,只能用全局锁,因为后面可能会有大页被切割为小页

	// 获取从对象到span的映射,给我一个地址,返回这个地址对应的span
	SpanData* MapObjectToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(SpanData* span);
private:
	PageCache() {}
	PageCache(const PageCache& obj) = delete;
private:
	//std::unordered_map<PAGE_ID, SpanData*> _idSpanMap;//存储页号和桶中对应的span的映射,解决换回来的内存对应哪个span的问题
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;
	SpanList _spanList[N_PAGES];
	static PageCache _singleton;
	ObjectPool<SpanData> _spanPool;
};
