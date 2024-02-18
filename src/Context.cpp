#include <lutask/Context.h>
#include <lutask/Scheduler.h>
#include <lutask/schedule/RoundRobinPolicy.h>

namespace lutask
{

struct MainContext : public Context
{
public:
    MainContext() noexcept : Context(1, EType::MainContext, ELaunch::Post) {}
};

struct DispatcherContext final : public Context
{
private:
    lutask::context::FiberContext Run_(lutask::context::FiberContext&&) 
    {
        // 스케줄러 시작
        return GetScheduler()->Dispatch();
    }

public:
    DispatcherContext(Preallocated const& palloc, context::FixedSizeStack&& salloc) 
        : Context{ 0, EType::DispatcherContext, ELaunch::Post } 
    {
        c_ = lutask::context::FiberContext{ std::allocator_arg, palloc, std::move(salloc),
                                    std::bind(&DispatcherContext::Run_, this, std::placeholders::_1) };
    }
};

static Context::Ptr MakeDispatcherContext(lutask::context::FixedSizeStack&& salloc) 
{
    auto sctx = salloc.Allocate();
    void* storage = reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sizeof(DispatcherContext)))
        & ~static_cast<uintptr_t>(0xff));
    void* stackBottom = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sctx.Size));
    const std::size_t size = reinterpret_cast<uintptr_t>(storage) - reinterpret_cast<uintptr_t>(stackBottom);
    
    return Context::Ptr(new (storage) DispatcherContext{ Preallocated{ storage, size, sctx }, std::move(salloc) });
};

struct ContextInitializer 
{
    static thread_local Context* active_;
    static thread_local std::size_t counter_;

    using DefaultSchedulePolicy = schedule::RoundRobinPolicy;

    template< typename ... Args >
    ContextInitializer(Args && ... args) 
    {
        if (0 == counter_++)
        {
            Initialize(std::forward< Args >(args) ...);
        }
    }

    ~ContextInitializer()
    {
        if (0 == --counter_) 
        {
            Deinitialize();
        }
    }

    void Initialize()
    {
        Initialize(new DefaultSchedulePolicy(), lutask::context::FixedSizeStack());
    }

    void Initialize(lutask::schedule::IPolicy* policy, lutask::context::FixedSizeStack&& salloc)
    {
        Context* mainCtx = new MainContext{};
        auto sched = new lutask::Scheduler(policy);
        sched->AttachMainContext(mainCtx);
        auto dispatcher = MakeDispatcherContext(std::move(salloc));
        sched->AttachDispatcherContext(dispatcher);
        active_ = mainCtx;
    }

    void Deinitialize()
    {
        Context* mainCtx = active_;
        assert(mainCtx->IsContext(EType::MainContext));
        lutask::Scheduler* sched = mainCtx->GetScheduler();

        delete sched;
        delete mainCtx;
    }
};

// zero-initialization
thread_local Context* ContextInitializer::active_{ nullptr };
thread_local std::size_t ContextInitializer::counter_{ 0 };

bool Context::InitializeThread(lutask::schedule::IPolicy* policy, lutask::context::FixedSizeStack&& salloc) noexcept
{
    if (ContextInitializer::counter_ == 0)
    {
        ContextInitializer ctxInitializer(policy, std::move(salloc));
        Active();

        return true;
    }

    return false;
}

Context::Context(std::size_t initialCount, EType type, ELaunch policy)noexcept
    : useCount_(initialCount)
    , scheduler_(nullptr)
    , originScheduler_(nullptr)
    , tp_(std::chrono::steady_clock::time_point::max())
    , type_(type)
    , policy_(policy)
{ }

Context::~Context()
{
    if (IsContext(EType::DispatcherContext)) 
    {
        assert(nullptr == Active());
    }

    assert(waitList_.IsEmpty());
}

Context* Context::Active() noexcept
{
    thread_local static ContextInitializer ctx_initializer;
    return ContextInitializer::active_;
}

void Context::ChangeActive(Context* ctx) noexcept
{
    ContextInitializer::active_ = ctx;
}

void Context::ResetActive() noexcept
{
    ContextInitializer::active_ = nullptr;
}

void Context::Join()
{
    Context* activeCtx = Context::Active();

    if (terminated_ == false) 
    {
        waitList_.SuspendAndWait(activeCtx);

        assert(Context::Active() == activeCtx);
    }
}

void Context::Resume() noexcept
{
    Context* prev = this;
    std::swap(ContextInitializer::active_, prev);
    std::move(c_).ResumeWith([prev](lutask::context::FiberContext&& c)
        {
            prev->c_ = std::move(c);
            return lutask::context::FiberContext();
        });
}

void Context::Resume(std::unique_lock<std::mutex>& lk) noexcept
{
    Context* prev = this;
    std::swap(ContextInitializer::active_, prev);
    std::move(c_).ResumeWith([prev, &lk](lutask::context::FiberContext&& c)
        {
            prev->c_ = std::move(c);
            lk.unlock();
            return lutask::context::FiberContext();
        });
}


void Context::Resume(Context* readyCtx) noexcept
{
    Context* prev = this;
    std::swap(ContextInitializer::active_, prev);
    std::move(c_).ResumeWith([prev, readyCtx](lutask::context::FiberContext&& c)
        {
            prev->c_ = std::move(c);
            Context::Active()->GetScheduler()->Schedule(readyCtx);
            return lutask::context::FiberContext();
        });
}

void Context::Suspend() noexcept
{
    scheduler_->Suspend();
}

void Context::Suspend(std::unique_lock<std::mutex>& lk) noexcept
{
    scheduler_->Suspend(lk);
}

lutask::context::FiberContext Context::SuspendWithCC() noexcept
{
    Context* prev = this;
    std::swap(ContextInitializer::active_, prev);
    return std::move(c_).ResumeWith([prev](lutask::context::FiberContext&& c)
           {
               prev->c_ = std::move(c);
               return lutask::context::FiberContext();
           });
}

lutask::context::FiberContext Context::Terminate() noexcept
{
    terminated_ = true;
    waitList_.NotifyAll();
    assert(waitList_.IsEmpty());
    return scheduler_->Terminate(this);
}

void Context::Yield() noexcept
{
    scheduler_->Yield(Context::Active());
}

void Context::YieldOrigin() noexcept
{
    scheduler_->YieldOrigin(Context::Active());
}

bool Context::WaitUntil(std::chrono::steady_clock::time_point const& tp) noexcept
{
    assert(scheduler_ != nullptr);
    assert(this == Active());
    return scheduler_->WaitUntil(this, tp);
}

bool Context::Wake() noexcept
{
    assert(Context::Active() != nullptr);

    scheduler_->Schedule(this);

    return true;
}

void Context::Detach() noexcept
{
    if (policy_ == ELaunch::Async)
    {
        return;
    }
    assert(Context::Active() != this);
    GetScheduler()->DetachWorkerContext(this);
}

void Context::Attach(Context* ctx) noexcept
{
    assert(Context::Active() != ctx);
    GetScheduler()->AttachWorkerContext(ctx);
}

}