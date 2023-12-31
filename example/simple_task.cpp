#include <iostream>
#include <thread>
#include <tuple>
#include <utility>
#include <condition_variable>
#include <concurrent_queue.h>

#include <lutask/Fiber.h>

//#define await_async(fn, ...) lutask::Fiber(fn, )
//#define await(fn, ...)

int main()
{

	//int a = await_async(GetInt);

	std::cout << "returned second time: " << a << std::endl;
	std::cout << "main: done" << std::endl;
	return 0;
}