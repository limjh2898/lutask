#pragma once

#include <cassert>
#include <mutex>
#include <lutask/WaitQueue.h>
#include <lutask/Context.h>

namespace lutask
{
class ConditionVariableAny
{
private:
    std::mutex  m_;
	WaitQueue	waitQueue_;

public:
	ConditionVariableAny() = default;
	~ConditionVariableAny() { assert(waitQueue_.IsEmpty()); }

	ConditionVariableAny(ConditionVariableAny const&) = delete;
	ConditionVariableAny& operator=(ConditionVariableAny const&) = delete;

    void NotifyOne() noexcept
    {
        std::unique_lock<std::mutex> lock(m_);
        waitQueue_.NotifyOne();
    }

    void NotifyAll() noexcept
    {
        std::unique_lock<std::mutex> lock(m_);
        waitQueue_.NotifyAll();
    }

    template< typename LockType >
    void Wait(LockType& lt) 
    {
        Context* active_ctx = Context::Active();
        std::unique_lock<std::mutex> lk(m_);

        lt.unlock();
        waitQueue_.SuspendAndWait(lk, active_ctx);
        try 
        {
            lt.lock();
        }
        catch (...) 
        {
            std::terminate();
        }
    }

    template< typename LockType, typename Pred >
    void Wait(LockType& lt, Pred pred) 
    {
        while (!pred()) 
        {
            Wait(lt);
        }
    }
};
}