#include <lutask/Fiber.h>
#include <lutask/Scheduler.h>
#include <lutask/Exceptions.h>

namespace lutask
{

void Fiber::_Start() noexcept {
    Context* ctx = Context::Active();
    ctx->Attach(impl_);
    switch (impl_->GetType())
    {
    case ELaunch::Post:
        ctx->GetScheduler()->Schedule(impl_);
        break;
    case ELaunch::Dispatch:
        impl_->Resume(ctx);
        break;
    case ELaunch::Async:

        break;
    default:
        assert(false && "unknown launch-policy");
    }
}

void Fiber::Join()
{
    if (Context::Active() == impl_)
    {
        throw FiberError(std::make_error_code(std::errc::resource_deadlock_would_occur), 
            "lutask: trying to join itself");
    }
    if (!Joinable())
    {
        throw FiberError(std::make_error_code(std::errc::invalid_argument),
            "lutask: not joinable");
    }
    impl_->Join();
    impl_ = nullptr;
}

void Fiber::Detach()
{
    if (!Joinable())
    {
        throw FiberError(std::make_error_code(std::errc::invalid_argument),
            "lutask: not joinable");
    }

    lutask::context::FiberContext c = std::move(impl_->c_);
    std::move(c).Resume();

    //impl_->~Context();
    impl_ = nullptr;
}

}