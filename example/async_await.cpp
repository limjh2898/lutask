#include <iostream>
#include <ostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <lutask/Fiber.h>
#include <lutask/ConditionVariableAny.h>
#include <lutask/schedule/SharedWorkPolicy.h>

template< typename Fn, typename ... Args >
typename std::result_of<typename std::decay< Fn >::type(typename std::decay< Args >::type ...)>::type
Async(Fn&& fn, Args ... args)
{
    lutask::this_fiber::Yield();
    
}

inline void fn(std::string const& str, int n)
{
    for (int i = 0; i < n; ++i)
    {
        std::cout << i << ": " << str << std::endl;
        lutask::this_fiber::Yield();
    }
}

int main()
{
    try
    {
        lutask::Fiber f1(fn, "abc", 5);
        //std::cerr << "f1 : " << f1.() << std::endl;
        f1.Join();
        std::cout << "done." << std::endl;

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