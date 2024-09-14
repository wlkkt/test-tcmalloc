#pragma once
#include<iostream>
#include<vector>
#include<time.h>
#include<Windows.h>

using std::cout;
using std::endl;

//�����ڴ��
template<class T>
class ObjectPool
{
public:
	inline static void* SystemAlloc(size_t kpage)
	{
#ifdef _WIN32
		void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
		// linux��brk mmap��
#endif
		if (ptr == nullptr)
			throw std::bad_alloc();
		return ptr;
	}
	T* New()
	{
		T* obj = nullptr;
		//���л��������ڴ�,����ʹ����������ȥ�и���ڴ�
		if (_freeList != nullptr)
		{
			void* next = *((void**)_freeList);//�Ȱ�ͷ�ڴ�ĺ���һ���ڴ��ҵ�,�ٰ�ͷ�ڴ�ͷɾ�˸������
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			if (_surplusBytes < sizeof(T))//��ʣ��Ŀռ䲻���Է����T����ʱ,ҲҪ����
			{
				//_memory = malloc(128 * 1024);//128KB
				_surplusBytes = 128 * 1024;
				_memory = (char*)SystemAlloc(_surplusBytes >> 13);
				if (_memory == nullptr)
					throw std::bad_alloc();
			}
			obj = (T*)_memory;
			//���˶���Ĵ�СС�ڵ�ָ��Ĵ�С(4/8),�����ͷź��޷��洢��һ���ڴ�ĵ�ַ,Ҫ�����⴦��
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_surplusBytes -= objSize;
		}
		//��λnew,��ʾ����T�Ĺ��캯����ʼ��
		new(obj)T;
		return obj;
	}

	void Delete(T* obj)
	{
		//��ʾ������������
		obj->~T();
		//ʹ��**ǿת,������32λ����64λ��û����
		//ȡobj����ͷ�ĸ��ֽ����洢nullptr����һ����Delete�Ķ���Ŀռ�ĵ�ַ
		//ʹ��ͷ��,����ÿ�ζ�ȥ��β
		*(void**)obj = _freeList;
		_freeList = obj;
	}

private:
	char* _memory = nullptr;     //��ϵͳ�����һ����ڴ�,�����˶����ֽھʹ���ʼ��ַ++���ٴ�,���Զ���Ϊchar*
	void* _freeList = nullptr;   //�������Ŀռ�Ĺ�����,���ӵ����������ͷָ��
	size_t _surplusBytes = 0;    //������ʣ��Ŀռ��С(�ֽ�Ϊ��λ)
};
