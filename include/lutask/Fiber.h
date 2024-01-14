#pragma once

#include <lutask/Context.h>
#include <lutask/LaunchPolicy.h>

namespace lutask
{
class Fiber final
{
private:
	friend struct Context;

	Context::Ptr impl_;

	void _Start() noexcept;

public:
	template<typename Fn, typename ...Args>
	explicit Fiber(Fn&& fn, Args&& ...args)
		: Fiber( ELaunch::Post, std::allocator_arg, context::FixedSizeStack(), std::forward<Fn>(fn), std::forward<Args>(args)...)
	{}

	template<typename Fn, typename ...Args>
	explicit Fiber(ELaunch launch, Fn&& fn, Args&& ...args)
		: Fiber(launch, std::allocator_arg, context::FixedSizeStack(), std::forward<Fn>(fn), std::forward<Args>(args)...)
	{}


	template<typename StackAllocator, typename Fn, typename ...Args>
	explicit Fiber(ELaunch launch, std::allocator_arg_t, StackAllocator&& salloc, Fn&& fn, Args&& ...args)
	{
		impl_ = MakeWorkerContext(launch, std::forward<StackAllocator>(salloc), std::forward<Fn>(fn), std::forward<Args>(args)...);
		_Start();
	}

	Fiber(Fiber const&) = delete;
	Fiber(Fiber&& other) noexcept : impl_() 
	{
		Swap(other);
	}

	~Fiber()
	{
		if (Joinable())
			std::terminate();
	}

	Fiber& operator=(Fiber const&) = delete;
	Fiber& operator=(Fiber&& other) noexcept
	{
		if (Joinable())
			std::terminate();

		if (this == &other)
			return *this;

		impl_.swap(other.impl_);
		return *this;
	}

	void Swap(Fiber& other)
	{
		impl_.swap(other.impl_);
	}

	bool Joinable() const noexcept { return nullptr != impl_.get(); }
	void Join();
	void Detach();

public:
	template<typename Policy, typename ... Args>
	static void SetSchedulingPolicy(Args && ... args) noexcept 
	{
		Context::InitializeThread(new Policy(std::forward<Args>(args)...), lutask::context::FixedSizeStack());
	}
};

namespace this_fiber
{
	namespace detail
	{
		inline std::chrono::steady_clock::time_point convert(std::chrono::steady_clock::time_point const& timeout_time) noexcept 
		{
			return timeout_time;
		}

		template< typename Clock, typename Duration >
		std::chrono::steady_clock::time_point convert(std::chrono::time_point< Clock, Duration > const& timeout_time) 
		{
			return std::chrono::steady_clock::now() + (timeout_time - Clock::now());
		}
	}

	inline void Yield() noexcept { lutask::Context::Active()->Yield(); }

	template<typename Rep, typename Period>
	void sleep_until(std::chrono::time_point<Rep, Period> const& sleepTime)
	{
		std::chrono::steady_clock::time_point sleep_time = detail::convert(sleepTime);
		lutask::Context* activeCtx = lutask::Context::Active();
		activeCtx->WaitUntil(sleep_time);
	}

	template<typename Rep, typename Period>
	void sleep_for(std::chrono::duration<Rep, Period> const& timeoutDuration)
	{
		lutask::Context* activeCtx = lutask::Context::Active();
		activeCtx->WaitUntil(std::chrono::steady_clock::now() + timeoutDuration);
	}
}

}