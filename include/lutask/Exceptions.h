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


}