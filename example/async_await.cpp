#include <iostream>
#include <ostream>
#include <thread>
#include <mutex>
#include <functional>
#include <tuple>
#include <condition_variable>
#include <lutask/Fiber.h>
#include <lutask/ConditionVariableAny.h>
#include <lutask/schedule/SharedWorkPolicy.h>
#include <lutask/smart_ptr/intrusive_ptr.h>
#include <lutask/Exceptions.h>

template<typename R, typename ...Args>
struct TaskBase
{
    using Ptr = lutask::intrusive_ptr<TaskBase>;

    virtual ~TaskBase() = default;

    virtual void Run(Args&& ...args) = 0;
    virtual Ptr Reset() = 0;

    void Wait()
    {
    }

    void SetValue(R const& value)
    {
        value_ = value;
    }

    void SetValue(R&& value)
    {
        value_ = std::move(value);
    }

    void SetException(std::exception_ptr except)
    {
    }

private:
    R value_;
};

template<typename ...Args>
struct TaskBase<void, Args...>
{
    using Ptr = lutask::intrusive_ptr<TaskBase>;

    virtual ~TaskBase() = default;

    virtual void Run(Args&& ...args) = 0;
    virtual Ptr Reset() = 0;

    void Wait()
    {
    }

    void SetValue()
    {
    }

    void SetException(std::exception_ptr except)
    {
    }

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
};

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

    void operator()(Args ...args)
    {
        if (IsValid() == false)
            throw lutask::PackagedTaskUninitialized();

        task_->Run(std::forward<Args>(args)...);
    }

    bool IsValid() const noexcept { return task_.get() != nullptr; }

private:
    TaskPtr task_;
};

template< typename Fn, typename ... Args >
typename std::result_of<typename std::decay< Fn >::type(typename std::decay< Args >::type ...)>::type
Async(Fn&& fn, Args ... args)
{
    typedef typename std::result_of<typename std::decay< Fn >::type(typename std::decay< Args >::type ...)>::type result_type;

    PackagedTask<result_type(typename std::decay< Args >::type...)> pt(std::forward<Fn>(fn));
    lutask::Fiber(std::move(pt), std::forward<Args>(args)...).Detach();
    lutask::this_fiber::Yield();
}

#define async_await(f, ...) Async(f, __VA_ARGS__)

inline int fn(std::string const& str, int n)
{
    //for (int i = 0; i < n; ++i)
    {
        std::cout << n << ": " << str << std::endl;
        //lutask::this_fiber::Yield();
    }

    return 0;
}

inline void main_loop()
{
    
    auto result = async_await(fn, "abc", 5);

}

int main()
{
    try
    {
        lutask::Fiber f1(main_loop);
        f1.Join();

        return 0;
    }
    catch (std::exception const& e)
    {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "unhandled exception" << std::endl;
    }
    return 1;
}