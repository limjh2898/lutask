#pragma once

#include <lutask/Exceptions.h>
#include <lutask/future/SharedState.h>

namespace lutask
{

struct IFuture
{
    virtual bool IsReady() const noexcept = 0;
};

//template<typename Signature>
//struct PackagedTask : public IFuture
//{};

template<typename R>
struct FutureBase : public IFuture
{
    using SharedStatePtr = typename SharedState<R>::Ptr;

    SharedStatePtr state_;

    FutureBase() = default;
    explicit FutureBase(SharedStatePtr p) noexcept : state_(std::move(p)) {}
    FutureBase(FutureBase const& other) : state_(other.state_) {}
    FutureBase(FutureBase&& other) : state_(other.state_) { other.state_.reset(); }
    ~FutureBase() = default;

    FutureBase& operator=(FutureBase const& other) noexcept
    {
        if (this != &other)
        {
            state_ = other.state_;
        }
        return *this;
    }

    FutureBase& operator=(FutureBase&& other) noexcept
    {
        if (this != &other)
        {
            state_ = other.state_;
            other.state_.reset();
        }
        return *this;
    }

    bool IsValid() const noexcept { return nullptr != state_.get(); }
    std::exception_ptr GetExceptionPtr() 
    {
        if (IsValid() == false)
        {
            throw lutask::FutureUninitialized();
        }

        return state_->GetExceptionPtr();
    }

    bool IsReady() const noexcept override
    {
        return state_->IsReady();
    }
};

template<typename R>
class Future : private FutureBase<R>
{
private:
    using BaseType = FutureBase<R>;

    template<typename Signature>
    friend struct PackagedTask;

    explicit Future(typename BaseType::SharedStatePtr const& p) noexcept
        : BaseType(p) {}

public:
    Future() = default;
    Future(Future const&) = delete;
    Future(Future&& other) noexcept : BaseType(std::move(other)) {}

    Future& operator=(Future const&) = delete;
    Future& operator=(Future&& other)
    {
        if (this != &other)
        {
            BaseType::opreator = (std::move(other));
        }
        return *this;
    }

    R Get() 
    {
        if (BaseType::IsValid() == false)
        {
            throw lutask::FutureUninitialized();
        }

        typename BaseType::SharedStatePtr temp{};
        temp.swap(BaseType::state_);
        return std::move(temp->Get());
    }

    using BaseType::IsValid;
    using BaseType::GetExceptionPtr;
};

template<>
class Future<void> : private FutureBase<void>
{
private:
    using BaseType = FutureBase<void>;

    template<typename Signature>
    friend struct PackagedTask;

    explicit Future(typename BaseType::SharedStatePtr const& p) noexcept
        : BaseType(p) {}

public:
    Future() = default;
    Future(Future const&) = delete;
    Future(Future&& other) noexcept : BaseType(std::move(other)) {}

    Future& operator=(Future const&) = delete;
    Future& operator=(Future&& other)
    {
        if (this != &other)
        {
            BaseType::operator=(std::move(other));
        }
        return *this;
    }

    void Get()
    {
        if (BaseType::IsValid() == false)
        {
            throw lutask::FutureUninitialized();
        }

        typename BaseType::SharedStatePtr temp;
        temp.swap(BaseType::state_);
        temp->Get();
    }

    using BaseType::IsValid;
    using BaseType::GetExceptionPtr;
};
}