#pragma once
#include <lutask/context/FixedSizeStack.h>

namespace lutask
{
struct Preallocated {
    void* sp;
    std::size_t     size;
    lutask::context::FixedSizeStack   sctx;

    Preallocated(void* sp_, std::size_t size_, lutask::context::FixedSizeStack sctx_) noexcept :
        sp(sp_), size(size_), sctx(sctx_) {
    }
};
}