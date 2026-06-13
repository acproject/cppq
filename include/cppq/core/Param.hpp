#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <variant>

namespace cppq {

// JSON/JSONB 参数包装：区分普通字符串和 JSON 数据
struct JsonParam {
    std::string data;   // JSON 字符串，如 {"name":"Alice"}
    bool is_jsonb = false; // true = jsonb (OID 3802), false = json (OID 114)
};

// C++23 variant: 参数化值类型，所有值通过占位符传递，永不内联到SQL
using Param = std::variant<
    std::monostate,   // NULL
    bool,
    int32_t,
    int64_t,
    double,
    std::string,
    JsonParam         // JSON / JSONB
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
        [](const std::string& v) -> std::string { return v; },
        [](const JsonParam& v) -> std::string { return v.data; }
    }, p);
}

// 便捷工厂
[[nodiscard]] inline Param json(std::string data) {
    return Param(JsonParam{.data = std::move(data), .is_jsonb = false});
}
[[nodiscard]] inline Param jsonb(std::string data) {
    return Param(JsonParam{.data = std::move(data), .is_jsonb = true});
}

} // namespace cppq
