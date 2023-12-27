#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <lutask/context/StackTraits.h>
#include <lutask/context/StackContext.h>

namespace lutask {
namespace context {

template<typename _StackTraitsTy>
class BasicFixedSizeStack {
private:
    std::size_t     size_;

public:
    BasicFixedSizeStack( std::size_t size = _StackTraitsTy::DefaultSize() ) noexcept
        : size_( size) { }

    StackContext Allocate() {
        void * vp = std::malloc( size_);
        if ( ! vp) {
            throw std::bad_alloc();
        }
        StackContext sctx;
        sctx.Size = size_;
        sctx.Sp = static_cast< char * >( vp) + sctx.Size;
        return sctx;
    }

    void Deallocate(StackContext& sctx) noexcept {
        assert(sctx.Sp);

        void *vp = static_cast<char*>(sctx.Sp) - sctx.Size;
        std::free(vp);
    }
};

using FixedSizeStack = BasicFixedSizeStack<StackTraits>;
}}