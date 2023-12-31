#include <lutask/schedule/RoundRobinPolicy.h>
#include <lutask/Context.h>
#include <cassert>

namespace lutask {
namespace schedule {

void RoundRobinPolicy::Awakened(Context* context) noexcept
{
	assert(nullptr != ctx);
	assert(ctx->IsResumable());
	readyQueue_.push(context);
}

Context* RoundRobinPolicy::PickNext() noexcept
{
	if (readyQueue_.empty() == false)
	{
		Context* ctx = readyQueue_.front();
		readyQueue_.pop();
		// prefetch?
		assert(ctx != nullptr);
		assert(ctx->IsResumable());
	}
	return nullptr;
}

bool RoundRobinPolicy::HasReadyFibers() const noexcept
{
	return readyQueue_.empty() == false;
}

void RoundRobinPolicy::SuspendUntil(TimePoint const& timePoint) noexcept
{
	if ((std::chrono::steady_clock::time_point::max)() == timePoint) 
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		cnd_.wait(lk, [&]() { return flag_; });
		flag_ = false;
	}
	else 
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		cnd_.wait_until(lk, timePoint, [&]() { return flag_; });
		flag_ = false;
	}
}

void RoundRobinPolicy::Notify() noexcept
{
	std::unique_lock<std::mutex> lk{ mtx_ };
	flag_ = true;
	lk.unlock();
	cnd_.notify_all();
}

}}
