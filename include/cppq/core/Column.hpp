#pragma once

#include <format>
#include <string>
#include <string_view>

namespace cppq {

// C++23 string_view 零拷贝列引用
struct ColumnRef {
    std::string_view table_name;
    std::string_view column_name;

    // C++23 std::format 拼接 "table.column"
    [[nodiscard]] std::string qualified() const {
        return std::format("{}.{}", table_name, column_name);
    }

    [[nodiscard]] constexpr std::string_view name() const { return column_name; }
};

// 便捷工厂：创建不带表名的列引用
[[nodiscard]] inline constexpr ColumnRef col(std::string_view name) {
    return {.table_name = "", .column_name = name};
}

[[nodiscard]] inline constexpr ColumnRef col(std::string_view table, std::string_view name) {
    return {.table_name = table, .column_name = name};
}

} // namespace cppq
