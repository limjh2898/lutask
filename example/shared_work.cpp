#include <iostream>
#include <ostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <lutask/Fiber.h>
#include <lutask/ConditionVariableAny.h>
#include <lutask/schedule/SharedWorkPolicy.h>

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
static lutask::ConditionVariableAny cnd_count;
typedef std::unique_lock< std::mutex > lock_type;

void whatevah(char me) 
{
    try 
    {
        std::thread::id my_thread = std::this_thread::get_id(); /*< get ID of initial thread >*/
        {
            std::cout << "fiber " << me << " started on thread " << my_thread << '\n';
        }
        for (unsigned i = 0; i < 10; ++i)
        {
            std::thread::id new_thread = std::this_thread::get_id();
            if (new_thread != my_thread) 
            {
                std::cout << "fiber " << me << " switched to thread [prev id:" << my_thread <<  "] [cur id:" << new_thread << "]\n";
                my_thread = new_thread;
            }
        }
        lutask::this_fiber::YieldOrigin();
        my_thread = std::this_thread::get_id();
        std::cout << "fiber " << me << " ended on thread " << my_thread << '\n';
    }
    catch (...) { }

    lock_type lk(mtx_count);
    if (0 == --fiber_count) {
        lk.unlock();
        cnd_count.NotifyAll();
    }
}

void Thread(thread_barrier* b)
{
	std::cout << "thread started " << std::this_thread::get_id() << std::endl;
	lutask::Fiber::SetSchedulingPolicy<lutask::schedule::SharedWorkPolicy>();

    b->wait();
    lock_type lk(mtx_count);
    cnd_count.Wait(lk, []() { return 0 == fiber_count; });
}

int main()
{
    std::cout << "main thread started " << std::this_thread::get_id() << std::endl;

    //lutask::Fiber::SetSchedulingPolicy<lutask::schedule::SharedWorkPolicy>();

    for (char c : std::string("abcdefghijklmnopqrstuvwxyz")) 
    {
        //lutask::Fiber([c]() { whatevah(c); }).Detach();
        lutask::Fiber(lutask::ELaunch::Async, [c]() { whatevah(c); }).Detach();
        ++fiber_count;
    }

    thread_barrier b(2);

    std::thread threads[] = {
       std::thread(Thread, &b),
       //std::thread(Thread, &b),
       //std::thread(Thread, &b)
    };

    b.wait();
    {
        lock_type lk(mtx_count);
        cnd_count.Wait(lk, []() { return 0 == fiber_count; });
    }

    for (std::thread& t : threads) 
    {
        t.join();
    }

    return 0;
}