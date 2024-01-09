#pragma once

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4702)
#endif

#include <type_traits>
#include <cassert>

#include <lutask/Preallocated.h>
#include <lutask/context/fcontext.h>
#include <lutask/context/FixedSizeStack.h>

namespace lutask {
namespace context {

struct forced_unwind {
	fcontext_t  fctx{ nullptr };

	forced_unwind() = default;

	forced_unwind(fcontext_t fctx_) :
		fctx(fctx_) {
	}
};

inline transfer_t FiberUnwind(transfer_t t) {
	throw forced_unwind(t.fctx);
	return { nullptr, nullptr };
}

template<typename Record>
transfer_t FiberExit(transfer_t t) noexcept 
{
	Record* rec = static_cast<Record*>(t.data);
#if USE_CONTEXT_SHADOW_STACK
	// destory shadow stack
	std::size_t ss_size = *((unsigned long*)(reinterpret_cast<uintptr_t>(rec) - 16));
	long unsigned int ss_base = *((unsigned long*)(reinterpret_cast<uintptr_t>(rec) - 8));
	munmap((void*)ss_base, ss_size);
#endif
	// destroy context stack
	rec->Deallocate();
	return { nullptr, nullptr };
}

template<typename Record>
void FiberEntry(transfer_t t) noexcept
{
	Record* rec = static_cast<Record*>(t.data);
	assert(nullptr != t.fctx);
	assert(nullptr != rec);

	try
	{
		t = jump_fcontext(t.fctx, nullptr);
		t.fctx = rec->Run(t.fctx);
	}
	catch (forced_unwind const& ex) {
		t = { ex.fctx, nullptr };
	}

	assert(t.fctx != nullptr);
	ontop_fcontext(t.fctx, rec, FiberExit<Record>);
	assert(false && "context already terminated");
}

template<typename ContextTy, typename Func>
transfer_t FiberOntop(transfer_t t) 
{
	assert(t.data != nullptr);
	auto p = *static_cast<Func*>(t.data);
	t.data = nullptr;
	ContextTy c = p(ContextTy{ t.fctx });
	return { std::exchange(c.fctx_,nullptr), nullptr };
}

template<typename ContextTy, typename StackAlloc, typename Func>
class FiberRecord
{
	using SAllocDecayType = typename std::decay<StackAlloc>::type;
	using TargetFunc = typename std::decay<Func>::type;
private:
	StackContext		sctx_;
	SAllocDecayType		salloc_;
	TargetFunc			fn_;

	static void Destroy(FiberRecord* p) noexcept {
		SAllocDecayType salloc = std::move(p->salloc_);
		StackContext sctx = p->sctx_;
		p->~FiberRecord();
		salloc.Deallocate(sctx);
	}

public:
	FiberRecord(StackContext sctx, StackAlloc&& salloc, Func&& fn) noexcept 
		: sctx_(sctx),
		salloc_(std::forward<StackAlloc>(salloc)),
		fn_(std::forward<Func>(fn)) {}

	FiberRecord(FiberRecord const&) = delete;
	FiberRecord& operator=(FiberRecord const&) = delete;

	void Deallocate() noexcept 
	{
		Destroy(this);
	}

	fcontext_t Run(fcontext_t fctx)
	{
		ContextTy c = std::invoke(fn_, ContextTy{ fctx });
		return std::exchange(c.fctx_, nullptr);
	}
};

template<typename Record, typename StackAlloc, typename Fn>
fcontext_t CreateFiber(StackAlloc&& salloc, Fn&& fn)
{
	auto sctx = salloc.Allocate();

	// 컨트롤을 위한 레코드 스페이스 예약
	void* storage = reinterpret_cast<void*>(
		(reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sizeof(Record))) 
		& ~static_cast<uintptr_t>(0xff));

	Record* record = new (storage) Record{
		sctx, std::forward<StackAlloc>(salloc), std::forward<Fn>(fn) };

	// 레코드와의 갭 64bit
	void* stackTop = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(storage) - static_cast<uintptr_t>(64));
	void* stackBottom = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sctx.Size));

	// fctx 버퍼 사이즈
	const std::size_t size = reinterpret_cast<uintptr_t>(stackTop) - reinterpret_cast<uintptr_t>(stackBottom);

#if USE_CONTEXT_SHADOW_STACK // 셰도우 스택을 사용했을 때 활용
	std::size_t ss_size = size >> 5;
	// align shadow stack to 8 bytes.
	ss_size = (ss_size + 7) & ~7;
	// Todo: shadow stack occupies at least 4KB
	ss_size = (ss_size > 4096) ? size : 4096;
	// create shadow stack
	void* ss_base = (void*)syscall(__NR_map_shadow_stack, 0, ss_size, SHADOW_STACK_SET_TOKEN);
	BOOST_ASSERT(ss_base != -1);
	unsigned long ss_sp = (unsigned long)ss_base + ss_size;
	/* pass the shadow stack pointer to make_fcontext
	 i.e., link the new shadow stack with the new fcontext
	 TODO should be a better way? */
	*((unsigned long*)(reinterpret_cast<uintptr_t>(stackTop) - 8)) = ss_sp;
	/* Todo: place shadow stack info in 64byte gap */
	*((unsigned long*)(reinterpret_cast<uintptr_t>(storage) - 8)) = (unsigned long)ss_base;
	*((unsigned long*)(reinterpret_cast<uintptr_t>(storage) - 16)) = ss_size;
#endif
	const fcontext_t fctx = make_fcontext(stackTop, size, &FiberEntry< Record >);
	assert(nullptr != fctx);
	// transfer control structure to context-stack
	return jump_fcontext(fctx, record).fctx;
}

template< typename Record, typename StackAlloc, typename Fn >
fcontext_t CreateFiberWithPreallocated(Preallocated palloc, StackAlloc&& salloc, Fn&& fn)
{
	// 컨트롤을 위한 레코드 스페이스 예약
	void* storage = reinterpret_cast<void*>(
		(reinterpret_cast<uintptr_t>(palloc.Sp) - static_cast<uintptr_t>(sizeof(Record)))
		& ~static_cast<uintptr_t>(0xff));

	Record* record = new (storage) Record{
		palloc.Sctx, std::forward<StackAlloc>(salloc), std::forward<Fn>(fn) };

	// 레코드와의 갭 64bit
	void* stackTop = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(storage) - static_cast<uintptr_t>(64));
	void* stackBottom = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(palloc.Sctx.Sp) - static_cast<uintptr_t>(palloc.Sctx.Size));

	// fctx 버퍼 사이즈
	const std::size_t size = reinterpret_cast<uintptr_t>(stackTop) - reinterpret_cast<uintptr_t>(stackBottom);

#if USE_CONTEXT_SHADOW_STACK // 셰도우 스택을 사용했을 때 활용
	std::size_t ss_size = size >> 5;
	// align shadow stack to 8 bytes.
	ss_size = (ss_size + 7) & ~7;
	// Todo: shadow stack occupies at least 4KB
	ss_size = (ss_size > 4096) ? size : 4096;
	// create shadow stack
	void* ss_base = (void*)syscall(__NR_map_shadow_stack, 0, ss_size, SHADOW_STACK_SET_TOKEN);
	BOOST_ASSERT(ss_base != -1);
	unsigned long ss_sp = (unsigned long)ss_base + ss_size;
	/* pass the shadow stack pointer to make_fcontext
	 i.e., link the new shadow stack with the new fcontext
	 TODO should be a better way? */
	*((unsigned long*)(reinterpret_cast<uintptr_t>(stackTop) - 8)) = ss_sp;
	/* Todo: place shadow stack info in 64byte gap */
	*((unsigned long*)(reinterpret_cast<uintptr_t>(storage) - 8)) = (unsigned long)ss_base;
	*((unsigned long*)(reinterpret_cast<uintptr_t>(storage) - 16)) = ss_size;
#endif
	const fcontext_t fctx = make_fcontext(stackTop, size, &FiberEntry< Record >);
	assert(nullptr != fctx);
	// transfer control structure to context-stack
	return jump_fcontext(fctx, record).fctx;
}

template< typename X, typename Y >
using disable_overload =
typename std::enable_if<!std::is_base_of<X, typename std::decay< Y >::type>::value>::type;

class FiberContext
{
private:
	template<typename ContextTy, typename StackAlloc, typename Func>
	friend class FiberRecord;

	template<typename ContextTy, typename Func>
	friend transfer_t FiberOntop(transfer_t);

	fcontext_t fctx_ = nullptr;

	FiberContext(fcontext_t fctx) noexcept : fctx_{ fctx } {}

public:
	FiberContext() noexcept = default;

	template<typename Func, typename = disable_overload<FiberContext, Func>>
	FiberContext(Func&& func) 
		: FiberContext{std::allocator_arg, FixedSizeStack(), std::forward<Func>(func)}
	{}

	template<typename StackAlloc, typename Func>
	FiberContext(std::allocator_arg_t, StackAlloc&& salloc, Func&& func)
		: fctx_{ CreateFiber<FiberRecord<FiberContext, StackAlloc, Func>>(std::forward<StackAlloc>(salloc), std::forward<Func>(func)) }
	{}

	template<typename StackAlloc, typename Func>
	FiberContext(std::allocator_arg_t, Preallocated palloc, StackAlloc&& salloc, Func&& func)
		: fctx_{ CreateFiberWithPreallocated<FiberRecord<FiberContext, StackAlloc, Func>>(palloc, std::forward<StackAlloc>(salloc), std::forward<Func>(func)) }
	{}

	FiberContext(FiberContext&& other) noexcept { swap(other); }

	~FiberContext() 
	{
		if (fctx_ != nullptr)
		{
			ontop_fcontext(std::exchange(fctx_, nullptr), nullptr, FiberUnwind);
		}
	}

	FiberContext& operator=(FiberContext&& other) noexcept
	{
		if (this != &other) 
		{
			FiberContext tmp = std::move(other);
			swap(tmp);
		}
		return *this;
	}

	FiberContext(FiberContext const& other) noexcept = delete;
	FiberContext& operator=(FiberContext const& other) noexcept = delete;

	FiberContext Resume()&& 
	{
		assert(fctx_ != nullptr);
		return { jump_fcontext(std::exchange(fctx_, nullptr), nullptr).fctx };
	}

	template< typename Fn >
	FiberContext ResumeWith(Fn&& fn)&& {
		assert(nullptr != fctx_);
		auto p = std::forward< Fn >(fn);
		return { ontop_fcontext(
					std::exchange(fctx_, nullptr),
					& p, FiberOntop< FiberContext, decltype(p) >).fctx };
	}

	explicit operator bool() const noexcept { return fctx_ != nullptr; }
	bool operator!() const noexcept { return fctx_ == nullptr; }
	bool operator<(FiberContext const& other) const noexcept { return fctx_ < other.fctx_; }

	template< typename charT, class traitsT >
	friend std::basic_ostream< charT, traitsT >& operator<<(std::basic_ostream< charT, traitsT >& os, FiberContext const& other)
	{
		if (nullptr != other.fctx_) {
			return os << other.fctx_;
		}
		else {
			return os << "{not-a-context}";
		}
	}

	void swap(FiberContext& other) noexcept {
		std::swap(fctx_, other.fctx_);
	}
};
}}

#if defined(_MSC_VER)
# pragma warning(pop)
#endif
