#pragma once
#include<iostream>
#include<vector>
#include<time.h>
#include<Windows.h>

using std::cout;
using std::endl;

//定长内存池
template<class T>
class ObjectPool
{
public:
	inline static void* SystemAlloc(size_t kpage)
	{
#ifdef _WIN32
		void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
		// linux下brk mmap等
#endif
		if (ptr == nullptr)
			throw std::bad_alloc();
		return ptr;
	}
	T* New()
	{
		T* obj = nullptr;
		//若有还回来的内存,优先使用它而不是去切割大内存
		if (_freeList != nullptr)
		{
			void* next = *((void**)_freeList);//先把头内存的后面一个内存找到,再把头内存头删了给外界用
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			if (_surplusBytes < sizeof(T))//当剩余的空间不足以分配给T类型时,也要扩容
			{
				//_memory = malloc(128 * 1024);//128KB
				_surplusBytes = 128 * 1024;
				_memory = (char*)SystemAlloc(_surplusBytes >> 13);
				if (_memory == nullptr)
					throw std::bad_alloc();
			}
			obj = (T*)_memory;
			//若此对象的大小小于的指针的大小(4/8),则它释放后无法存储下一个内存的地址,要做特殊处理
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_surplusBytes -= objSize;
		}
		//定位new,显示调用T的构造函数初始化
		new(obj)T;
		return obj;
	}

	void Delete(T* obj)
	{
		//显示调用析构函数
		obj->~T();
		//使用**强转,不管是32位还是64位都没问题
		//取obj对象头四个字节来存储nullptr或下一个被Delete的对象的空间的地址
		//使用头插,不用每次都去找尾
		*(void**)obj = _freeList;
		_freeList = obj;
	}

private:
	char* _memory = nullptr;     //向系统申请的一大块内存,申请了多少字节就从起始地址++多少次,所以定义为char*
	void* _freeList = nullptr;   //还回来的空间的过程中,链接的自由链表的头指针
	size_t _surplusBytes = 0;    //定长池剩余的空间大小(字节为单位)
};
