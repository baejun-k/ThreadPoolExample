#ifndef __THREAD_POOL_HEADER__
#define __THREAD_POOL_HEADER__

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


namespace thread
{

class ThreadPool
{
public:
	template <class Fn, class... Args>
	using ResultType = typename std::result_of<Fn(Args...)>::type;

	template <class Fn, class... Args>
	using FutureResultType = std::future<ResultType<Fn, Args...>>;

	using WorkType = std::function<void()>;

	class AlreadyStoppedException : std::exception
	{
	public:
		AlreadyStoppedException()
			: std::exception("ThreadPool is stopped")
		{}
		~AlreadyStoppedException() override {}
	};

public:
	ThreadPool(const size_t& numThreads);
	~ThreadPool();

	template <class Fn, class... Args>
	FutureResultType<Fn, Args...> EnqueueJob(Fn&& fn, Args&&... args);

private:
	size_t m_numThreads;
	std::vector<std::thread> m_threads;
	std::queue<WorkType> m_jobs;

	std::condition_variable m_jobsTrigger;
	std::mutex m_jobsMutex;

	bool m_isAllStop;

	void Work();

};

}




inline thread::ThreadPool::ThreadPool(const size_t& numThreads)
	: m_numThreads(numThreads)
	, m_threads()
	, m_jobs()
	, m_jobsTrigger()
	, m_jobsMutex()
	, m_isAllStop(false)
{
	m_threads.reserve(m_numThreads);
	for (size_t i = 0; i < m_numThreads; ++i)
	{
		m_threads.emplace_back([this]() { this->Work(); });
	}
}

inline thread::ThreadPool::~ThreadPool()
{
	m_isAllStop = true;
	m_jobsTrigger.notify_all();

	for (auto& t : m_threads)
	{
		t.join();
	}
}

template<class Fn, class ...Args>
inline thread::ThreadPool::FutureResultType<Fn, Args...> thread::ThreadPool::EnqueueJob(Fn&& fn, Args&& ...args)
{
	if (m_isAllStop)
	{
		throw AlreadyStoppedException();
	}

	using ReturnType = ResultType<Fn, Args...>;
	using FutureReturnType = FutureResultType<Fn, Args...>;
	using PackagedTaskType = std::packaged_task<ReturnType()>;

	std::function<ReturnType()> CallableObject = std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...);
	std::shared_ptr<PackagedTaskType> pJob = std::make_shared<PackagedTaskType>(CallableObject);

	FutureReturnType jobResultFuture = pJob->get_future();
	{
		std::lock_guard<std::mutex> lock(m_jobsMutex);
		m_jobs.push([pJob]() { (*pJob)(); });
	}
	m_jobsTrigger.notify_one();

	return jobResultFuture;
}

inline void thread::ThreadPool::Work()
{
	while (true)
	{
		std::unique_lock<std::mutex> lock(m_jobsMutex);
		m_jobsTrigger.wait(lock, [this]() {
			return (!this->m_jobs.empty() || m_isAllStop);
		});
		if (m_isAllStop && this->m_jobs.empty())
		{
			return;
		}

		std::function<void()> job = std::move(m_jobs.front());
		m_jobs.pop();
		lock.unlock();

		job();
	}
}





#endif // !__THREAD_POOL_HEADER__