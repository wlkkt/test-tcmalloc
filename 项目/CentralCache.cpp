#include"CentralCache.h"
CentralCache CentralCache::_singleton;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t massNum, size_t size)
{
	size_t index = AlignmentRule::Index(size);
	_spanlist[index]._mtx.lock();//加锁
	SpanData* span = GetOneSpan(_spanlist[index], size);//获取哪个桶中的span
	assert(span && span->_freeList);
	//从span中获取massnum个对象,若没有这么多对象的话,有多少就给多少
	start = span->_freeList;//把start指向首地址
	end = start;
	int factcount = 1;
	int i = 0;
	while (*(void**)end != nullptr && i < massNum - 1)
	{
		end = *(void**)end;
		i++;
		factcount++;
	}
	span->_useCount += factcount;
	span->_freeList = *(void**)end;
	*(void**)end = nullptr;
	_spanlist[index]._mtx.unlock();//解锁
	return factcount;
}

SpanData* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	SpanData* it = list.Begin();
	//遍历centralcache的中固定桶的所有span,若找到有不为空的freelist,则直接返回
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
			return it;
		else
			it = it->_next;
	}
	//先把centralcache的桶锁解除,这样如果其他线程释放内存对象回来不会阻塞
	list._mtx.unlock();
	//走到这儿证明这个桶中没有span小对象了,去找pagecache要span
	//直接在这里加锁,Newspan内就不用加锁了
	PageCache::GetInstance()->_mtx.lock();
	SpanData* span = PageCache::GetInstance()->NewSpan(AlignmentRule::NumMovePage(size));//传的参数是要申请的页数,size越大对应的页就应该越大
	span->_isUse = true;//将这个span的状态修改为正在使用
	span->_objSize = size;
	PageCache::GetInstance()->_mtx.unlock();
	//下面的内容不需要加锁,因为获取到的span只有我这个线程有,其他线程访问不到
	char* address = (char*)((span->_pageid) << PAGE_SHIFT); //这个页的起始地址是页号*8*1024,第0页的地址是0,以此类推
	size_t bytes = span->_n << PAGE_SHIFT; //计算这个span总共有多少个字节,用_n(页数)*8*1024
	//接下来要将这个span的大块内存切分成小块内存用自由链表连接起来
	char* end = address + bytes;//address和end对应空间的开头和结尾
	//1. 先切一块下来去做头,方便后续尾插
	span->_freeList = address;
	address += size;
	void* cur = span->_freeList;
	while (address < end)//2. 遍历空间尾插
	{
		*(void**)cur = address;
		cur = *(void**)cur;
		address += size;
	}
	*(void**)cur = nullptr;
	//插入时需要加锁,否则指向可能乱掉
	list._mtx.lock();
	list.PushFront(span);
	return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)//一个桶中有多个span,这些内存块儿属于哪个span是不确定的
{
	size_t index = AlignmentRule::Index(size);//计算在哪个桶
	_spanlist[index]._mtx.lock();
	//要想知道这些内存块分别在哪个span,将内存块的地址除8*1024,得到这个内存块属于第几页
	while (start != nullptr)
	{
		void* next = *(void**)start;
		SpanData* span = PageCache::GetInstance()->MapObjectToSpan(start);//利用地址与span的映射函数,找到这个内存块对应的span
		*(void**)start = span->_freeList;//将内存块start头插到span中
		span->_freeList = start;
		span->_useCount--;//还回来一次内存块,就将usecount--,减到0后就又把内存还给pagecache
		if (span->_useCount == 0)//此时说明span切分出去的所有小块儿内存都被还回来了,直接将整个span还给pagecache,pagecache再进行前后页的合并
		{
			_spanlist[index].Erase(span);
			span->_freeList = nullptr;//span中的小块内存已经打乱了,但我知道起始地址和结束地址,可直接置空
			span->_next = nullptr;
			span->_prev = nullptr;
			//还回来内存时不用加桶锁了,但是pagecache的整体大锁需要加上
			_spanlist[index]._mtx.unlock();
			PageCache::GetInstance()->_mtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_mtx.unlock();
			_spanlist[index]._mtx.lock();
		}
		start = next;
	}
	_spanlist[index]._mtx.unlock();
}

