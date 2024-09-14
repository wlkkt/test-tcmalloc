#include"CentralCache.h"
CentralCache CentralCache::_singleton;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t massNum, size_t size)
{
	size_t index = AlignmentRule::Index(size);
	_spanlist[index]._mtx.lock();//����
	SpanData* span = GetOneSpan(_spanlist[index], size);//��ȡ�ĸ�Ͱ�е�span
	assert(span && span->_freeList);
	//��span�л�ȡmassnum������,��û����ô�����Ļ�,�ж��پ͸�����
	start = span->_freeList;//��startָ���׵�ַ
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
	_spanlist[index]._mtx.unlock();//����
	return factcount;
}

SpanData* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	SpanData* it = list.Begin();
	//����centralcache���й̶�Ͱ������span,���ҵ��в�Ϊ�յ�freelist,��ֱ�ӷ���
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
			return it;
		else
			it = it->_next;
	}
	//�Ȱ�centralcache��Ͱ�����,������������߳��ͷ��ڴ���������������
	list._mtx.unlock();
	//�ߵ����֤�����Ͱ��û��spanС������,ȥ��pagecacheҪspan
	//ֱ�����������,Newspan�ھͲ��ü�����
	PageCache::GetInstance()->_mtx.lock();
	SpanData* span = PageCache::GetInstance()->NewSpan(AlignmentRule::NumMovePage(size));//���Ĳ�����Ҫ�����ҳ��,sizeԽ���Ӧ��ҳ��Ӧ��Խ��
	span->_isUse = true;//�����span��״̬�޸�Ϊ����ʹ��
	span->_objSize = size;
	PageCache::GetInstance()->_mtx.unlock();
	//��������ݲ���Ҫ����,��Ϊ��ȡ����spanֻ��������߳���,�����̷߳��ʲ���
	char* address = (char*)((span->_pageid) << PAGE_SHIFT); //���ҳ����ʼ��ַ��ҳ��*8*1024,��0ҳ�ĵ�ַ��0,�Դ�����
	size_t bytes = span->_n << PAGE_SHIFT; //�������span�ܹ��ж��ٸ��ֽ�,��_n(ҳ��)*8*1024
	//������Ҫ�����span�Ĵ���ڴ��зֳ�С���ڴ�������������������
	char* end = address + bytes;//address��end��Ӧ�ռ�Ŀ�ͷ�ͽ�β
	//1. ����һ������ȥ��ͷ,�������β��
	span->_freeList = address;
	address += size;
	void* cur = span->_freeList;
	while (address < end)//2. �����ռ�β��
	{
		*(void**)cur = address;
		cur = *(void**)cur;
		address += size;
	}
	*(void**)cur = nullptr;
	//����ʱ��Ҫ����,����ָ������ҵ�
	list._mtx.lock();
	list.PushFront(span);
	return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)//һ��Ͱ���ж��span,��Щ�ڴ��������ĸ�span�ǲ�ȷ����
{
	size_t index = AlignmentRule::Index(size);//�������ĸ�Ͱ
	_spanlist[index]._mtx.lock();
	//Ҫ��֪����Щ�ڴ��ֱ����ĸ�span,���ڴ��ĵ�ַ��8*1024,�õ�����ڴ�����ڵڼ�ҳ
	while (start != nullptr)
	{
		void* next = *(void**)start;
		SpanData* span = PageCache::GetInstance()->MapObjectToSpan(start);//���õ�ַ��span��ӳ�亯��,�ҵ�����ڴ���Ӧ��span
		*(void**)start = span->_freeList;//���ڴ��startͷ�嵽span��
		span->_freeList = start;
		span->_useCount--;//������һ���ڴ��,�ͽ�usecount--,����0����ְ��ڴ滹��pagecache
		if (span->_useCount == 0)//��ʱ˵��span�зֳ�ȥ������С����ڴ涼����������,ֱ�ӽ�����span����pagecache,pagecache�ٽ���ǰ��ҳ�ĺϲ�
		{
			_spanlist[index].Erase(span);
			span->_freeList = nullptr;//span�е�С���ڴ��Ѿ�������,����֪����ʼ��ַ�ͽ�����ַ,��ֱ���ÿ�
			span->_next = nullptr;
			span->_prev = nullptr;
			//�������ڴ�ʱ���ü�Ͱ����,����pagecache�����������Ҫ����
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

