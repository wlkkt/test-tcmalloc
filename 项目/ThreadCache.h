#pragma once
#include"Common.h"
#include"PageCache.h"
#include"CentralCache.h"

class ThreadCache
{
public:
	void* Allocate(size_t size);


	void Deallocate(void* ptr, size_t size);

	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);

	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeList[N_FREE_LIST];
};

//TLS��������
static _declspec(thread) ThreadCache* ptc = nullptr;//ÿһ���߳��ڲ�����һ��������ȫ�ֶ������

