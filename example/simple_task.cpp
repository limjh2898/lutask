#include <iostream>
#include <thread>
#include <tuple>
#include <utility>
#include <condition_variable>
#include <concurrent_queue.h>
#include <lutask/context/windows/FixedSizeStack.h>
#include <lutask/context/TaskFContext.h>

class SharedStateBase
{
private:
	std::atomic<std::size_t> useCount_;
	mutable std::condition_variable waiters_{};

protected:
	mutable std::mutex  mtx_{};
	bool                ready_{ false };
	std::exception_ptr  except_{};


	void MarkReadyAndNotify(std::unique_lock<std::mutex>& lk) noexcept
	{
		assert(lk.owns_lock());
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