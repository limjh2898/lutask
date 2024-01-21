#include <lutask/Exceptions.h>

namespace lutask
{
class TaskErrorCategory : public std::error_category
{
public:
	const char* name() const noexcept override { return "lutask-task"; }

	std::error_condition default_error_condition(int ev) const noexcept override
	{
		switch (static_cast<ETaskError>(ev))
		{
		case ETaskError::AlreadyRetrived:
			return std::error_condition(static_cast<int>(ETaskError::AlreadyRetrived), TaskCategory());
		case ETaskError::AlreadySatisfied:
			return std::error_condition(static_cast<int>(ETaskError::AlreadySatisfied), TaskCategory());
		case ETaskError::NoState:
			return std::error_condition(static_cast<int>(ETaskError::NoState), TaskCategory());
		default:
			return std::error_condition(ev, *this);
		}
	}

	bool equivalent(std::error_code const& code, int condition) const noexcept override {
		return *this == code.category() &&
			static_cast<int>(default_error_condition(code.value()).value()) == condition;
	}

	std::string message(int ev) const override
	{
		switch (static_cast<ETaskError>(ev))
		{
		case ETaskError::AlreadyRetrived:
			return "the task has already been retrieved from packaged task";
		case ETaskError::AlreadySatisfied:
			return "the state of the task already been set.";
		case ETaskError::NoState:
			return "Operation not permitted on an object without an associated state.";
		default:
			return "unspecified task error value";
		}
	}
};

std::error_category const& lutask::TaskCategory() noexcept
{
	static TaskErrorCategory cat;
	return cat;
}

}