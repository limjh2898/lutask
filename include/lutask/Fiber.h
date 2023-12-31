#pragma once

#include <lutask/Context.h>
#include <lutask/TaskPolicy.h>

namespace lutask
{
class Fiber
{
private:
	friend class Context;

	Context* impl_;

	void _Start() noexcept;

public:
	template<
		typename Fn, 
		typename ...Args>
	explicit Fiber(Fn&& fn, Args&& ...args)
		: Fiber{ ELaunch::Post, std::allocator_arg,  }
	{}

	template<
		typename Fn,
		typename ...Args>
	explicit Fiber(ELaunch launch, Fn&& fn, Args&& ...args)
		: Fiber{}
	{}

};
}