﻿#pragma once
#include<iostream>
#include<vector>
#include<time.h>
#include<assert.h>
#include<unordered_map>
#include<thread>
#include<mutex>
#include<algorithm>
#ifdef _WIN32
#include<Windows.h>
#else
//linux
#endif

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024; //最大内存限制
static const size_t N_FREE_LIST = 208; //哈希桶的数量
static const size_t N_PAGES = 129; // page cache 管理span list哈希表大小,共128页,ipage对应下标是i
static const size_t PAGE_SHIFT = 13;// 页大小转换偏移(右移13), 即一页定义为2^13,也就是8KB

#ifdef _WIN64 
typedef size_t PAGE_ID;
#else _WIN32
typedef unsigned long long PAGE_ID;
#endif

inline static void* SystemAlloc(size_t kpage)//申请kpage页内存
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}


//管理切分好的小对象的自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		assert(obj);
		//头插
		*(void**)obj = _freeList;
		_freeList = obj;
		_size++;
	}

	void PushRange(void* start, void* end, size_t n)//范围型的插入
	{
		*(void**)end = _freeList;
		_freeList = start;
		_size += n;
	}

	void* Pop()
	{
		assert(_freeList);
		//头删
		void* obj = _freeList;
		_freeList = *(void**)obj;
		--_size;
		return obj;
	}

	void PopRange(void*& start, void*& end, size_t n)//start和end是输出型参数
	{
		assert(n <= _size);
		start = _freeList;
		end = start;
		for (int i = 0; i < n - 1; i++)
			end = *(void**)end;
		_freeList = *(void**)end;
		*(void**)end = nullptr;
		_size -= n;
	}

	bool Empty()
	{
		return _freeList == nullptr;
	}

	size_t& MaxSize()
	{
		return _maxSize;
	}

	size_t Size()
	{
		return _size;
	}

private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;//记录ThreadCache一次性最多向CentralCache申请多少个对象
	size_t _size = 0;//记录自由链表中挂的内存个数
};

//计算对象大小的对齐映射规则
class AlignmentRule
{
	// [1字节,128字节] 8byte对齐 freelist[0,16)  1字节对应1+7=8字节
	// [128+1,1024] 16byte对齐 freelist[16,72)   129要对应129+15=144个字节,以16个字节为单位
	// [1024+1,8*1024] 128byte对齐 freelist[72,128)  1025要对应1025+127=1152个字节,以128个字节为单位
	// [8*1024+1,64*1024] 1024byte对齐 freelist[128,184) 8*1024+1对应8*1024+1+1023个字节
	// [64*1024+1,256*1024] 8*1024byte对齐   freelist[184,208)   ......
public:

	//一次ThreeadCache从CentralCache获取多少个对象
public:

	static inline size_t AlignUp(size_t size)//将size向上对齐
	{
		if (size <= 128)
			return _AlignUp(size, 8);
		else if (size <= 1024)
			return _AlignUp(size, 16);
		else if (size <= 8 * 1024)
			return _AlignUp(size, 128);
		else if (size <= 64 * 1024)
			return _AlignUp(size, 1024);
		else if (size <= 256 * 1024)
			return _AlignUp(size, 8 * 1024);
		else
		{
			return _AlignUp(size, 1 << PAGE_SHIFT);
		}
	}
	static inline size_t _AlignUp(size_t size, size_t align_number)
	{
		return (((size)+align_number - 1) & ~(align_number - 1));
	}

	// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128)
			return _Index(bytes, 3);
		else if (bytes <= 1024)
			return _Index(bytes - 128, 4) + group_array[0];
		else if (bytes <= 8 * 1024)
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		else if (bytes <= 64 * 1024)
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1]
			+ group_array[0];
		else if (bytes <= 256 * 1024)
			return _Index(bytes - 64 * 1024, 13) + group_array[3] +
			group_array[2] + group_array[1] + group_array[0];
		else {
			assert(false);
			return -1;
		}
	}
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	static size_t NumMoveSize(size_t size)//此函数代表从中心缓存取多少个对象(和慢增长对比)
	{
		assert(size > 0);
		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 大对象一次批量上限低
		int num = MAX_BYTES / size; //256KB除单个对象大小
		if (num < 2)//最少给2个
			num = 2;
		if (num > 512)//最大给512个
			num = 512;
		return num;
	}
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	static size_t NumMovePage(size_t size)//向pagecache申请多少页
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size; //总共的字节数
		npage >>= PAGE_SHIFT; //总的字节数右移13位(除2^13,也就是一页8kb的大小)
		if (npage == 0)
			npage = 1;
		return npage;
	}
};


//管理多个连续页的大块内存跨度结构,centralcache的哈希桶中链接的就是这种结构
class SpanData
{
public:
	PAGE_ID _pageid = 0;//32位下,程序地址空间,2^32byte,一页8kb=2^13byte,一共有2^19页
	size_t _n = 0;//页数
	SpanData* _next = nullptr;
	SpanData* _prev = nullptr;
	size_t _useCount = 0;//span中切分好的小对象有几个被使用了
	void* _freeList = nullptr;//切分好的小块内存的自由链表
	bool _isUse = false; //这个span是否正在被使用,若没有被使用则可能被pagecache合并成为大页
	size_t _objSize = 0; //切分好的小对象的大小,方便后面删除的时候可以直接知道它的大小
};

class SpanList//双向带头循环链表
{
public:
	SpanList()
	{
		_head = new SpanData;
		_head->_next = _head;
		_head->_prev = _head;
	}

	SpanData* Begin()
	{
		return _head->_next;
	}

	SpanData* End()
	{
		return _head;
	}

	bool Empty()//判断这个桶中是不是没有span
	{
		return _head->_next == _head;
	}

	void Insert(SpanData* pos, SpanData* newSpan)
	{
		assert(pos && newSpan);
		SpanData* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}

	void Erase(SpanData* pos)
	{
		assert(pos);
		assert(pos != _head);
		/*if (pos == _head)
		{
			int x = 0;
		}*/
		SpanData* prev = pos->_prev;
		SpanData* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}

	void PushFront(SpanData* span)
	{
		Insert(Begin(), span);
	}

	SpanData* PopFront()
	{
		SpanData* front = _head->_next;
		Erase(front);
		return front;//erase中没有将此节点释放
	}

	SpanData* _head = nullptr;
	std::mutex _mtx;//桶锁
};
