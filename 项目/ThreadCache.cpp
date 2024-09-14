#include"ThreadCache.h"
#include"CentralCache.h"
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t align_size = AlignmentRule::AlignUp(size);//���������
	size_t index = AlignmentRule::Index(size);//�������ĸ�Ͱ
	if (!_freeList[index].Empty())//����ǰ������������Դ���������ͷŵ�����Դ
		return _freeList[index].Pop();
	else//��������û�оʹ����Ļ����ȡ�ռ�
		return ThreadCache::FetchFromCentralCache(index, align_size);
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//��������ʼ�ķ��������㷨,С�������һ��,��������һ��
	size_t massNum = min(_freeList[index].MaxSize(), AlignmentRule::NumMoveSize(size));//�����Ļ����ȡ���ٸ�����
	if (_freeList[index].MaxSize() == massNum)//������,�ʼһ���Ǹ�1,����һ���Ը����ܶ��ڴ�,��������size��С���ڴ�����,�����������
		_freeList[index].MaxSize() += 1;
	void* begin = nullptr;
	void* end = nullptr;
	//��Ҫmassnum������,����ʵ���ϲ�һ������ô���,����ֵΪʵ���ϻ�ȡ���ĸ���
	size_t fact = CentralCache::GetInstance()->CentralCache::FetchRangeObj(begin, end, massNum, size);//Ҫmassmun������,ÿ������Ĵ�С��size
	assert(fact != 0);
	if (fact == 1)
	{
		assert(begin == end);
		return begin;
	}
	else
	{
		//��������Ļ����ȡ�˶��,�򽫵�һ�����ظ�threadcachee,Ȼ���ٽ�ʣ����ڴ����threadcache������������
		_freeList[index].PushRange((*(void**)begin), end, fact - 1);
		return begin;
	}
	return nullptr;
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);
	//�ҵ���Ӧ����������Ͱ[[0,208]
	size_t index = AlignmentRule::Index(size);
	_freeList[index].Push(ptr);
	//�����������ڴ������������еĳ��ȴ���һ���������Ļ�������ĳ���,�ͽ��ڴ滹�����Ļ���
	if (_freeList[index].Size() >= _freeList[index].MaxSize())
		ListTooLong(_freeList[index], size);
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)//���ڵ���һ�������ڴ�ͻ���ȥһ������,��һ��ȫ������ȥ
{
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list.MaxSize());//start��end������Ͳ���
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}


