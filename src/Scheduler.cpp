#include <lutask/Scheduler.h>

namespace lutask
{
Scheduler::Scheduler(lutask::schedule::IPolicy* policy) noexcept
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
        terminatedQueue_.pop();
        if (ctx == nullptr)
            continue;

        assert(ctx->IsContext(EType::WorkerContext));
        assert(this == ctx->GetScheduler());
        assert(ctx->terminated_);    
    }
}

void Scheduler::ProcSleepToReady()
{
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    for (auto iter = sleepQueue_.begin(); iter != sleepQueue_.end();)
    {
        Context* ctx = (*iter);
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
            std::chrono::steady_clock::time_point suspendTime =
                (std::chrono::steady_clock::time_point::max)();

            auto iter = sleepQueue_.begin();
            if (sleepQueue_.end() != iter)
            {
                suspendTime = (*iter)->tp_;
            }
            policy_->SuspendUntil(suspendTime);
        }
    }
    ProcTerminated();

    // return to main-context
    return mainContext_->SuspendWithCC();
}

lutask::context::FiberContext Scheduler::Terminate(Context* ctx) noexcept
{
    assert(nullptr != ctx);
    assert(Context::Active() == ctx);
    assert(this == ctx->GetScheduler());
    assert(ctx->IsContext(EType::WorkerContext));

    return policy_->PickNext()->SuspendWithCC();
}

void Scheduler::Yield(Context* ctx) noexcept
{
    assert(nullptr != ctx);
    assert(Context::Active() == ctx);
    assert(ctx->IsContext(EType::WorkerContext) || ctx->IsContext(EType::MainContext));

    policy_->PickNext()->Resume(ctx);
}

bool Scheduler::WaitUntil(Context* ctx, std::chrono::steady_clock::time_point const& tp) noexcept
{
    assert(nullptr != ctx);
    assert(Context::Active() == ctx);
    assert(ctx->IsContext(EType::WorkerContext) || ctx->IsContext(EType::MainContext));

    ctx->tp_ = tp;
    sleepQueue_.insert(ctx);

    policy_->PickNext()->Resume(ctx);

    return std::chrono::steady_clock::now() < tp;
}

void Scheduler::Suspend() noexcept
{
    policy_->PickNext()->Resume();
}

void Scheduler::AttachMainContext(Context* ctx) noexcept
{
    mainContext_ = ctx;
    ctx->scheduler_ = this;
}

void Scheduler::AttachDispatcherContext(Context* ctx) noexcept
{
    dispatcherContext_ = ctx;
    dispatcherContext_->scheduler_ = this;
    policy_->Awakened(dispatcherContext_);
}

void Scheduler::AttachWorkerContext(Context* ctx) noexcept 
{
    assert(nullptr != ctx);
    assert(nullptr == ctx->GetScheduler());
    workerQueue_.push(ctx);
    ctx->scheduler_ = this;
    // an attached context must belong at least to worker-queue
}

void Scheduler::DetachWorkerContext(Context* ctx) noexcept
{
    assert(nullptr != ctx);
    assert(ctx->IsContext(EType::PinnedContext) == false);
    // unlink
    ctx->scheduler_ = nullptr;
}
}