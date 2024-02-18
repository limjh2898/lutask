#pragma once

#include <lutask/future/Future.h>
#include <lutask/future/TaskObject.h>

namespace lutask
{

template<typename Signature>
struct PackagedTask;

template<typename R, typename ... Args>
struct PackagedTask<R(Args ...)>
{
private:
    using TaskPtr = typename TaskBase<R, Args...>::Ptr;
public:
    template<typename Fn>
    PackagedTask(Fn&& fn)
    {
        typedef TaskObject<typename std::decay<Fn>::type, R, Args...> ObjectType;

        task_.reset(new ObjectType(std::forward<Fn>(fn)));
    }

    ~PackagedTask() = default;


    PackagedTask(PackagedTask const&) = delete;
    PackagedTask& operator=(PackagedTask const&) = delete;

    PackagedTask(PackagedTask&& other) noexcept :
        task_{ std::move(other.task_) } 
    { }

    void operator()(Args ...args)
    {
        if (IsValid() == false)
            throw lutask::PackagedTaskUninitialized();

        task_->Run(std::forward<Args>(args)...);
    }

    bool IsValid() const noexcept { return task_.get() != nullptr; }

    R& Get() { return task_->Get(); }

    Future<R> GetFuture()
    {
        if (IsValid() == false)
        {
            throw lutask::PackagedTaskUninitialized();
        }

        return Future<R>(lutask::static_pointer_cast<SharedState<R>>(task_));
    }

private:
    TaskPtr task_;
};

}