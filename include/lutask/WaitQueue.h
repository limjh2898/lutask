#pragma once

#include <queue>

namespace lutask
{

struct Context;
class WaitQueue final
{
public:
    WaitQueue() = default;

    void SuspendAndWait(Context* activeCtx);
    void NotifyOne();
    void NotifyAll();

    bool IsEmpty() const;

private:
    std::queue<Context*> waits_;
};

}