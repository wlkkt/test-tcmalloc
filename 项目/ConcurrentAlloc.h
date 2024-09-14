#pragma once

#include"Common.h"
#include"ThreadCache.h"
#include"ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)//大于256k时
	{
		size_t alignsize = AlignmentRule::AlignUp(size);//以256k对齐
		size_t kpage = alignsize >> PAGE_SHIFT;//拿到想要申请到的页数
		PageCache::GetInstance()->_mtx.lock();
		SpanData* span = PageCache::GetInstance()->NewSpan(kpage);//有可能在32页到128页之间,也有可能大于128页,大于128页在NewSpan中要做特殊处理
		span->_objSize = size;
		PageCache::GetInstance()->_mtx.unlock();
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);//地址就是页号左移13位(*8K)
		return ptr;
	}
	else
	{
		if (ptc == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;
			ptc = tcPool.New();
		}
		return ptc->Allocate(size);
	}

}

static void ConcurrentFree(void* ptr)//要想在释放内存时与free一样不用传大小进去,
{
	SpanData* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if (size > MAX_BYTES)
	{
		PageCache::GetInstance()->_mtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_mtx.unlock();
	}
	else ptc->Deallocate(ptr, size);
}

