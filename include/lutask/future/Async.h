#pragma once

#include <lutask/Fiber.h>
#include <lutask/future/PackagedTask.h>

namespace lutask
{

template< typename Fn, typename ... Args >
Future<
    typename std::result_of<typename std::decay< Fn >::type(typename std::decay< Args >::type ...)>::type
>
Async(Fn&& fn, Args ... args)
{
    typedef typename std::invoke_result<Fn, Args...>::type result_type;

    PackagedTask<result_type(typename std::decay< Args >::type...)> pt(std::forward<Fn>(fn));
    Future<result_type> f(pt.GetFuture());
    lutask::Fiber(lutask::ELaunch::Async, std::move(pt), std::forward<Args>(args)...).Detach();
    return f;
}

}

#define async_await(f, ...) lutask::Async(f, __VA_ARGS__);