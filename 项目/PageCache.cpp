#include"PageCache.h"

PageCache PageCache::_singleton;

SpanData* PageCache::NewSpan(size_t k)//ȥ��K��Ͱ����span��central,��i��Ͱ�йҵ�span����iҳ�ڴ�
{
	//��KͰ����,ֱ�ӷ���,KͰû��span��������ȥ���Ѵ�span
	assert(k > 0);
	if (k > N_PAGES - 1)//��������ҳ��������128ҳ,pagecacheֻ�����������
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
	//�ȼ��K��Ͱ����span,��ֱ�ӷ���һ��
	if (!_spanList[k].Empty())
	{
		SpanData* KSpan = _spanList[k].PopFront();
		for (PAGE_ID i = 0; i < KSpan->_n; i++)
			//_idSpanMap[KSpan->_pageid + i] = KSpan;
			_idSpanMap.set(KSpan->_pageid + i, KSpan);
		return KSpan;
	}
	//�ߵ��������k��ͰΪ��,�������Ͱ��û��span,�ó������ѳ�����Сspan
	for (int i = k + 1; i < N_PAGES; i++)
	{
		if (!_spanList[i].Empty())//kҳ��span���ظ�centralcache,i-kҳ��span�ҵ�i-k��Ͱ��
		{
			SpanData* ISpan = _spanList[i].PopFront();
			//SpanData* KSpan = new SpanData;
			SpanData* KSpan = _spanPool.New();
			KSpan->_pageid = ISpan->_pageid;
			KSpan->_n = k;
			ISpan->_pageid += k;//��ͷKҳ�зָ�KSpan
			ISpan->_n -= k; //ҳ����i��Ϊi-k
			_spanList[ISpan->_n].PushFront(ISpan);//�ٽ���i-kҳ�����i-k��Ͱ
			//�洢Ispan����βҳ�Ÿ�ISpan��ӳ���ϵ
			// ����ֻ��Ҫӳ����βҳ������Ҫ������һ��ȫ��ҳ��ӳ��,��Ϊ�����зֳ�ȥ��span�ᱻ�з�ΪС����ڴ�
			// ��ЩС����ڴ涼�п��ܱ�ʹ��,���Ե����ǻ�����ʱ��ЩС����ڴ����ӳ����ǲ�ͬ��ҳ,����Щҳ���������KSpan
			// Ȼ��ISpan�в��ᱻ�з�ΪС����ڴ�,��ֻ��Ҫ�����Ƿ������ǰ��ҳ�ϲ�,��������ֻ��Ҫӳ����βҳ����ISpan�Ĺ�ϵ
			// ISpan��ΪҪ�ϲ�ҳ��ǰ��,��1000ҳҪ�ϲ�ISpan��1001ҳ,��ô1001��1001+n���ǿ��е�!ISpan��ΪҪ�ϲ�ҳ�ĺ���,��100ҳҪ�ϲ�ISpan��999ҳ,��ô999-n���ǿ��е�!
			//_idSpanMap[ISpan->_pageid] = ISpan;
			//_idSpanMap[ISpan->_pageid + ISpan->_n - 1] = ISpan;
			_idSpanMap.set(ISpan->_pageid, ISpan);
			_idSpanMap.set(ISpan->_pageid + ISpan->_n - 1, ISpan);
			//����id��span��ӳ���ϵ,����centralcache����С���ڴ�ʱ�鿴�Ŀ��ڴ����Ŀ�span
			for (PAGE_ID i = 0; i < KSpan->_n; i++)//���ص�KSpan��һ����nҳ,����ÿһҳ��ҳ�Ŷ���ӦKSpan�����ַ
				//_idSpanMap[KSpan->_pageid + i] = KSpan;
				_idSpanMap.set(KSpan->_pageid + i, KSpan);
			return KSpan;
		}
	}
	//�ߵ�����˵���������е�Ͱ��û��span��
	//��ʱ��Ҫ�������һ��128ҳ��span���������з�
	SpanData* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(N_PAGES - 1);
	bigSpan->_pageid = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = N_PAGES - 1;
	_spanList[bigSpan->_n].PushFront(bigSpan);//�����128ҳ��span���뵽Ͱ��
	return NewSpan(k);//�ٴε����Լ�,���һ������ǰ���forѭ��������
}

//����һ����ַ,���������ַ��Ӧ��span
SpanData* PageCache::MapObjectToSpan(void* obj)
{
	PAGE_ID pageId = (PAGE_ID)obj >> PAGE_SHIFT;//����ַ����13λ�������ҳ��
	//std::unique_lock<std::mutex> lock(_mtx);
	//auto ret = _idSpanMap.find(pageId);
	//if (ret != _idSpanMap.end())//���ҵ��˶�Ӧ��ҳ��,�ͷ��ض�Ӧ��span
	//	return _idSpanMap[pageId];
	//else assert(false);
	auto ret = (SpanData*)_idSpanMap.get(pageId);
	assert(ret);
	return ret;

}

void PageCache::ReleaseSpanToPageCache(SpanData* span)
{
	if (span->_n > N_PAGES - 1)//����128ҳ���ڴ�ֱ�ӻ�����,����Ҫ��pagecache
	{
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}
	//��spanǰ���ҳ���Խ��кϲ�,��������Ƭ����
	while (1)//������ǰ�ϲ�,ֱ���������ܺϲ������
	{
		PAGE_ID prevId = span->_pageid - 1;
		//auto prevret = _idSpanMap.find(prevId);
		//if (prevret == _idSpanMap.end())//ǰ��û��ҳ����
		//	break;
		auto ret = (SpanData*)_idSpanMap.get(prevId);
		SpanData* prevspan = ret;
		if (ret == nullptr)
			break;
		if (prevspan->_isUse == true)//ǰ���ҳ����ʹ��
			break;
		if (prevspan->_n + span->_n > N_PAGES - 1)//��ǰҳ������span��ҳ������128��,pagecache�Ҳ�����
			break;
		//��ʼ�ϲ�span��span��ǰ��ҳ
		span->_pageid = prevspan->_pageid;
		span->_n += prevspan->_n;
		_spanList[prevspan->_n].Erase(prevspan);//�����ϲ���ҳ��pagecache��������
		//delete prevspan;//��prevspan�е��������,����ҳ��,ҳ����
		_spanPool.Delete(prevspan);
	}
	while (1)//��������ϲ�,ֱ���������ܺϲ������
	{
		PAGE_ID nextId = span->_pageid + span->_n;
		//auto nextret = _idSpanMap.find(nextId);
		//if (nextret == _idSpanMap.end())//ǰ��û��ҳ����
		//	break;
		//SpanData* nextspan = nextret->second;
		auto ret = (SpanData*)_idSpanMap.get(nextId);
		if (ret == nullptr)
			break;
		SpanData* nextspan = ret;
		if (nextspan->_isUse == true)//ǰ���ҳ����ʹ��
			break;
		if (nextspan->_n + span->_n > N_PAGES - 1)//��ǰҳ������span��ҳ������128��,pagecache�Ҳ�����
			break;
		//��ʼ�ϲ�span��span��ǰ��ҳ
		span->_n += nextspan->_n;
		_spanList[nextspan->_n].Erase(nextspan);//�����ϲ���ҳ��pagecache��������
		//delete nextspan;//��prevspan�е��������,����ҳ��,ҳ����
		_spanPool.Delete(nextspan);
	}
	//�ϲ����span������
	_spanList[span->_n].PushFront(span);
	//�ϲ����,Ҫ���½����span����β��ҳ��id�����span����ӳ��,������span���ϲ��ҵ�ʱ��ʹ��
	//_idSpanMap[span->_pageid] = span;
	//_idSpanMap[span->_pageid + span->_n - 1] = span;
	_idSpanMap.set(span->_pageid, span);
	_idSpanMap.set(span->_pageid + span->_n - 1, span);
	span->_isUse = false;
}

