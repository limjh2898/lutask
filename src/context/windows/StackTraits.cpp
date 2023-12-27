#include <lutask/context/StackTraits.h>
#include <exception>

extern "C" {
#include <windows.h>
}

// x86_64
// test x86_64 before i386 because icc might
// define __i686__ for x86_64 too
#if defined(__x86_64__) || defined(__x86_64) \
    || defined(__amd64__) || defined(__amd64) \
    || defined(_M_X64) || defined(_M_AMD64)

// Windows seams not to provide a constant or function
// telling the minimal stacksize
# define MIN_STACKSIZE  8 * 1024
#else
# define MIN_STACKSIZE  4 * 1024
#endif

namespace 
{
	size_t pagesize() 
	{
		SYSTEM_INFO si;
		::GetSystemInfo(&si);
		return static_cast<std::size_t>(si.dwPageSize);
	}
}

bool lutask::StackTraits::IsUnbounded()
{
	return true;
}

std::size_t lutask::StackTraits::PageSize()
{
	static size_t size = pagesize();
	return size;
}

std::size_t lutask::StackTraits::DefaultSize()
{
	return 128 * 1024;
}

std::size_t lutask::StackTraits::MinimumSize()
{
	return MIN_STACKSIZE;
}

std::size_t lutask::StackTraits::MaximumSize()
{
	if (IsUnbounded() == false)
		throw std::exception();

	return  1 * 1024 * 1024 * 1024; // 1GB
}
