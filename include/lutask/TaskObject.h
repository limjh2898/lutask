#pragma once
#include <functional>
#include <memory>

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
			auto value = std::apply(fn_, std::make_tuple(std::forward<Args>(args)...));
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
