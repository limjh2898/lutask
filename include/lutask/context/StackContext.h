#pragma once

#include <cstddef>

#define FCONTEXT_SEGMENTS 10

namespace lutask {
namespace context {

	struct StackContext
	{
		StackContext()
			: Size(0)
			, Sp(nullptr)
			, SegmentCtx()
		{}

		using SegmentContext = void*[FCONTEXT_SEGMENTS];

		size_t Size;
		void* Sp;
		SegmentContext SegmentCtx;
	};
}}