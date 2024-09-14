#include"ThreadCache.h"
#include"CentralCache.h"
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t align_size = AlignmentRule::AlignUp(size);//计算对齐数
	size_t index = AlignmentRule::Index(size);//计算在哪个桶
	if (!_freeList[index].Empty())//若当前自由链表有资源则优先拿释放掉的资源
		return _freeList[index].Pop();
	else//自由链表没有就从中心缓存获取空间
		return ThreadCache::FetchFromCentralCache(index, align_size);
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//采用慢开始的反馈调节算法,小对象给多一点,大对象给少一点
	size_t massNum = min(_freeList[index].MaxSize(), AlignmentRule::NumMoveSize(size));//向中心缓存获取多少个对象
	if (_freeList[index].MaxSize() == massNum)//慢增长,最开始一定是给1,不会一次性给它很多内存,若不断有size大小的内存需求,再慢慢多给你
		_freeList[index].MaxSize() += 1;
	void* begin = nullptr;
	void* end = nullptr;
	//需要massnum个对象,但是实际上不一定有这么多个,返回值为实际上获取到的个数
	size_t fact = CentralCache::GetInstance()->CentralCache::FetchRangeObj(begin, end, massNum, size);//要massmun个对象,每个对象的大小是size
	assert(fact != 0);
	if (fact == 1)
	{
		assert(begin == end);
		return begin;
	}
	else
	{
		//如果从中心缓存获取了多个,则将第一个返回给threadcachee,然后再将剩余的内存挂在threadcache的自由链表中
		_freeList[index].PushRange((*(void**)begin), end, fact - 1);
		return begin;
	}
	return nullptr;
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);
	//找到对应的自由链表桶[[0,208]
	size_t index = AlignmentRule::Index(size);
	_freeList[index].Push(ptr);
	//当换回来的内存在自由链表中的长度大于一次性向中心缓存申请的长度,就将内存还给中心缓存
	if (_freeList[index].Size() >= _freeList[index].MaxSize())
		ListTooLong(_freeList[index], size);
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)//大于等于一个批量内存就还回去一个批量,不一定全部还回去
{
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());//start和end是输出型参数
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}


