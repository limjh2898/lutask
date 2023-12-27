#pragma once

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4702)
#endif

#include <type_traits>
#include <cassert>

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

inline
transfer_t TaskUnwind(transfer_t t) {
	throw forced_unwind(t.fctx);
	return { nullptr, nullptr };
}

template<typename _RecordTy>
transfer_t TaskExit(transfer_t t) noexcept 
{
	_RecordTy* rec = static_cast<_RecordTy*>(t.data);
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

template<typename _RecordTy>
void TaskEntry(transfer_t t) noexcept
{
	_RecordTy* rec = static_cast<_RecordTy*>(t.data);
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
	ontop_fcontext(t.fctx, rec, TaskExit<_RecordTy>);
	assert(false && "context already terminated");
}

template<typename _CtxTy, typename Func>
transfer_t TaskOntop(transfer_t t) 
{
	assert(t.data != nullptr);
	auto p = *static_cast<Func*>(t.data);
	t.data = nullptr;
	_CtxTy c = p(_CtxTy{ t.fctx });
	return { std::exchange(c.fctx,nullptr), nullptr };
}

template<typename _CtxTy, typename _StackAllocTy, typename Func>
class TaskRecord
{
	using SAllocDecayType = typename std::decay<_StackAllocTy>::type;
	using TargetFunc = typename std::decay<Func>::type;
private:
	StackContext		sctx_;
	SAllocDecayType		salloc_;
	TargetFunc			fn_;

	static void Destroy(TaskRecord* p) noexcept {
		SAllocDecayType salloc = std::move(p->salloc_);
		StackContext sctx = p->sctx_;
		p->~TaskRecord();
		salloc.Deallocate(sctx);
	}

public:
	TaskRecord(StackContext sctx, _StackAllocTy&& salloc, Func&& fn) noexcept 
		: sctx_(sctx),
		salloc_(std::forward<_StackAllocTy>(salloc)),
		fn_(std::forward<Func>(fn)) {}

	TaskRecord(TaskRecord const&) = delete;
	TaskRecord& operator=(TaskRecord const&) = delete;

	void Deallocate() noexcept 
	{
		Destroy(this);
	}

	fcontext_t Run(fcontext_t fctx)
	{
		_CtxTy c = std::invoke(fn_, _CtxTy{ fctx });
		return std::exchange(c.fctx_, nullptr);
	}
};

template<typename Record, typename _StackAllocTy, typename Func>
fcontext_t CreateTask(_StackAllocTy&& salloc, Func&& fn)
{
	auto sctx = salloc.Allocate();

	// ��Ʈ���� ���� ���ڵ� �����̽� ����
	void* storage = reinterpret_cast<void*>(
		(reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sizeof(Record))) 
		& ~static_cast<uintptr_t>(0xff));

	Record* record = new (storage) Record{
		sctx, std::forward<_StackAllocTy>(salloc), std::forward<Func>(fn) };

	// ���ڵ���� �� 64bit
	void* stackTop = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(storage) - static_cast<uintptr_t>(64));
	void* stackBottom = reinterpret_cast<void*>(
		reinterpret_cast<uintptr_t>(sctx.Sp) - static_cast<uintptr_t>(sctx.Size));

	// fctx ���� ������
	const std::size_t size = reinterpret_cast<uintptr_t>(stackTop) - reinterpret_cast<uintptr_t>(stackBottom);

#if USE_CONTEXT_SHADOW_STACK // �ε��� ������ ������� �� Ȱ��
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
	const fcontext_t fctx = make_fcontext(stackTop, size, &TaskEntry< Record >);
	assert(nullptr != fctx);
	// transfer control structure to context-stack
	return jump_fcontext(fctx, record).fctx;
}

template< typename X, typename Y >
using disable_overload =
typename std::enable_if<!std::is_base_of<X, typename std::decay< Y >::type>::value>::type;

class TaskContext
{
private:
	template<typename _CtxTy, typename _StackAllocTy, typename Func>
	friend class TaskRecord;

	template<typename _CtxTy, typename Func>
	friend transfer_t TaskOntop(transfer_t);

	fcontext_t fctx_ = nullptr;

	TaskContext(fcontext_t fctx) noexcept : fctx_{ fctx } {}

public:
	TaskContext() noexcept = default;

	template<typename Func, typename = disable_overload<TaskContext, Func>>
	TaskContext(Func&& func) 
		: TaskContext{std::allocator_arg, FixedSizeStack(), std::forward<Func>(func)}
	{}

	template<typename _StackAllocTy, typename Func>
	TaskContext(std::allocator_arg_t, _StackAllocTy&& salloc, Func&& func)
		: fctx_{ CreateTask<TaskRecord<TaskContext, _StackAllocTy, Func>>(std::forward<_StackAllocTy>(salloc), std::forward<Func>(func)) }
	{}

	TaskContext(TaskContext&& other) noexcept { swap(other); }

	~TaskContext() 
	{
		if (fctx_ != nullptr)
		{
			ontop_fcontext(std::exchange(fctx_, nullptr), nullptr, TaskUnwind);
		}
	}

	TaskContext& operator=(TaskContext&& other) noexcept 
	{
		if (this != &other) 
		{
			TaskContext tmp = std::move(other);
			swap(tmp);
		}
		return *this;
	}

	TaskContext(TaskContext const& other) noexcept = delete;
	TaskContext& operator=(TaskContext const& other) noexcept = delete;

	TaskContext Resume()&& 
	{
		assert(fctx_ != nullptr);
		return { jump_fcontext(std::exchange(fctx_, nullptr), nullptr).fctx };
	}

	explicit operator bool() const noexcept { return fctx_ != nullptr; }
	bool operator!() const noexcept { return fctx_ == nullptr; }
	bool operator<(TaskContext const& other) const noexcept { return fctx_ < other.fctx_; }

	template< typename charT, class traitsT >
	friend std::basic_ostream< charT, traitsT >& operator<<(std::basic_ostream< charT, traitsT >& os, TaskContext const& other) 
	{
		if (nullptr != other.fctx_) {
			return os << other.fctx_;
		}
		else {
			return os << "{not-a-context}";
		}
	}

	void swap(TaskContext& other) noexcept {
		std::swap(fctx_, other.fctx_);
	}
};
}}

#if defined(_MSC_VER)
# pragma warning(pop)
#endif