#include <lutask/Fiber.h>
#include <lutask/Scheduler.h>
#include <lutask/Exceptions.h>
#include <lutask/schedule/SharedWorkPolicy.h>

namespace lutask
{

void Fiber::_Start() noexcept {
    Context* ctx = Context::Active();
    switch (impl_->GetType())
    {
    case ELaunch::Post:
        ctx->Attach(impl_.get());
        ctx->GetScheduler()->Schedule(impl_.get());
        break;
    case ELaunch::Dispatch:
        ctx->Attach(impl_.get());
        impl_->Resume(ctx);
        break;
    case ELaunch::Async:
        std::cout << "## _start thread id: " << std::this_thread::get_id() << std::endl;
        impl_.get()->originScheduler_ = ctx->GetScheduler();
        schedule::SharedWorkPolicy::AwakenedAsync(impl_.get());
        break;
    default:
        assert(false && "unknown launch-policy");
    }
}

void Fiber::Join()
{
    if (Context::Active() == impl_.get())
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
    impl_.reset();
}

void Fiber::Detach()
{
    if (!Joinable())
    {
        throw FiberError(std::make_error_code(std::errc::invalid_argument),
            "lutask: not joinable");
    }
    impl_.reset();
}

}