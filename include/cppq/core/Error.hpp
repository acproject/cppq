#pragma once

#include <cstdint>
#include <string>

namespace cppq {

enum class ErrorCode {
    ConnectionFailed,
    QueryFailed,
    InvalidParam,
    SyntaxError
};

struct CppqError {
    ErrorCode code;
    std::string message;
};

} // namespace cppq
