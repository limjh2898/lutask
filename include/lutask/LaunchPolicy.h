#pragma once

namespace lutask
{
enum class ELaunch {
	Dispatch,
	Post,
	Async
};

template<typename Fn>
struct IsLaunchPolicy : public std::false_type {};

template<>
struct IsLaunchPolicy<ELaunch> : public std::true_type {};
}