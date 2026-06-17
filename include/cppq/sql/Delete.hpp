#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Error.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/sql/Expression.hpp>

#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace cppq {

class DeleteBuilder {
public:
    DeleteBuilder& from(std::string_view table) {
        table_ = table;
        return *this;
    }

    DeleteBuilder& where(ExprPtr expr) {
        where_expr_ = std::move(expr);
        return *this;
    }

    DeleteBuilder& returning(std::vector<std::string_view> cols) {
        returning_ = std::move(cols);
        return *this;
    }

    [[nodiscard]] Query build() const {
        ParamList params;

        // 校验: table 不能为空
        if (table_.empty()) {
            throw CppqError::build_error("DELETE: table name is required");
        }

        auto sql = std::format("DELETE FROM {}", table_);

        if (where_expr_) {
            sql += std::format(" WHERE {}", where_expr_->to_sql(params));
        }

        // RETURNING
        if (!returning_.empty()) {
            auto ret_list = std::ranges::views::all(returning_)
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();
            sql += std::format(" RETURNING {}", ret_list);
        }

        return {.sql = std::move(sql), .params = params.release()};
    }

private:
    std::string_view table_;
    ExprPtr where_expr_;
    std::vector<std::string_view> returning_;
};

// 便捷工厂
[[nodiscard]] inline DeleteBuilder delete_from() { return {}; }

} // namespace cppq
