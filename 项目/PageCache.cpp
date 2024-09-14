#include"PageCache.h"

PageCache PageCache::_singleton;

SpanData* PageCache::NewSpan(size_t k)//去第K个桶中找span给central,此i号桶中挂的span都是i页内存
{
	//若K桶中有,直接返回,K桶没有span就往后找去分裂大span
	assert(k > 0);
	if (k > N_PAGES - 1)//如果申请的页数大于了128页,pagecache只能向堆申请了
	{
		void* ptr = SystemAlloc(k);
		//SpanData* span = new SpanData();
		SpanData* span = _spanPool.New();
		span->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;
		//_idSpanMap[span->_pageid] = span;
		_idSpanMap.set(span->_pageid, span);
		return span;
	}
	//先检查K号桶有无span,有直接返回一个
	if (!_spanList[k].Empty())
	{
		SpanData* KSpan = _spanList[k].PopFront();
		for (PAGE_ID i = 0; i < KSpan->_n; i++)
			//_idSpanMap[KSpan->_pageid + i] = KSpan;
			_idSpanMap.set(KSpan->_pageid + i, KSpan);
		return KSpan;
	}
	//走到这儿代表k号桶为空,检查后面的桶有没有span,拿出来分裂成两个小span
	for (int i = k + 1; i < N_PAGES; i++)
	{
		if (!_spanList[i].Empty())//k页的span返回给centralcache,i-k页的span挂到i-k号桶中
		{
			SpanData* ISpan = _spanList[i].PopFront();
			//SpanData* KSpan = new SpanData;
			SpanData* KSpan = _spanPool.New();
			KSpan->_pageid = ISpan->_pageid;
			KSpan->_n = k;
			ISpan->_pageid += k;//把头K页切分给KSpan
			ISpan->_n -= k; //页数从i变为i-k
			_spanList[ISpan->_n].PushFront(ISpan);//再将后i-k页分配给i-k号桶
			//存储Ispan的首尾页号跟ISpan的映射关系
			// 这里只需要映射首尾页而不需要像下面一样全部页都映射,因为下面切分出去的span会被切分为小块儿内存
			// 这些小块儿内存都有可能被使用,所以当它们还回来时这些小块儿内存可能映射的是不同的页,但这些页都属于这个KSpan
			// 然而ISpan中不会被切分为小块儿内存,它只需要关心是否和它的前后页合并,所以这里只需要映射首尾页号与ISpan的关系
			// ISpan作为要合并页的前面,如1000页要合并ISpan是1001页,那么1001到1001+n都是空闲的!ISpan作为要合并页的后面,如100页要合并ISpan是999页,那么999-n都是空闲的!
			//_idSpanMap[ISpan->_pageid] = ISpan;
			//_idSpanMap[ISpan->_pageid + ISpan->_n - 1] = ISpan;
			_idSpanMap.set(ISpan->_pageid, ISpan);
			_idSpanMap.set(ISpan->_pageid + ISpan->_n - 1, ISpan);
			//建立id和span的映射关系,方便centralcache回收小块内存时查看哪块内存在哪块span
			for (PAGE_ID i = 0; i < KSpan->_n; i++)//返回的KSpan中一共有n页,并且每一页的页号都对应KSpan这个地址
				//_idSpanMap[KSpan->_pageid + i] = KSpan;
				_idSpanMap.set(KSpan->_pageid + i, KSpan);
			return KSpan;
		}
	}
	//走到这里说明后面所有的桶都没有span了
	//这时需要向堆申请一个128页的span再拿来做切分
	SpanData* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(N_PAGES - 1);
	bigSpan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = N_PAGES - 1;
	_spanList[bigSpan->_n].PushFront(bigSpan);//将这个128页的span插入到桶中
	return NewSpan(k);//再次调用自己,这次一定会在前面的for循环处返回
}

//给我一个地址,返回这个地址对应的span
SpanData* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID pageId = (PAGE_ID)obj >> PAGE_SHIFT;//将地址右移13位就算出了页号
	//std::unique_lock<std::mutex> lock(_mtx);
	//auto ret = _idSpanMap.find(pageId);
	//if (ret != _idSpanMap.end())//若找到了对应的页号,就返回对应的span
	//	return _idSpanMap[pageId];
	//else assert(false);
	auto ret = (SpanData*)_idSpanMap.get(pageId);
	assert(ret);
	return ret;

}

void PageCache::ReleaseSpanToPageCache(SpanData* span)
{
	if (span->_n > N_PAGES - 1)//大于128页的内存直接还给堆,不需要走pagecache
	{
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}
	//对span前后的页尝试进行合并,缓解外碎片问题
	while (1)//不断往前合并,直到遇见不能合并的情况
	{
		PAGE_ID prevId = span->_pageid - 1;
		//auto prevret = _idSpanMap.find(prevId);
		//if (prevret == _idSpanMap.end())//前面没有页号了
		//	break;
		auto ret = (SpanData*)_idSpanMap.get(prevId);
		SpanData* prevspan = ret;
		if (ret == nullptr)
			break;
		if (prevspan->_isUse == true)//前面的页正在使用
			break;
		if (prevspan->_n + span->_n > N_PAGES - 1)//当前页数加上span的页数大于128了,pagecache挂不下了
			break;
		//开始合并span和span的前面页
		span->_pageid = prevspan->_pageid;
		span->_n += prevspan->_n;
		_spanList[prevspan->_n].Erase(prevspan);//将被合并的页从pagecache中拿下来
		//delete prevspan;//将prevspan中的数据清除,诸如页号,页数等
		_spanPool.Delete(prevspan);
	}
	while (1)//不断往后合并,直到遇见不能合并的情况
	{
		PAGE_ID nextId = span->_pageid + span->_n;
		//auto nextret = _idSpanMap.find(nextId);
		//if (nextret == _idSpanMap.end())//前面没有页号了
		//	break;
		//SpanData* nextspan = nextret->second;
		auto ret = (SpanData*)_idSpanMap.get(nextId);
		if (ret == nullptr)
			break;
		SpanData* nextspan = ret;
		if (nextspan->_isUse == true)//前面的页正在使用
			break;
		if (nextspan->_n + span->_n > N_PAGES - 1)//当前页数加上span的页数大于128了,pagecache挂不下了
			break;
		//开始合并span和span的前面页
		span->_n += nextspan->_n;
		_spanList[nextspan->_n].Erase(nextspan);//将被合并的页从pagecache中拿下来
		//delete nextspan;//将prevspan中的数据清除,诸如页号,页数等
		_spanPool.Delete(nextspan);
	}
	//合并完后将span挂起来
	_spanList[span->_n].PushFront(span);
	//合并完后,要重新将这个span的首尾两页的id和这个span进行映射,方便别的span来合并我的时候使用
	//_idSpanMap[span->_pageid] = span;
	//_idSpanMap[span->_pageid + span->_n - 1] = span;
	_idSpanMap.set(span->_pageid, span);
	_idSpanMap.set(span->_pageid + span->_n - 1, span);
	span->_isUse = false;
}

