#include <iostream>
#include <thread>
#include "ThreadPool.h"


int WorkFunction(const int& t, const int& id)
{
	printf("ID %d beg \n", id);
	std::this_thread::sleep_for(std::chrono::seconds(t));
	printf("ID %d end after %d(sec)\n", id, t);
	return ( t + id );
}

struct WorkClass
{
	int operator()(const int& t, const int& id) const
	{
		return WorkFunction(t, id);
	}
};

int main(int argc, char** argv)
{
	thread::ThreadPool pool(3);

	std::vector<std::future<int>> futures;
	for (int i = 0; i < 10; i++)
	{
		if (i % 3 == 2)
		{
			futures.emplace_back(pool.EnqueueJob(WorkClass{}, i % 3 + 1, i));
		}
		else
		{
			futures.emplace_back(pool.EnqueueJob(WorkFunction, i % 3 + 1, i));
		}
	}
	for (auto& f : futures)
	{
		printf("result : %d\n", f.get());
	}

	return 0;
}