#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Param.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/sql/Expression.hpp>

#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppq {

class UpdateBuilder {
public:
    explicit UpdateBuilder(std::string_view table) : table_(table) {}

    UpdateBuilder& set(std::string_view col, Param val) {
        sets_.emplace_back(col, std::move(val));
        return *this;
    }

    UpdateBuilder& where(ExprPtr expr) {
        where_expr_ = std::move(expr);
        return *this;
    }

    [[nodiscard]] Query build() const {
        ParamList params;

        // SET clauses
        std::vector<std::string> set_parts;
        set_parts.reserve(sets_.size());
        for (const auto& [col, val] : sets_) {
            set_parts.push_back(std::format("{}={}", col, params.add(val)));
        }
        auto set_clause = std::ranges::views::all(set_parts)
            | std::views::join_with(std::string_view(", "))
            | std::ranges::to<std::string>();

        auto sql = std::format("UPDATE {} SET {}", table_, set_clause);

        // WHERE
        if (where_expr_) {
            sql += std::format(" WHERE {}", where_expr_->to_sql(params));
        }

        return {.sql = std::move(sql), .params = params.release()};
    }

private:
    std::string_view table_;
    std::vector<std::pair<std::string_view, Param>> sets_;
    ExprPtr where_expr_;
};

// 便捷工厂
[[nodiscard]] inline UpdateBuilder update(std::string_view table) {
    return UpdateBuilder(table);
}

} // namespace cppq
