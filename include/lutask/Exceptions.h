#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace lutask
{

class FiberError : public std::system_error 
{
public:
    explicit FiberError(std::error_code ec) : std::system_error(ec) { }
    FiberError(std::error_code ec, const char* what_arg) : std::system_error(ec, what_arg) { }
    FiberError(std::error_code ec, std::string const& what_arg) : std::system_error(ec, what_arg) { }

    ~FiberError() override = default;
};

enum class ETaskError
{
    AlreadyRetrived,
    AlreadySatisfied,
    NoState
};

std::error_category const& TaskCategory() noexcept;
}

namespace std 
{
    inline std::error_code make_error_code(lutask::ETaskError e) noexcept
    {
        return std::error_code(static_cast<int>(e), lutask::TaskCategory());
    }
}

namespace lutask
{
class TaskError : public FiberError
{
public:
    explicit TaskError(std::error_code ec) : FiberError{ ec } { }
};

class FutureUninitialized : public TaskError {
public:
    FutureUninitialized() : TaskError{ std::make_error_code(ETaskError::NoState) } { }
};

class TaskAlreadyRetrived : public TaskError
{
public:
    TaskAlreadyRetrived() : TaskError{ std::make_error_code(ETaskError::AlreadyRetrived) } { }
};

class TaskAlreadySatisfied : public TaskError
{
public:
    TaskAlreadySatisfied() : TaskError{ std::make_error_code(ETaskError::AlreadySatisfied) } { }
};

class PackagedTaskUninitialized : public TaskError
{
public:
    PackagedTaskUninitialized() : TaskError{ std::make_error_code(ETaskError::NoState) } { }
};

}