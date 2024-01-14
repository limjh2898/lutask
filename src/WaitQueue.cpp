#include <lutask/WaitQueue.h>
#include <lutask/Context.h>

namespace lutask
{
void WaitQueue::SuspendAndWait(Context* activeCtx)
{
	waits_.push(activeCtx);
	activeCtx->Suspend();
}

void WaitQueue::SuspendAndWait(std::unique_lock<std::mutex>& lk, Context* activeCtx)
{
	waits_.push(activeCtx);
	activeCtx->Suspend(lk);
}

void WaitQueue::NotifyOne()
{
	while (waits_.empty() == false)
	{
		Context* ctx = waits_.front();
		waits_.pop();

		if (ctx->Wake())
			break;
	}
}

void WaitQueue::NotifyAll()
{
	while (waits_.empty() == false)
	{
		Context* ctx = waits_.front();
		waits_.pop();
		ctx->Wake();
	}
}

bool WaitQueue::IsEmpty() const
{
	return waits_.empty();
}

}
