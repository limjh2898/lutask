#pragma once

//extern "C" {
//#include <windows.h>
//}
//
//#include <cstddef>
//#include <exception>
//#include <lutask/context/StackContext.h>
//#include <lutask/context/StackTraits.h>
//
//namespace lutask {
//namespace context {
//	template<typename _StackTraitsTy>
//	class BaseFixedSizeStack 
//	{
//		using StackTraitsType = _StackTraitsTy;
//
//	private: 
//	 	size_t size_;
//
//	public:
//		BaseFixedSizeStack(size_t size = StackTraitsType::DefaultSize())
//			: size_(size) {}
//
//		StackContext Allocate()
//		{
//			const size_t pages = (size_ + StackTraitsType::PageSize() - 1) / StackTraitsType::PageSize();
//			const size_t size = (pages + 1) * StackTraitsType::PageSize();
//
//			void* vp = ::VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
//			if (!vp) throw std::bad_alloc();
//
//			DWORD oldOptioins;
//			const BOOL result = ::VirtualProtect(
//				vp, StackTraitsType::PageSize(), PAGE_READWRITE | PAGE_GUARD, &oldOptioins);
//
//			if (result == FALSE)
//				throw std::exception();
//
//			StackContext sctx;
//			sctx.Size = size;
//			sctx.Sp = static_cast<char*>(vp) + sctx.Size;
//			return sctx;
//		}
//
//		void Deallocate(StackContext& sctx) 
//		{
//			if (sctx.Sp == nullptr)
//				throw std::exception();
//
//			void* vp = static_cast<char*>(sctx.Sp) - sctx.Size;
//			::VirtualFree(vp, 0, MEM_RELEASE);
//		}
//
//	};
//
//	using FixedSizeStack = BaseFixedSizeStack<StackTraits>;
//}}