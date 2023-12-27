#pragma once

#ifdef BOOST_CONTEXT_DECL
# undef BOOST_CONTEXT_DECL
#endif


#if ! defined(BOOST_CONTEXT_DECL)
# define BOOST_CONTEXT_DECL
#endif

#if (defined(BOOST_ALL_DYN_LINK) || defined(BOOST_CONTEXT_DYN_LINK) ) && ! defined(BOOST_CONTEXT_STATIC_LINK)
# if defined(BOOST_CONTEXT_SOURCE)
#  define BOOST_CONTEXT_DECL BOOST_SYMBOL_EXPORT
#  define BOOST_CONTEXT_BUILD_DLL
# else
#  define BOOST_CONTEXT_DECL BOOST_SYMBOL_IMPORT
# endif
#endif

#undef BOOST_CONTEXT_CALLDECL
#if (defined(i386) || defined(__i386__) || defined(__i386) \
     || defined(__i486__) || defined(__i586__) || defined(__i686__) \
     || defined(__X86__) || defined(_X86_) || defined(__THW_INTEL__) \
     || defined(__I86__) || defined(__INTEL__) || defined(__IA32__) \
     || defined(_M_IX86) || defined(_I86_)) && defined(_WIN32)
# define BOOST_CONTEXT_CALLDECL __cdecl
#else
# define BOOST_CONTEXT_CALLDECL
#endif

#if defined(__CET__) && defined(__unix__)
#  include <cet.h>
#  include <sys/mman.h>
#  define SHSTK_ENABLED (__CET__ & 0x2)
#  define USE_CONTEXT_SHADOW_STACK (SHSTK_ENABLED && SHADOW_STACK_SYSCALL)
#  define __NR_map_shadow_stack 451
#ifndef SHADOW_STACK_SET_TOKEN
#  define SHADOW_STACK_SET_TOKEN 0x1
#endif
#endif

#include <iostream>

namespace lutask { 
namespace context {
    typedef void*   fcontext_t;
    
    struct transfer_t 
    {
        fcontext_t  fctx;
        void    *   data;
    };
    
    extern "C" BOOST_CONTEXT_DECL
    transfer_t BOOST_CONTEXT_CALLDECL jump_fcontext( fcontext_t const to, void * vp);
    extern "C" BOOST_CONTEXT_DECL
    fcontext_t BOOST_CONTEXT_CALLDECL make_fcontext( void * sp, std::size_t size, void (* fn)( transfer_t) );
    
    // based on an idea of Giovanni Derreta
    extern "C" BOOST_CONTEXT_DECL
    transfer_t BOOST_CONTEXT_CALLDECL ontop_fcontext( fcontext_t const to, void * vp, transfer_t (* fn)( transfer_t) );
    
}}
