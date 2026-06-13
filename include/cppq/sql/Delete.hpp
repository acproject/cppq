#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/sql/Expression.hpp>

#include <format>
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

    [[nodiscard]] Query build() const {
        ParamList params;

        auto sql = std::format("DELETE FROM {}", table_);

        if (where_expr_) {
            sql += std::format(" WHERE {}", where_expr_->to_sql(params));
        }

        return {.sql = std::move(sql), .params = params.release()};
    }

private:
    std::string_view table_;
    ExprPtr where_expr_;
};

// 便捷工厂
[[nodiscard]] inline DeleteBuilder delete_from() { return {}; }

} // namespace cppq
