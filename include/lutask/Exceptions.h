#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace lutask
{

class TaskError : public std::system_error 
{
    explicit TaskError(std::error_code ec) : std::system_error(ec) { }
    TaskError(std::error_code ec, const char* what_arg) : std::system_error(ec, what_arg) { }
    TaskError(std::error_code ec, std::string const& what_arg) : std::system_error(ec, what_arg) { }

    ~TaskError() override = default;
};


}