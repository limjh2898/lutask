#include <cstddef>

namespace lutask 
{
	struct StackTraits
	{
		static bool IsUnbounded();
		static std::size_t PageSize();
		static std::size_t DefaultSize();
		static std::size_t MinimumSize();
		static std::size_t MaximumSize();
	};
}