#pragma once

#include <mutex>
#include <queue>
#include <memory>

namespace lutask
{

struct Context;
class WaitQueue final
{
public:
    WaitQueue() = default;

    void SuspendAndWait(Context* activeCtx);
    void SuspendAndWait(std::unique_lock<std::mutex>& lk, Context* activeCtx);
    void NotifyOne();
    void NotifyAll();

    bool IsEmpty() const;

private:
    std::queue<Context*> waits_;
};

}