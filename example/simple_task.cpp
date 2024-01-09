#include <iostream>
#include <ostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <lutask/Fiber.h>
#include <lutask/schedule/SharedWorkPolicy.h>

//class ITask
//{
//public:
//	virtual void Proc() = 0;
//	virtual uint64_t Id() = 0;
//};
//
//class TaskManager
//{
//
//};
//
//template< typename Signature >
//class PackagedTask;
//
//template< typename R, typename ... Args >
//class PackagedTask< R(Args ...) >
//{
//	bool isObtained = false;
//
//public:
//	PackagedTask() = default;
//
//	template<typename Fn>
//	explicit PackagedTask(Fn&& fn)
//		: 
//	{}
//
//	void operator()(Args&&... args)
//	{
//		try
//		{
//			auto value = std::apply(fn_, std::make_tuple(std::forward<Args>(args)...));
//			lutask::this_fiber::Yield();
//		}
//		catch (...)
//		{
//
//		}
//	}
//};
//
//template<typename R, typename Fn, typename ...Args>
//inline R&& AsyncAwait(Fn&& fn, Args&&... args)
//{
//	typedef typename std::result_of<
//		typename std::decay< Fn >::type(typename std::decay< Args >::type ...)
//	>::type     result_type;
//
//	PackagedTask<result_type(typename std::decay<Args>::type ...)> pt(std::forward<Fn>(fn));
//	lutask::Fiber(std::move(pt), std::forward<Args>(args)...).Detach();
//	return R();
//}
//
//#define async_await()

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

#include <iosfwd>

static std::size_t fiber_count{ 0 };
static std::mutex mtx_count{};
static std::condition_variable_any cnd_count;
typedef std::unique_lock< std::mutex > lock_type;

void whatevah(char me) {
    try 
    {
        std::thread::id my_thread = std::this_thread::get_id(); /*< get ID of initial thread >*/
        {
            std::cout << "fiber " << me << " started on thread " << my_thread << '\n';
        }
        for (unsigned i = 0; i < 10; ++i) 
        {
            lutask::this_fiber::Yield();
            std::thread::id new_thread = std::this_thread::get_id(); 
            if (new_thread != my_thread) 
            { 
                my_thread = new_thread;
                std::cout << "fiber " << me << " switched to thread " << my_thread << '\n';
            }
        }
    }
    catch (...) { }

    lock_type lk(mtx_count);
    if (0 == --fiber_count) { /*< Decrement fiber counter for each completed fiber. >*/
        lk.unlock();
        cnd_count.notify_all(); /*< Notify all fibers waiting on `cnd_count`. >*/
    }
}

void Thread(thread_barrier* b)
{
	std::cout << "thread started " << std::this_thread::get_id() << std::endl;
	lutask::Fiber::SetSchedulingPolicy<lutask::schedule::SharedWorkPolicy>();

    b->wait();
    lock_type lk(mtx_count);
    cnd_count.wait(lk, []() { return 0 == fiber_count; });
}

int main()
{
    std::cout << "main thread started " << std::this_thread::get_id() << std::endl;

    lutask::Fiber::SetSchedulingPolicy<lutask::schedule::SharedWorkPolicy>();

    for (char c : std::string("abcdefghijklmnopqrstuvwxyz")) 
    {
        lutask::Fiber([c]() { whatevah(c); }).Detach();
        ++fiber_count;
    }

    thread_barrier b(4);

    std::thread threads[] = {
       std::thread(Thread, &b),
       std::thread(Thread, &b),
       std::thread(Thread, &b)
    };

    b.wait();
    {
        lock_type lk(mtx_count);
        cnd_count.wait(lk, []() { return 0 == fiber_count; });
    }

    for (std::thread& t : threads) 
    {
        t.join();
    }

	return 0;
}


///////////////////////
//////////////////////

//void aaa(size_t num, size_t size, size_t div)
//{
//}
//
//void test(size_t num, size_t size, size_t div)
//{
//	std::vector<lutask::Fiber> fibers;
//	for (size_t i = 0; i < size; ++i)
//	{
//		//auto subNum = num + i * size / div;
//		lutask::Fiber f(lutask::ELaunch::Post, aaa, num, size / div, div);
//		fibers.emplace_back(std::move(f));
//	}
//
//	for (auto& f : fibers)
//	{
//		f.Join();
//	}
//}
//
//int main()
//{
//	try
//	{
//		size_t size = 10000;
//		size_t div = 10;
//
//		auto start = std::chrono::steady_clock::now();
//		test(0, size, div);
//
//		auto duration = std::chrono::steady_clock::now() - start;
//		std::cout << "duration: " << 
//			std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << " ms" << std::endl;
//	}
//	catch (std::exception const& e) 
//	{
//		std::cerr << "exception: " << e.what() << std::endl;
//	}
//	catch (...) 
//	{
//		std::cerr << "unhandled exception" << std::endl;
//	}
//
//	return 0;
//}