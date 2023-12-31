#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>

namespace lutask{

class Context;

namespace schedule{

class IPolicy 
{
    using TimePoint = std::chrono::steady_clock::time_point;

public:
    virtual ~IPolicy() = default;
    virtual void Awakened( Context *) noexcept = 0;
    virtual Context * PickNext() noexcept = 0;
    virtual bool HasReadyFibers() const noexcept = 0;
    virtual void SuspendUntil(TimePoint const&) noexcept = 0;
    virtual void Notify() noexcept = 0;
};

}}