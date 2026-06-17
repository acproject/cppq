#pragma once

#include <format>
#include <string>
#include <string_view>

namespace cppq {

// ============================================================
// 聚合函数助手: 生成 SQL 聚合表达式字符串
// 用于 SelectBuilder::columns() 中
// 例: select().columns({count("*"), "user_id"}).from("t").group_by({"user_id"})
// ============================================================

[[nodiscard]] inline std::string count(std::string_view col = "*") {
    return std::format("COUNT({})", col);
}

[[nodiscard]] inline std::string sum(std::string_view col) {
    return std::format("SUM({})", col);
}

[[nodiscard]] inline std::string avg(std::string_view col) {
    return std::format("AVG({})", col);
}

[[nodiscard]] inline std::string min(std::string_view col) {
    return std::format("MIN({})", col);
}

[[nodiscard]] inline std::string max(std::string_view col) {
    return std::format("MAX({})", col);
}

// 带别名: count("*") AS cnt
[[nodiscard]] inline std::string count_as(std::string_view col, std::string_view alias) {
    return std::format("COUNT({}) AS {}", col, alias);
}

[[nodiscard]] inline std::string sum_as(std::string_view col, std::string_view alias) {
    return std::format("SUM({}) AS {}", col, alias);
}

[[nodiscard]] inline std::string avg_as(std::string_view col, std::string_view alias) {
    return std::format("AVG({}) AS {}", col, alias);
}

} // namespace cppq
