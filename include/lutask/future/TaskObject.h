#pragma once

#include <lutask/future/SharedState.h>

namespace lutask
{

template<typename R, typename ...Args>
struct TaskBase : public SharedState<R>
{
    using Ptr = lutask::intrusive_ptr<TaskBase>;

    virtual ~TaskBase() = default;

    virtual void Run(Args&& ...args) = 0;
    virtual Ptr Reset() = 0;
};

template<typename Fn, typename R, typename ...Args>
struct TaskObject : public TaskBase<R, Args...>
{
    TaskObject(Fn const& fn)
        : fn_(fn)
    {}

    void Run(Args&& ...args) override final
    {
        try
        {
            this->SetValue(std::apply(fn_, std::make_tuple(std::forward<Args>(args)...)));
        }
        catch (...)
        {
            this->SetException(std::current_exception());
        }
    }

    typename TaskBase<R, Args...>::Ptr Reset() override final
    {
        //delete 
        return this;
    }

private:
    Fn fn_;
};

template<typename Fn, typename ...Args>
struct TaskObject<Fn, void, Args...> : public TaskBase<void, Args...>
{
    void Run(Args&& ...args) override final
    {
        try
        {
            std::apply(fn_, std::make_tuple(std::forward<Args>(args)...));
            this->SetValue();
        }
        catch (...)
        {
            this->SetException(std::current_exception());
        }
    }

    typename TaskBase<void, Args...>::Ptr Reset() override final
    {
        //delete 
        return this;
    }
};

}