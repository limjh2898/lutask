#pragma once
#include <lutask/context/StackContext.h>

namespace lutask
{
struct Preallocated {
    void* Sp;
    std::size_t     size;
    lutask::context::StackContext   Sctx;

    Preallocated(void* sp_, std::size_t size_, lutask::context::StackContext sctx_) noexcept
        : Sp(sp_), size(size_), Sctx(sctx_) 
    { }
};
}