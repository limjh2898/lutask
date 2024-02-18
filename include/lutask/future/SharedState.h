#pragma once

#include <mutex>
#include <memory>
#include <lutask/Exceptions.h>
#include <lutask/smart_ptr/intrusive_ptr.h>

namespace lutask
{

class SharedStateBase
{
private:
    std::atomic_size_t useCount_ = 0;
    mutable std::condition_variable waiters_{};

protected:
    mutable std::mutex mtx_;
    bool ready_ = false;
    std::exception_ptr except_{};

    void MarkReadyAndNotify(std::unique_lock<std::mutex>& lk) noexcept
    {
        assert(lk.owns_lock());
        ready_ = true;
        lk.unlock();
        waiters_.notify_all();
    }

    void SetException(std::exception_ptr except, std::unique_lock<std::mutex>& lk)
    {
        assert(lk.owns_lock());
        if (ready_ == true)
        {
            throw lutask::TaskAlreadySatisfied();
        }
        except_ = except;
        MarkReadyAndNotify(lk);
    }

    void SetException(std::exception_ptr except)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        SetException(except, lk);
    }

    std::exception_ptr GetExceptionPtr(std::unique_lock<std::mutex>& lk)
    {
        assert(lk.owns_lock());
        return except_;
    }

    void Wait(std::unique_lock<std::mutex>& lk) const
    {
        assert(lk.owns_lock());
        waiters_.wait(lk, [this]() { return ready_; });
    }

public:

    SharedStateBase() = default;
    virtual ~SharedStateBase() = default;

    friend inline void intrusive_ptr_add_ref(SharedStateBase* p) noexcept
    {
        p->useCount_.fetch_add(1, std::memory_order_relaxed);
    }

    friend inline void intrusive_ptr_release(SharedStateBase* p) noexcept
    {
        if (1 == p->useCount_.fetch_sub(1, std::memory_order_release))
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            //p->~p();
        }
    }

    bool IsReady() const noexcept { return ready_; }
};

template<typename R>
class SharedState : public SharedStateBase
{
    alignas(alignof(R)) unsigned char storage_[sizeof(R)]{};
public:
    using Ptr = lutask::intrusive_ptr<SharedState>;

    SharedState() = default;
    virtual ~SharedState()
    {
        if (ready_ && !except_)
        {
            (reinterpret_cast<R*>(std::addressof(storage_)))->~R();
        }
    }

    SharedState(SharedState const&) = delete;
    SharedState& operator=(SharedState const&) = delete;

    void SetValue(R const& value)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        if (ready_)
        {
            throw lutask::TaskAlreadySatisfied();
        }

        ::new (static_cast<void*>(std::addressof(storage_))) R(value);
        MarkReadyAndNotify();
    }

    void SetValue(R&& value)
    {
        std::unique_lock<std::mutex> lk(mtx_);
        if (ready_)
        {
            throw lutask::TaskAlreadySatisfied();
        }
        ::new (static_cast<void*>(std::addressof(storage_))) R(std::move(value));
        MarkReadyAndNotify(lk);
    }

    R& Get()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        Wait(lk);
        if (except_)
        {
            std::rethrow_exception(except_);
        }
        return *reinterpret_cast<R*>(std::addressof(storage_));
    }
};

template<>
class SharedState<void> : public SharedStateBase
{
public:
    using Ptr = lutask::intrusive_ptr<SharedState<void>>;

    SharedState() = default;
    virtual ~SharedState()
    {
    }

    SharedState(SharedState const&) = delete;
    SharedState& operator=(SharedState const&) = delete;

    void SetValue()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        if (ready_)
        {
            throw lutask::TaskAlreadySatisfied();
        }

        MarkReadyAndNotify(lk);
    }

    void Get()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        Wait(lk);
        if (except_)
        {
            std::rethrow_exception(except_);
        }
    }
};

}