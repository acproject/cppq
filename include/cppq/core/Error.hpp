#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

namespace cppq {

enum class ErrorCode {
    ConnectionFailed,
    QueryFailed,
    InvalidParam,
    SyntaxError,
    BuildError   // 构建时校验失败（列/值不匹配、空表名等）
};

// CppqError 继承 std::exception，可被 throw/catch
struct CppqError : std::exception {
    ErrorCode code = ErrorCode::SyntaxError;
    std::string message;

    CppqError() = default;
    CppqError(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}

    [[nodiscard]] const char* what() const noexcept override {
        return message.c_str();
    }

    // 便捷工厂
    [[nodiscard]] static CppqError connection_failed(std::string msg) {
        return {ErrorCode::ConnectionFailed, std::move(msg)};
    }
    [[nodiscard]] static CppqError query_failed(std::string msg) {
        return {ErrorCode::QueryFailed, std::move(msg)};
    }
    [[nodiscard]] static CppqError build_error(std::string msg) {
        return {ErrorCode::BuildError, std::move(msg)};
    }
};

} // namespace cppq
