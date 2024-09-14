#pragma once
#include"Common.h"
#include"CentralCache.h"
#include"ThreadCache.h"
#include"ObjectPool.h"
#include"PageMap.h"

//����ģʽ
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_singleton;
	}
	//��ȡһ��Kҳ��span
	SpanData* NewSpan(size_t k);
	std::mutex _mtx;//pagecache������Ͱ��,ֻ����ȫ����,��Ϊ������ܻ��д�ҳ���и�ΪСҳ

	// ��ȡ�Ӷ���span��ӳ��,����һ����ַ,���������ַ��Ӧ��span
	SpanData* MapObjectToSpan(void* obj);

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(SpanData* span);
private:
	PageCache() {}
	PageCache(const PageCache& obj) = delete;
private:
	//std::unordered_map<PAGE_ID, SpanData*> _idSpanMap;//�洢ҳ�ź�Ͱ�ж�Ӧ��span��ӳ��,������������ڴ��Ӧ�ĸ�span������
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;
	SpanList _spanList[N_PAGES];
	static PageCache _singleton;
	ObjectPool<SpanData> _spanPool;
};
