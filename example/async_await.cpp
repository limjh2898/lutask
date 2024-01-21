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
            //p->();
        }
    }
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

template<typename Signature>
struct PackagedTask;

template<typename R>
struct FutureBase
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
            other.state_.Reset();
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

    void Wait() const
    {
        if (IsValid() == false)
        {
            throw lutask::FutureUninitialized();
        }

        state_->Wait();
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
    using BaseType::Wait;
};

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

template< typename Fn, typename ... Args >
Future<
    typename std::result_of<typename std::decay< Fn >::type(typename std::decay< Args >::type ...)>::type
>
Async(Fn&& fn, Args ... args)
{
    typedef typename std::result_of<typename std::decay< Fn >::type(typename std::decay< Args >::type ...)>::type result_type;

    PackagedTask<result_type(typename std::decay< Args >::type...)> pt(std::forward<Fn>(fn));
    Future<result_type> f(pt.GetFuture());
    lutask::Fiber(lutask::ELaunch::Async, std::move(pt), std::forward<Args>(args)...).Detach();
    lutask::this_fiber::Yield();
    return f;
}

#define async_await(f, ...) Async(f, __VA_ARGS__).Get();

class thread_barrier {
private:
    std::size_t             initial_;
    std::size_t             current_;
    bool                    cycle_{ true };
    std::mutex              mtx_{};
    std::condition_variable cond_{};

public:
    explicit thread_barrier(std::size_t initial) :
        initial_{ initial },
        current_{ initial_ } {
        assert(0 != initial);
    }

    thread_barrier(thread_barrier const&) = delete;
    thread_barrier& operator=(thread_barrier const&) = delete;

    bool wait() {
        std::unique_lock< std::mutex > lk(mtx_);
        const bool cycle = cycle_;
        if (0 == --current_) {
            cycle_ = !cycle_;
            current_ = initial_;
            lk.unlock(); // no pessimization
            cond_.notify_all();
            return true;
        }
        cond_.wait(lk, [&]() { return cycle != cycle_; });
        return false;
    }
};

static std::size_t fiber_count{ 0 };
static std::mutex mtx_count{};
static lutask::ConditionVariableAny cnd_count;
typedef std::unique_lock< std::mutex > lock_type;
bool end = false;

void Thread(thread_barrier* b)
{
    //std::cout << "thread started " << std::this_thread::get_id() << std::endl;
    lutask::Fiber::SetSchedulingPolicy<lutask::schedule::SharedWorkPolicy>();

    b->wait();
    lock_type lk(mtx_count);
    cnd_count.Wait(lk, []() { return end; });
}

inline int fn(std::string const& str, int n)
{
    //for (int i = 0; i < n; ++i)
    {
        std::cout << "## Proc" << n << ": " << str << " - thread id: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 10;
}


inline void proc_logic(int i)
{
    std::cout << "main_loop started [" << i << "]" << std::this_thread::get_id() << std::endl;
    auto result = async_await(fn, "abc", 5);
    std::cout << "main_loop ended [" << i << "]" << std::this_thread::get_id() << std::endl;
    
    std::cout << "main_loop2 started [" << i << "]" << std::this_thread::get_id() << std::endl;
    result = async_await(fn, "abc", 5);
    std::cout << "main_loop2 ended [" << i << "]" << std::this_thread::get_id() << std::endl;

    end = true;
}

int main()
{
    std::cout << "main thread started " << std::this_thread::get_id() << std::endl;

    thread_barrier b(1);

    try
    {
        std::vector<lutask::Fiber> fs;

        for (int i = 0; i < 10; i++)
        {
            lutask::Fiber f1(proc_logic, i);
            fs.push_back(std::move(f1));
        }

        std::thread threads[] = {
            std::thread(Thread, &b)
            //std::thread(Thread, &b),
            //std::thread(Thread, &b)
        };

        b.wait();
        {
            lock_type lk(mtx_count);
            cnd_count.Wait(lk, []() { return end; });
        }

        for (std::thread& t : threads)
        {
            t.join();
        }

        for (auto& f : fs)
        {
            f.Join();
        }

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