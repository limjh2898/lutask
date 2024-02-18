#pragma once

#include <queue>
#include <list>
#include <set>
#include <concurrent_unordered_map.h>
#include <concurrent_queue.h>

#include <lutask/Context.h>
#include <lutask/schedule/IPolicy.h>

namespace lutask
{

class Scheduler
{
	struct TimepointLess
	{
		bool operator()(Context* const l, Context* const r) const noexcept 
		{
			return l->tp_ < r->tp_;
		}
	};

private:
	Context*		mainContext_;
	Context::Ptr	dispatcherContext_;
	lutask::schedule::IPolicy*	policy_;
	bool				shutdown_{ false };

	std::list<Context*> workerQueue_;
	concurrency::concurrent_queue<Context*> terminatedQueue_;
	std::multiset<Context*, TimepointLess> sleepQueue_;
	std::mutex mtx_;

private:
	void ProcTerminated();
	void ProcSleepToReady();

public:
	Scheduler(lutask::schedule::IPolicy* policy) noexcept;
	Scheduler(Scheduler const&) = delete;
	Scheduler& operator=(Scheduler const&) = delete;

	virtual ~Scheduler();

	void Schedule(Context* ctx) noexcept;

	Context* GetDispatcherContext() const noexcept { return dispatcherContext_.get(); }

	lutask::context::FiberContext Dispatch() noexcept;
	lutask::context::FiberContext Terminate(Context* ctx) noexcept;

	void Yield(Context* ctx) noexcept;
	void YieldOrigin(Context* ctx) noexcept;

	bool WaitUntil(Context* ctx,
		std::chrono::steady_clock::time_point const& tp) noexcept;

	void Suspend() noexcept;
	void Suspend(std::unique_lock<std::mutex>& lk) noexcept;

	void AttachMainContext(Context* ctx) noexcept;
	void AttachDispatcherContext(Context::Ptr ctx) noexcept;
	void AttachWorkerContext(Context* ctx) noexcept;
	void DetachWorkerContext(Context* ctx) noexcept;
};

}