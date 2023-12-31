#include <lutask/Fiber.h>
#include <lutask/Scheduler.h>

namespace lutask
{

void Fiber::_Start() noexcept {
    Context* ctx = Context::Active();
    ctx->Attach(impl_.get());
    switch (impl_->GetType())
    {
    case ELaunch::Post:
        ctx->GetScheduler()->Schedule(impl_);
        break;
    case ELaunch::Dispatch:
        impl_->Resume(ctx);
        break;
    default:
        assert(false && "unknown launch-policy");
    }
}

}