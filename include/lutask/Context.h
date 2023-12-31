#pragma once

#include <tuple>
#include <functional>
#include <memory>
#include <chrono>

#include <lutask/Fiber.h>
#include <lutask/context/FiberContext.h>
#include <lutask/Preallocated.h>
#include <lutask/TaskPolicy.h>

namespace lutask
{
namespace schedule
{
class IPolicy;
}

enum class EType 
{
    none = 0,
    MainContext = 1 << 1,
    DispatcherContext = 1 << 2,
    WorkerContext = 1 << 3,
};

class Scheduler;

struct Context 
{
    using TimePoint = std::chrono::steady_clock::time_point;

private:
    friend class Scheduler;
    friend class DispatcherContext;
    friend class MainContext;
    template< typename Fn, typename ... Arg > 
    friend class WorkerContext;

private:
    std::size_t useCount_;
    Scheduler* scheduler_{ nullptr };
    context::FiberContext c_{};
    TimePoint tp_;
    EType type_;
    ELaunch policy_;
    bool terminated_{ false };

    Context(std::size_t initial_count, EType t, ELaunch policy) noexcept :
        useCount_{ initial_count },
        tp_{ (std::chrono::steady_clock::time_point::max)() },
        type_{ t },
        policy_{ policy } {
    }

public:
    class id {
    private:
        Context* impl_{ nullptr };

    public:
        id() = default;

        explicit id(Context* impl) noexcept :
            impl_{ impl } {
        }

        bool operator==(id const& other) const noexcept {
            return impl_ == other.impl_;
        }

        bool operator!=(id const& other) const noexcept {
            return impl_ != other.impl_;
        }

        bool operator<(id const& other) const noexcept {
            return impl_ < other.impl_;
        }

        bool operator>(id const& other) const noexcept {
            return other.impl_ < impl_;
        }

        bool operator<=(id const& other) const noexcept {
            return !(*this > other);
        }

        bool operator>=(id const& other) const noexcept {
            return !(*this < other);
        }

        template< typename charT, class traitsT >
        friend std::basic_ostream< charT, traitsT >&
            operator<<(std::basic_ostream< charT, traitsT >& os, id const& other) {
            if (nullptr != other.impl_) {
                return os << other.impl_;
            }
            return os << "{not-valid}";
        }

        explicit operator bool() const noexcept {
            return nullptr != impl_;
        }

        bool operator!() const noexcept {
            return nullptr == impl_;
        }
    };

    static bool InitializeThread(schedule::IPolicy* policy, context::FixedSizeStack&& salloc) noexcept;
    static Context* Active() noexcept;
    static void ResetActive() noexcept;

public:
    Context(Context const&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context const&) = delete;
    Context& operator=(Context&&) = delete;

    void Resume() noexcept;
    void Resume(Context* ctx) noexcept;

    void Suspend() noexcept;
    lutask::context::FiberContext SuspendWithCC() noexcept;
    lutask::context::FiberContext Terminate() noexcept;


    bool IsResumable() const noexcept { return static_cast<bool>(c_); }

    bool IsContext(EType t) const noexcept { return type_ == t; }
    EType GetType() const noexcept { return type_; }
    Scheduler* GetScheduler() const noexcept { return scheduler_; }

    void Detach() noexcept;
    void Attach(Context* ctx) noexcept;
};

template<typename Fn, typename ...Args>
struct WorkerContext : public Context
{
    typename std::decay<Fn>::type fn_;
    std::tuple<Args ...> args_;

    Fiber Run_(Fiber&& /*c*/)
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
        : Context{ 1, type::worker_context, policy }
        , fn_(std::forward< Fn >(fn))
        , args_(std::forward< Args >(args) ...)
    {
        c_ = context::FiberContext{ std::allocator_arg, palloc, std::forward< StackAlloc >(salloc),
                                    std::bind(&WorkerContext::Run_, this, std::placeholders::_1) };
    }
};

template<typename StackAlloc, typename Fn, typename ...Args>
static std::shared_ptr<Context> MakeWorkerContext(ELaunch policy,
    StackAlloc&& salloc, Fn&& fn, Args ... args)
{
    typedef WorkerContext< Fn, Args ... >   context_t;

    auto sctx = salloc.allocate();
    void* storage = reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(sctx.sp) - static_cast<uintptr_t>(sizeof(context_t)))
        & ~static_cast<uintptr_t>(0xff));
    void* stack_bottom = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(sctx.sp) - static_cast<uintptr_t>(sctx.size));
    const std::size_t size = reinterpret_cast<uintptr_t>(storage) - reinterpret_cast<uintptr_t>(stack_bottom);
   

    return std::shared_ptr<Context>{
        new (storage) context_t{
            policy,
            Preallocated{ storage, size, sctx },
            std::forward< StackAlloc >(salloc),
            std::forward< Fn >(fn),
            std::forward< Args >(args) ... } };
}
}