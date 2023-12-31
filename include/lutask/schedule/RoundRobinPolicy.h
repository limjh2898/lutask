#pragma once

#include <mutex>
#include <queue>

#include <lutask/schedule/IPolicy.h>

namespace lutask{
class Context;
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

    void Awakened(Context* context) noexcept override;
    Context* PickNext() noexcept override;
    bool HasReadyFibers() const noexcept override;
    void SuspendUntil(TimePoint const&) noexcept override;
    void Notify() noexcept override;
};

}}