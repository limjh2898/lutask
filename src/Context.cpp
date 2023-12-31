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

class DispatcherContext final : public Context 
{
private:
    lutask::context::FiberContext Run_(lutask::context::FiberContext&&) 
    {
        // 스케줄러 시작
        return GetScheduler()->Dispatch();
    }

public:
    DispatcherContext(Preallocated const& palloc, context::FixedSizeStack&& salloc) 
        : Context{ 0, type::dispatcher_context, launch::post } 
    {
        c_ = lutask::context::FiberContext{ std::allocator_arg, palloc, std::move(salloc),
                                    std::bind(&DispatcherContext::Run_, this, std::placeholders::_1) };
    }
};

static Context* MakeDispatcherContext(lutask::context::FixedSizeStack&& salloc) {
    auto sctx = salloc.Allocate();
    void* storage = reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sizeof(DispatcherContext)))
        & ~static_cast<uintptr_t>(0xff));
    void* stackBottom = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sctx.Size));
    const std::size_t size = reinterpret_cast<uintptr_t>(storage) - reinterpret_cast<uintptr_t>(stackBottom);
    
    return new (storage) DispatcherContext{ Preallocated{ storage, size, sctx }, std::move(salloc) } 
};

struct ContextInitializer 
{
    static thread_local lutask::Context* active_;
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
        lutask::Context* mainCtx = new MainContext{};
        auto sched = new lutask::Scheduler(policy);
        sched->AttachMainContext(mainCtx);
        sched->AttachDispatcherContext(MakeDispatcherContext(std::move(salloc)));
        active_ = mainCtx;
    }

    void Deinitialize()
    {
        lutask::Context* mainCtx = active_;
        assert(mainCtx->is_context(type::main_context));
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
}

Context* Context::Active() noexcept
{
    thread_local static ContextInitializer ctx_initializer;
    return ContextInitializer::active_;
}

void Context::ResetActive() noexcept
{
    ContextInitializer::active_ = nullptr;
}

void Context::Detach() noexcept
{
    assert(Context::Active() != this);
    GetScheduler()->DetachWorkerContext(this);
}

void Context::Attach(Context* ctx) noexcept
{
    assert(Context::Active() != ctx);
    GetScheduler()->AttachWorkerContext(this);
}

}