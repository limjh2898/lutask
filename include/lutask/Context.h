#pragma once

#include <tuple>
#include <functional>
#include <memory>
#include <chrono>

#include <lutask/Preallocated.h>
#include <lutask/LaunchPolicy.h>
#include <lutask/WaitQueue.h>
#include <lutask/context/FiberContext.h>
#include <lutask/smart_ptr/intrusive_ptr.h>

namespace lutask
{
    namespace schedule
    {
        class IPolicy;
    }

    enum class EType
    {
        None = 0,
        MainContext = 1 << 1,
        DispatcherContext = 1 << 2,
        WorkerContext = 1 << 3,
        PinnedContext = MainContext | DispatcherContext
    };

    inline constexpr EType operator&(EType l, EType r)
    {
        return static_cast<EType>(static_cast<unsigned int>(l) & static_cast<unsigned int>(r));
    }

    class Scheduler;
    class Fiber;
    struct Context
    {
        friend class Fiber;
        using TimePoint = std::chrono::steady_clock::time_point;

        using Ptr = intrusive_ptr<Context>;

    private:
        friend class Scheduler;
        friend struct DispatcherContext;
        friend struct MainContext;
        friend struct ContextDeleter;
        template< typename Fn, typename ... Arg >
        friend struct WorkerContext;

    private:
        std::atomic_uint64_t useCount_;
        Scheduler* scheduler_;
        Scheduler* originScheduler_;
        context::FiberContext c_;
        WaitQueue waitList_;
        TimePoint tp_;
        EType type_;
        ELaunch policy_;
        bool terminated_{ false };

        Context(std::size_t initialCount, EType type, ELaunch policy) noexcept;

    public:
        static bool InitializeThread(schedule::IPolicy* policy, context::FixedSizeStack&& salloc) noexcept;
        static Context* Active() noexcept;
        static void ChangeActive(Context* ctx) noexcept;
        static void ResetActive() noexcept;

    public:
        Context(Context const&) = delete;
        Context(Context&&) = delete;
        Context& operator=(Context const&) = delete;
        Context& operator=(Context&&) = delete;

        ~Context();

        void Join();

        void Yield() noexcept;

        void Resume() noexcept;
        void Resume(std::unique_lock<std::mutex>& lk) noexcept;
        void Resume(Context* ctx) noexcept;

        void Suspend() noexcept;
        void Suspend(std::unique_lock<std::mutex>& lk) noexcept;

        lutask::context::FiberContext SuspendWithCC() noexcept;
        lutask::context::FiberContext Terminate() noexcept;

        bool WaitUntil(std::chrono::steady_clock::time_point const& tp) noexcept;
        bool Wake() noexcept;

        bool IsResumable() const noexcept { return static_cast<bool>(c_); }

        bool IsContext(EType t) const noexcept { return EType::None != (type_ & t); }
        ELaunch GetType() const noexcept { return policy_; }
        Scheduler* GetScheduler() const noexcept { return scheduler_; }
        void SetScheduler(Scheduler* sche) noexcept { scheduler_ = sche; }

        void Detach() noexcept;
        void Attach(Context* ctx) noexcept;

        friend void intrusive_ptr_add_ref(Context* ctx) noexcept
        {
            assert(nullptr != ctx);
            ctx->useCount_.fetch_add(1, std::memory_order_relaxed);
        }

        friend void intrusive_ptr_release(Context* ctx) noexcept
        {
            assert(nullptr != ctx);
            if (1 == ctx->useCount_.fetch_sub(1, std::memory_order_release))
            {
                std::atomic_thread_fence(std::memory_order_acquire);
                context::FiberContext c = std::move(ctx->c_);
                // destruct context
                ctx->~Context();
                // deallocated stack
                std::move(c).Resume();
            }
        }
    };

    template<typename Fn, typename ...Args>
    struct WorkerContext : public Context
    {
        typename std::decay<Fn>::type fn_;
        std::tuple<Args ...> args_;

        lutask::context::FiberContext Run_(lutask::context::FiberContext&& /*c*/)
        {
            auto fn = std::move(fn_);
            auto args = std::move(args_);

            std::apply(std::move(fn), std::move(args));

            return Terminate();
        }

    public:
        template< typename StackAlloc >
        WorkerContext(ELaunch policy, Preallocated const& palloc,
            StackAlloc&& salloc, Fn&& fn, Args ... args)
            : Context{ 1, EType::WorkerContext, policy }
            , fn_(std::forward< Fn >(fn))
            , args_(std::forward< Args >(args) ...)
        {
            c_ = context::FiberContext{ std::allocator_arg, palloc, std::forward< StackAlloc >(salloc),
                                        std::bind(&WorkerContext::Run_, this, std::placeholders::_1) };
        }
    };

    template<typename StackAlloc, typename Fn, typename ...Args>
    static Context::Ptr MakeWorkerContext(ELaunch policy, StackAlloc&& salloc, Fn&& fn, Args ... args)
    {
        typedef WorkerContext< Fn, Args ... >   ContextType;

        auto sctx = salloc.Allocate();
        void* storage = reinterpret_cast<void*>(
            (reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sizeof(ContextType)))
            & ~static_cast<uintptr_t>(0xff));
        void* stack_bottom = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sctx.Size));
        const std::size_t size = reinterpret_cast<uintptr_t>(storage) - reinterpret_cast<uintptr_t>(stack_bottom);


        return Context::Ptr(new (storage) ContextType(policy,
            Preallocated(storage, size, sctx), std::forward<StackAlloc>(salloc),
            std::forward<Fn>(fn), std::forward<Args>(args)...));
    }
}