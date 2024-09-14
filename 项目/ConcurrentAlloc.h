#pragma once

#include"Common.h"
#include"ThreadCache.h"
#include"ObjectPool.h"

static void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)//����256kʱ
	{
		size_t alignsize = AlignmentRule::AlignUp(size);//��256k����
		size_t kpage = alignsize >> PAGE_SHIFT;//�õ���Ҫ���뵽��ҳ��
		PageCache::GetInstance()->_mtx.lock();
		SpanData* span = PageCache::GetInstance()->NewSpan(kpage);//�п�����32ҳ��128ҳ֮��,Ҳ�п��ܴ���128ҳ,����128ҳ��NewSpan��Ҫ�����⴦��
		span->_objSize = size;
		PageCache::GetInstance()->_mtx.unlock();
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);//��ַ����ҳ������13λ(*8K)
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

static void ConcurrentFree(void* ptr)//Ҫ�����ͷ��ڴ�ʱ��freeһ�����ô���С��ȥ,
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

