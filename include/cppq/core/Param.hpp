#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <variant>

namespace cppq {

// C++23 variant: 参数化值类型，所有值通过占位符传递，永不内联到SQL
using Param = std::variant<
    std::monostate,   // NULL
    bool,
    int32_t,
    int64_t,
    double,
    std::string
>;

// C++23 std::visit + overloaded lambda 模式匹配
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

// 将 Param 序列化为字符串（用于 FFI 层传输 / 调试）
[[nodiscard]] inline std::string param_to_string(const Param& p) {
    return std::visit(overloaded{
        [](std::monostate) -> std::string { return "NULL"; },
        [](bool v)          -> std::string { return v ? "true" : "false"; },
        [](int32_t v)       -> std::string { return std::format("{}", v); },
        [](int64_t v)       -> std::string { return std::format("{}", v); },
        [](double v)        -> std::string { return std::format("{}", v); },
        [](const std::string& v) -> std::string { return v; }
    }, p);
}

} // namespace cppq
