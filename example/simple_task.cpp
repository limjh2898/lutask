#include <iostream>
#include <thread>
#include <tuple>
#include <utility>
#include <condition_variable>
#include <concurrent_queue.h>
#include <lutask/context/windows/FixedSizeStack.h>
#include <lutask/context/TaskFContext.h>

enum class ETaskState {
	Ready = 1,
	Timeout,
	Deferred
};


class SharedStateBase
{
private:
	std::atomic<std::size_t> useCount_;
	mutable std::condition_variable waiters_{};

protected:
	mutable std::mutex  mtx_{};
	bool                ready_{ false };
	std::exception_ptr  except_{};


	void _MarkReadyAndNotify(std::unique_lock<std::mutex>& lk) noexcept
	{
		assert(lk.owns_lock());
		ready_ = true;
		lk.unlock();
		waiters_.notify_all();
	}

	void _OwnerDestroyed(std::unique_lock<std::mutex>& lk)
	{
		assert(lk.owns_lock());
		if (!ready_)
		{
			//SetException(std::make_exception_ptr())
		}
	}

	void _SetException(std::exception_ptr except, std::unique_lock<std::mutex>& lk)
	{
		assert(lk.owns_lock());
		if (ready_ == false)
		{
			// throw promise_already_satisfied();
		}
		except_ = except;
		_MarkReadyAndNotify(lk);
	}

	std::exception_ptr _GetExceptionPtr(std::unique_lock<std::mutex>& lk)
	{
		assert(lk.owns_lock());
		_Wait(lk);
		return except_;
	}

	void _Wait(std::unique_lock<std::mutex>& lk) const
	{
		assert(lk.owns_lock());
		waiters_.wait(lk, [this]() { return ready_; });
	}

	template< typename Rep, typename Period >
	ETaskState _WaitFor(std::unique_lock<std::mutex>& lk, std::chrono::duration<Rep, Period> const& timeoutDuration) const
	{
		assert(lk.owns_lock());
		return waiters_.wait_for(lk, timeoutDuration, [this]() { return ready_; }) 
			? ETaskState::Ready : ETaskState::Timeout;
	}

	template< typename Clock, typename Period >
	ETaskState _WaitUntil(std::unique_lock<std::mutex>& lk, std::chrono::time_point<Clock, Period> const& timeoutTime) const
	{
		assert(lk.owns_lock());
		return waiters_.wait_until(lk, timeoutTime, [this]() { return ready_; })
			? ETaskState::Ready : ETaskState::Timeout;
	}

	virtual void DeallocateFuture() noexcept = 0;

public:
	SharedStateBase() = default;
	virtual ~SharedStateBase() = default;

	SharedStateBase(SharedStateBase const&) = delete;
	SharedStateBase& operator=(SharedStateBase const&) = delete;

	void OwnerDestroyed()
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		_OwnerDestroyed(lk);
	}

	void SetException(std::exception_ptr except)
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		_SetException(except, lk);
	}

	std::exception_ptr GetExceptionPtr()
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		return _GetExceptionPtr(lk);
	}

	void Wait() const
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		_Wait(lk);
	}

	template< typename Rep, typename Period >
	ETaskState WaitFor(std::chrono::duration<Rep, Period> const& timeoutDuration) const
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		return _WaitFor(lk, timeoutDuration);
	}

	template< typename Clock, typename Period >
	ETaskState WaitUntil(std::unique_lock<std::mutex>& lk, std::chrono::time_point<Clock, Period> const& timeoutTime) const
	{
		std::unique_lock<std::mutex> lk{ mtx_ };
		return _WaitUntil(lk, timeoutTime);
	}

	friend inline
		void intrusive_ptr_add_ref(shared_state_base* p) noexcept {
		p->use_count_.fetch_add(1, std::memory_order_relaxed);
	}

	friend inline
		void intrusive_ptr_release(shared_state_base* p) noexcept {
		if (1 == p->use_count_.fetch_sub(1, std::memory_order_release)) {
			std::atomic_thread_fence(std::memory_order_acquire);
			p->deallocate_future();
		}
	}

};

template<typename Ret, typename ...Args>
struct TaskBase : public std::enable_shared_from_this<TaskBase<Ret, Args...>>
{
	using Ptr = std::shared_ptr<TaskBase>;

	virtual ~TaskBase() {}
	virtual void Run(Args&& ...args) = 0;
	virtual Ptr Reset() = 0;
};

template< typename Fn, typename Allocator, typename Ret, typename ... Args >
struct TaskObject : public TaskBase<Ret, Args...>
{
private:
	using BaseType = TaskBase<Ret, Args...>;
	using AllocatorTraits = std::allocator_traits<Allocator>;

public:
	typedef typename AllocatorTraits::template rebind_alloc<TaskObject> AllocatorType;

	TaskObject(AllocatorType const& alloc, Fn const& fn)
		: BaseType{}, fn_(fn), alloc_(alloc) {}

	TaskObject(AllocatorType const& alloc, Fn&& fn)
		: BaseType{}, fn_(std::move(fn)), alloc_{ alloc } {}

	void Run(Args&& ...args) override final 
	{
		try
		{
			std::apply(fn_, std::make_tuple(std::forward<Args>(args)...));
		}
		catch (...)
		{
			
		}
	}

	typename BaseType::Ptr Reset() override final 
	{

	}

protected:
	void DeallocateFuture() noexcept override final
	{

	}

private:
	Fn                  fn_;
	AllocatorType      alloc_;

	static void Destroy(AllocatorType const& alloc, TaskObject* p) noexcept
	{
		AllocatorType a{ alloc };
		typedef std::allocator_traits<AllocatorType> traity_type;
		traity_type::destroy(a, p);
		traity_type::deallocate(a, p, 1);
	}
};

template<typename _RetTy>
class AsyncTask
{
public:
	AsyncTask(const _RetTy& ret) 
		: value_(ret) 
	{
	}
	AsyncTask(_RetTy&& ret) : value_(std::move(ret)) {}

	_RetTy&& GetValue() const { return std::forward<_RetTy>(value_); }

private:
	_RetTy value_;
};

AsyncTask<int> GetInt()
{
	std::cout << "entered first time: " << 3 << std::endl;
	return 3;
}

class IAsyncTaskAwaiter
{

};

template<typename _RetTy, typename Func>
class AsyncTaskAwaiter
{
public:
	AsyncTaskAwaiter(lutask::context::TaskContext&& context)
		: context_(context)
	{}

private:
	lutask::context::TaskContext context_;
};

#define await_async(x) \
	 x.GetValue();
#define await(x)

template<typename _RetTy>
void logic(lutask::context::TaskContext&& t)
{
	auto awaiter = new AsyncTaskAwaiter<_RetTy, Func>(std::move(t));

	t = std::move(t).Resume();
}


concurrency::concurrent_queue<IAsyncTaskAwaiter*> taskQueue_;

int main()
{
	lutask::context::TaskContext t{ logic<int> };

	int a = await_async(GetInt());

	std::cout << "returned second time: " << a << std::endl;
	std::cout << "main: done" << std::endl;
	return 0;
}