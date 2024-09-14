#include"ThreadCache.h"
#include"CentralCache.h"
#include"PageCache.h"
#include"ConcurrentAlloc.h"

void test_concurrent_alloc1()
{
	void* ptr1 = ConcurrentAlloc(6);
	void* ptr2 = ConcurrentAlloc(7);
	void* ptr3 = ConcurrentAlloc(8);
	void* ptr4 = ConcurrentAlloc(8);
	void* ptr5 = ConcurrentAlloc(8);
	void* ptr6 = ConcurrentAlloc(8);
	void* ptr7 = ConcurrentAlloc(8);

	cout << ptr1 << endl;
	cout << ptr2 << endl;
	cout << ptr3 << endl;
	cout << ptr3 << endl;
	ConcurrentFree(ptr1);
	ConcurrentFree(ptr2);
	ConcurrentFree(ptr3);
	ConcurrentFree(ptr4);
	ConcurrentFree(ptr5);
	ConcurrentFree(ptr6);
	ConcurrentFree(ptr7);
}

void test_Muti_alloc2()
{
	std::vector<void*> v;
	for (int i = 0; i < 5; i++)
	{
		void* p1 = ConcurrentAlloc(4);
		v.push_back(p1);
	}
	for (auto& x : v)
		ConcurrentFree(x);
}
void test_Muti_alloc1()
{
	std::vector<void*> v;
	for (int i = 0; i < 5; i++)
	{
		void* p1 = ConcurrentAlloc(4);
		v.push_back(p1);
	}
	for (auto& x : v)
		ConcurrentFree(x);
}

void testthread()
{
	std::thread t1(test_Muti_alloc1);
	std::thread t2(test_Muti_alloc2);
	t1.join();
	t2.join();
}

void BigAlloc()
{
	void* p1 = ConcurrentAlloc(257 * 1024);
	ConcurrentFree(p1);
	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	ConcurrentFree(p2);
}

int main()
{
	test_concurrent_alloc1();
	BigAlloc();
	return 0;
}

