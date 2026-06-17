#pragma once

#include <string>
#include <string_view>

namespace cppq {

// 安全引用 PostgreSQL 标识符 (表名/列名)
// 将 ident 包裹在双引号中, 并转义内部双引号
// 例: users -> "users", my"col -> "my""col"
[[nodiscard]] inline std::string quote_ident(std::string_view ident) {
    std::string result;
    result.reserve(ident.size() + 2);
    result += '"';
    for (char c : ident) {
        if (c == '"') {
            result += "\"\"";
        } else {
            result += c;
        }
    }
    result += '"';
    return result;
}

// 安全引用字符串字面量 (用于需要内联的场景, 通常不推荐)
// 将 value 包裹在单引号中, 并转义内部单引号
[[nodiscard]] inline std::string quote_literal(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2);
    result += '\'';
    for (char c : value) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    result += '\'';
    return result;
}

} // namespace cppq
