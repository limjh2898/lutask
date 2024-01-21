#include <lutask/schedule/SharedWorkPolicy.h>
#include <lutask/Context.h>

namespace lutask {
namespace schedule {

std::queue<Context*> SharedWorkPolicy::readyQueue_;
std::mutex SharedWorkPolicy::rqueueMutex_;

void SharedWorkPolicy::Awakened(Context* ctx) noexcept
{
	if (ctx->IsContext(EType::PinnedContext))
	{
		localQueue_.push(ctx);
	}
	else
	{
		ctx->Detach();

		std::unique_lock<std::mutex> lk(rqueueMutex_);
		readyQueue_.push(ctx);
	}
}

Context* SharedWorkPolicy::PickNext() noexcept
{
	Context* ctx = nullptr;
	std::unique_lock<std::mutex> lk(rqueueMutex_);
	if (readyQueue_.empty() == false)
	{
		ctx = readyQueue_.front();
		readyQueue_.pop();
		lk.unlock();

		assert(ctx != nullptr);

		Context::Active()->Attach(ctx);
	}
	else
	{
		lk.unlock();
		if (localQueue_.empty() == false)
		{
			ctx = localQueue_.front();
			localQueue_.pop();
		}
	}
	return ctx;
}

bool SharedWorkPolicy::HasReadyFibers() const noexcept
{
	std::unique_lock<std::mutex> lk(rqueueMutex_);
	return !readyQueue_.empty() || !localQueue_.empty();
}

void SharedWorkPolicy::SuspendUntil(TimePoint const& time_point) noexcept
{
	if (suspend_)
	{	
		if (std::chrono::steady_clock::time_point::max() == time_point)
		{
			std::unique_lock<std::mutex> lk(rqueueMutex_);
			cnd_.wait(lk, [this]() { return flag_; });
			flag_ = false;
		}
		else
		{
			std::unique_lock<std::mutex> lk(rqueueMutex_);
			cnd_.wait_until(lk, time_point, [this]() { return flag_; });
			flag_ = false;
		}
	}
}

void SharedWorkPolicy::Notify() noexcept
{
	if (suspend_) 
	{
		std::unique_lock<std::mutex> lk(rqueueMutex_);
		flag_ = true;
		lk.unlock();
		cnd_.notify_all();
	}
}

void SharedWorkPolicy::AwakenedAsync(Context* ctx) noexcept
{
	ctx->Detach();
	std::unique_lock<std::mutex> lk(rqueueMutex_);
	readyQueue_.push(ctx);
}

}}