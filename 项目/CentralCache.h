#pragma once
#include"Common.h"
#include"PageCache.h"
#include"ThreadCache.h"
//����ģʽ,����ģʽ
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_singleton;
	}
	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t massNum, size_t size);//��n���ڴ����,��С��byte_size,start��end������Ͳ���

	// ��SpanList��ȡһ���ǿյ�span
	SpanData* GetOneSpan(SpanList& list, size_t size);

	// ��ThreadCache���������ڴ����¹���CentralCache��span
	void ReleaseListToSpans(void* start, size_t byte_size);

private:
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
private:
	SpanList _spanlist[N_FREE_LIST];//���Ļ����Ͱӳ������Threadһ��
	static CentralCache _singleton;
};


