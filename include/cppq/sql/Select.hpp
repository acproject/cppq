#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/sql/Expression.hpp>

#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppq {

enum class Order { Asc, Desc };

[[nodiscard]] inline constexpr std::string_view order_str(Order o) {
    return (o == Order::Asc) ? "ASC" : "DESC";
}

class SelectBuilder {
public:
    SelectBuilder& columns(std::vector<std::string_view> cols) {
        columns_ = std::move(cols);
        return *this;
    }

    SelectBuilder& from(std::string_view table) {
        table_ = table;
        return *this;
    }

    SelectBuilder& where(ExprPtr expr) {
        where_expr_ = std::move(expr);
        return *this;
    }

    SelectBuilder& order_by(std::string_view col, Order ord = Order::Asc) {
        order_by_.emplace_back(col, ord);
        return *this;
    }

    SelectBuilder& limit(int64_t n) {
        limit_ = n;
        return *this;
    }

    SelectBuilder& offset(int64_t n) {
        offset_ = n;
        return *this;
    }

    [[nodiscard]] Query build() const {
        ParamList params;

        // SELECT columns
        auto col_list = columns_.empty()
            ? std::string("*")
            : std::ranges::views::all(columns_)
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();

        auto sql = std::format("SELECT {}", col_list);

        // FROM
        if (!table_.empty()) {
            sql += std::format(" FROM {}", table_);
        }

        // WHERE
        if (where_expr_) {
            sql += std::format(" WHERE {}", where_expr_->to_sql(params));
        }

        // ORDER BY
        if (!order_by_.empty()) {
            std::vector<std::string> parts;
            parts.reserve(order_by_.size());
            for (const auto& [col, ord] : order_by_) {
                parts.push_back(std::format("{} {}", col, order_str(ord)));
            }
            auto joined = std::ranges::views::all(parts)
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();
            sql += std::format(" ORDER BY {}", joined);
        }

        // LIMIT
        if (limit_.has_value()) {
            sql += std::format(" LIMIT {}", params.add(Param(*limit_)));
        }

        // OFFSET
        if (offset_.has_value()) {
            sql += std::format(" OFFSET {}", params.add(Param(*offset_)));
        }

        return {.sql = std::move(sql), .params = params.release()};
    }

private:
    std::vector<std::string_view> columns_;
    std::string_view table_;
    ExprPtr where_expr_;
    std::vector<std::pair<std::string_view, Order>> order_by_;
    std::optional<int64_t> limit_;
    std::optional<int64_t> offset_;
};

// 便捷工厂
[[nodiscard]] inline SelectBuilder select() { return {}; }

} // namespace cppq
