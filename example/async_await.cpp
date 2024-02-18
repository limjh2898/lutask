#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <concurrent_queue.h>
#include <lutask/future/Async.h>
#include <lutask/ConditionVariableAny.h>
#include <lutask/schedule/SharedWorkPolicy.h>
#include <lutask/smart_ptr/intrusive_ptr.h>
#include <lutask/Exceptions.h>

static std::atomic_size_t fiber_count{ 0 };
static std::mutex mtx_count{};
static lutask::ConditionVariableAny cnd_count;
typedef std::unique_lock< std::mutex > lock_type;
bool end = false;

static std::atomic_llong delayTime{ 1000 };

std::thread::id fn(std::string str, int n)
{
    if (delayTime.load() != 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayTime.load()));
        delayTime.fetch_sub(100);
    }
    return std::this_thread::get_id();
}

struct Handler
{
    Handler(int num) : num_(num), startThread(std::this_thread::get_id()) {}

    void Run()
    {
        for (auto f : futures)
        {
            if (f->IsReady() == false)
                continue;

            auto result = reinterpret_cast<lutask::Future<std::thread::id>*>(&f);

            std::cout 
                << "start: " << startThread 
                << "\tproc: " << result 
                << "\tend: " << std::this_thread::get_id() 
                << "\tnum: " << num_ << std::endl;

            lock_type lk(mtx_count);
            if (0 == fiber_count.fetch_sub(1) && end == true)
            {
                lk.unlock();
                cnd_count.NotifyAll();
            }
        }

        auto future = async_await(fn, "abc", 1);

        futures.push_back(reinterpret_cast<lutask::IFuture*>(&future));

        //std::cout 
        //    << "start: " << startThread 
        //    << "\tproc: " << result 
        //    << "\tend: " << std::this_thread::get_id() 
        //    << "\tnum: " << num_ << std::endl;

       // Destroy(this);
    }

    static void Destroy(Handler* handler)
    {
        delete handler;
        handler = nullptr;
    }

    static std::vector<lutask::IFuture*> futures;

    int num_;
    std::thread::id startThread;
};

std::vector<lutask::IFuture*> Handler::futures{};

void producer()
{
    std::cout << "producer started " << std::this_thread::get_id() << std::endl;

    for (int i = 0; i < 10; i++)
    {   
        Handler* handler = new Handler(i);
        lutask::Fiber(std::bind(&Handler::Run, handler)).Detach();
        fiber_count.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    end = true;
}

void consumer()
{
    std::cout << "consumer started " << std::this_thread::get_id() << std::endl;
    lutask::Fiber::SetSchedulingPolicy<lutask::schedule::SharedWorkPolicy>();

    lock_type lk(mtx_count);
    cnd_count.Wait(lk, []() { return fiber_count == 0; });
}

int main()
{
    try
    {
        std::thread producerThread(producer);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::thread threads[] = {
            std::thread(consumer),
            std::thread(consumer),
            std::thread(consumer),
            std::thread(consumer)
        };

        {
            lock_type lk(mtx_count);
            cnd_count.Wait(lk, []() { return fiber_count == 0; });
        }

        for (std::thread& t : threads)
        {
            t.join();
        }

        producerThread.join();

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