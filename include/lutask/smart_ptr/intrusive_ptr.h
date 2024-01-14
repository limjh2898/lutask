#pragma once

#include <cassert>
#include <string>

namespace lutask
{
template<class T> 
class intrusive_ptr
{
private:

    typedef intrusive_ptr this_type;

public:

    typedef T element_type;

    constexpr intrusive_ptr() noexcept : px( 0 )
    {
    }

    intrusive_ptr( T * p, bool add_ref = true ): px( p )
    {
        if( px != 0 && add_ref ) intrusive_ptr_add_ref( px );
    }

    template<class U>
    intrusive_ptr( intrusive_ptr<U> const & rhs )
        : px( rhs.get() )
    {
        if( px != 0 ) intrusive_ptr_add_ref( px );
    }


    intrusive_ptr(intrusive_ptr const & rhs): px( rhs.px )
    {
        if( px != 0 ) intrusive_ptr_add_ref( px );
    }

    ~intrusive_ptr()
    {
        if( px != 0 ) intrusive_ptr_release( px );
    }


    intrusive_ptr(intrusive_ptr && rhs) noexcept : px( rhs.px )
    {
        rhs.px = 0;
    }

    intrusive_ptr & operator=(intrusive_ptr && rhs) noexcept
    {
        this_type( static_cast< intrusive_ptr && >( rhs ) ).swap(*this);
        return *this;
    }

    template<class U> friend class intrusive_ptr;

    template<class U>
    intrusive_ptr(intrusive_ptr<U> && rhs)     
        : px( rhs.px )
    {
        rhs.px = 0;
    }

    template<class U>
    intrusive_ptr & operator=(intrusive_ptr<U> && rhs) noexcept
    {
        this_type( static_cast< intrusive_ptr<U> && >( rhs ) ).swap(*this);
        return *this;
    }

    intrusive_ptr & operator=(intrusive_ptr const & rhs)
    {
        this_type(rhs).swap(*this);
        return *this;
    }

    intrusive_ptr & operator=(T * rhs)
    {
        this_type(rhs).swap(*this);
        return *this;
    }

    void reset()
    {
        this_type().swap( *this );
    }

    void reset( T * rhs )
    {
        this_type( rhs ).swap( *this );
    }

    void reset( T * rhs, bool add_ref )
    {
        this_type( rhs, add_ref ).swap( *this );
    }

    T * get() const noexcept
    {
        return px;
    }

    T * detach() noexcept
    {
        T * ret = px;
        px = 0;
        return ret;
    }

    T & operator*() const
    {
        assert( px != 0 );
        return *px;
    }

    T * operator->() const
    {
        assert( px != 0 );
        return px;
    }

    void swap(intrusive_ptr & rhs) noexcept
    {
        T * tmp = px;
        px = rhs.px;
        rhs.px = tmp;
    }

private:
    T * px;
};

template<class T, class U> 
inline bool operator==(intrusive_ptr<T> const & a, intrusive_ptr<U> const & b) noexcept
{
    return a.get() == b.get();
}

template<class T, class U> 
inline bool operator!=(intrusive_ptr<T> const & a, intrusive_ptr<U> const & b) noexcept
{
    return a.get() != b.get();
}

template<class T, class U> 
inline bool operator==(intrusive_ptr<T> const & a, U * b) noexcept
{
    return a.get() == b;
}

template<class T, class U> 
inline bool operator!=(intrusive_ptr<T> const & a, U * b) noexcept
{
    return a.get() != b;
}

template<class T, class U> 
inline bool operator==(T * a, intrusive_ptr<U> const & b) noexcept
{
    return a == b.get();
}

template<class T, class U> 
inline bool operator!=(T * a, intrusive_ptr<U> const & b) noexcept
{
    return a != b.get();
}

template<class T> 
inline bool operator<(intrusive_ptr<T> const & a, intrusive_ptr<T> const & b) noexcept
{
    return std::less<T *>()(a.get(), b.get());
}

template<class T> 
void swap(intrusive_ptr<T> & lhs, intrusive_ptr<T> & rhs) noexcept
{
    lhs.swap(rhs);
}

// mem_fn support

template<class T> 
T * get_pointer(intrusive_ptr<T> const & p) noexcept
{
    return p.get();
}

template<class T, class U> 
intrusive_ptr<T> static_pointer_cast(intrusive_ptr<U> const & p)
{
    return static_cast<T *>(p.get());
}

template<class T, class U> 
intrusive_ptr<T> const_pointer_cast(intrusive_ptr<U> const & p)
{
    return const_cast<T *>(p.get());
}

template<class T, class U> 
intrusive_ptr<T> dynamic_pointer_cast(intrusive_ptr<U> const & p)
{
    return dynamic_cast<T *>(p.get());
}

template< class T > struct hash;

template<class T> std::size_t hash_value(intrusive_ptr<T> const& p) noexcept
{
    return std::hash<T*>()(p.get());
}


}