#pragma once

#include <mutex>
#include <queue>

#include <lutask/schedule/IPolicy.h>

namespace lutask{
struct Context;
namespace schedule{

class RoundRobinPolicy : public IPolicy 
{
    using TimePoint = std::chrono::steady_clock::time_point;

    std::queue<Context*>        readyQueue_{};
    std::mutex                  mtx_{};
    std::condition_variable     cnd_{};
    bool                        flag_{ false };

public:
    RoundRobinPolicy() = default;

    RoundRobinPolicy(RoundRobinPolicy const&) = delete;
    RoundRobinPolicy& operator=(RoundRobinPolicy const&) = delete;

    virtual void Awakened(Context* context) noexcept override final;
    virtual Context* PickNext() noexcept override final;
    virtual bool HasReadyFibers() const noexcept override final;
    virtual void SuspendUntil(TimePoint const&) noexcept override final;
    virtual void Notify() noexcept override final;
};

}}