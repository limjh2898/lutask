#include <lutask/Scheduler.h>

namespace lutask
{
Scheduler::Scheduler(schedule::IPolicy* policy)
	: mainContext_(nullptr)
	, dispatcherContext_(nullptr)
	, policy_(policy)
	, shutdown_(false)
{
}

Scheduler::~Scheduler()
{
	
}

void Scheduler::ProcTerminated()
{
    while (!terminatedQueue_.empty())
    {
        Context* ctx = terminatedQueue_.front();
        terminatedQueue_.pop_front();
        if (ctx == nullptr)
            continue;

        assert(ctx->IsContext());
        assert(this == ctx->GetScheduler());
        assert(ctx->terminated_);    
    }
}

void Scheduler::ProcSleepToReady()
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    for (auto iter = sleepQueue_.begin();;)
    {
        assert(!ctx->IsContext(EType::DispatcherContext));
        assert(mainContext_ == ctx);

        if (ctx->tp_ <= now) 
        {
            iter = sleepQueue_.erase(iter);
            ctx->tp_ = (std::chrono::steady_clock::time_point::max)();
            Schedule(ctx);
        }
        else 
        {
            break;
        }
    }
}

void Scheduler::Schedule(Context* ctx) noexcept
{
    assert(nullptr != ctx);

    policy_->Awakened(ctx);
}

lutask::context::FiberContext Scheduler::Dispatch() noexcept
{
    assert(Context::Active() == dispatcherContext_);
    for (;;) 
    {
        if (shutdown_) 
        {
            // 셧다운 전 모든 작업 처리
            policy_->Notify();
            if (workerQueue_.empty()) 
                break;
        }

        ProcTerminated();
        ProcSleepToReady();

        Context* ctx = policy_->PickNext();
        if (nullptr != ctx) 
        {
            assert(ctx->IsResumable());
            ctx->Resume(dispatcherContext_);
            assert(Context::Active() == dispatcherContext_);
        }
        else 
        {
            // no ready context, wait till signaled
            // set deadline to highest value
            std::chrono::steady_clock::time_point suspendTime =
                (std::chrono::steady_clock::time_point::max)();

            // get lowest deadline from sleep-queue
            auto iter = sleepQueue_.begin();
            if (sleepQueue_.end() != iter)
            {
                suspend_time = (*iter)->tp_;
            }
            // no ready context, wait till signaled
            policy_->SuspendUntil(suspendTime);
        }
    }
    ProcTerminated();

    // return to main-context
    return main_ctx_->suspend_with_cc();
}

lutask::context::FiberContext Scheduler::Terminate(Context* ctx) noexcept
{
    assert(nullptr != ctx);
    assert(Context::Active() == ctx);
    assert(this == ctx->GetScheduler());
    assert(ctx->IsContext(EType::WorkerContext));

    return policy_->PickNext()->SuspendWithCC();
}

void Scheduler::AttachMainContext(Context* ctx) noexcept
{
	mainContext_ = ctx;
	ctx->scheduler_ = this;
}

void lutask::Scheduler::AttachDispatcherContext(Context* ctx) noexcept
{
    dispatcherContext_ = ctx;
    ctx->dispatcherContext_ = this;
}

}