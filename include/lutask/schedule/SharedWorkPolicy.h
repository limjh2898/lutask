#pragma once

#include <mutex>
#include <queue>
#include <lutask/schedule/IPolicy.h>
#include <lutask/Context.h>

namespace lutask{
namespace schedule{

class SharedWorkPolicy : public IPolicy 
{
    using TimePoint = std::chrono::steady_clock::time_point;

    static std::queue<Context*> readyQueue_;
    static std::mutex           rqueueMutex_;

    std::queue<Context*>    localQueue_;
    std::mutex                  mtx_;
    std::condition_variable     cnd_;
    bool                        flag_ = false;
    bool                        suspend_ = false;

public:
    SharedWorkPolicy() = default;
    SharedWorkPolicy(SharedWorkPolicy const&) = delete;
    SharedWorkPolicy(SharedWorkPolicy&&) = delete;

    SharedWorkPolicy& operator=(SharedWorkPolicy const&) = delete;
    SharedWorkPolicy& operator=(SharedWorkPolicy&&) = delete;

    virtual void Awakened(Context* ctx) noexcept override final;
    virtual Context* PickNext() noexcept override final;
    virtual bool HasReadyFibers() const noexcept override final;
    virtual void SuspendUntil(TimePoint const&) noexcept override final;
    virtual void Notify() noexcept override final;

    static void AwakenedAsync(Context* ctx) noexcept;
};

}}