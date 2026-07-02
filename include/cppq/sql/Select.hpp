#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Error.hpp>
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

// JOIN 类型
enum class JoinType {
    Inner,  // INNER JOIN
    Left,   // LEFT JOIN
    Right,  // RIGHT JOIN
    Full,   // FULL OUTER JOIN
    Cross   // CROSS JOIN
};

[[nodiscard]] inline constexpr std::string_view join_type_str(JoinType t) {
    switch (t) {
        using enum JoinType;
        case Inner: return "INNER JOIN";
        case Left:  return "LEFT JOIN";
        case Right: return "RIGHT JOIN";
        case Full:  return "FULL OUTER JOIN";
        case Cross: return "CROSS JOIN";
    }
    return "INNER JOIN";
}

// JOIN 子句定义
struct JoinClause {
    JoinType type;
    std::string_view table;
    std::string_view alias;       // 可选别名
    ExprPtr on;                   // ON 条件 (CROSS JOIN 时为空)
};

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

    // 设置 FROM 表别名: SELECT ... FROM users u
    SelectBuilder& from_alias(std::string_view alias) {
        table_alias_ = alias;
        return *this;
    }

    // FROM 子查询: SELECT ... FROM (SELECT ...) AS alias
    SelectBuilder& from_subquery(Query sub, std::string_view alias) {
        from_subquery_ = std::move(sub);
        from_subquery_alias_ = alias;
        return *this;
    }

    // CTE (WITH 子句): WITH name AS (SELECT ...) SELECT ...
    SelectBuilder& with(std::string_view name, Query sub) {
        ctes_.emplace_back(name, std::move(sub));
        return *this;
    }

    // 添加 JOIN
    SelectBuilder& join(JoinType type, std::string_view table, ExprPtr on) {
        joins_.push_back(JoinClause{
            .type = type,
            .table = table,
            .alias = "",
            .on = std::move(on)
        });
        return *this;
    }

    // 添加 JOIN 带别名
    SelectBuilder& join_as(JoinType type, std::string_view table,
                           std::string_view alias, ExprPtr on) {
        joins_.push_back(JoinClause{
            .type = type,
            .table = table,
            .alias = alias,
            .on = std::move(on)
        });
        return *this;
    }

    // 便捷方法: INNER JOIN
    SelectBuilder& inner_join(std::string_view table, ExprPtr on) {
        return join(JoinType::Inner, table, std::move(on));
    }

    // 便捷方法: LEFT JOIN
    SelectBuilder& left_join(std::string_view table, ExprPtr on) {
        return join(JoinType::Left, table, std::move(on));
    }

    SelectBuilder& where(ExprPtr expr) {
        if (where_expr_) {
            // 多个 WHERE 条件用 AND 组合, 而非覆盖
            auto combined = std::make_unique<AndExpr>();
            combined->children.push_back(std::move(where_expr_));
            combined->children.push_back(std::move(expr));
            where_expr_ = std::move(combined);
        } else {
            where_expr_ = std::move(expr);
        }
        return *this;
    }

    // GROUP BY
    SelectBuilder& group_by(std::vector<std::string_view> cols) {
        group_by_ = std::move(cols);
        return *this;
    }

    // HAVING
    SelectBuilder& having(ExprPtr expr) {
        having_expr_ = std::move(expr);
        return *this;
    }

    // DISTINCT
    SelectBuilder& distinct(bool v = true) {
        distinct_ = v;
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

        // 校验: table 或 from_subquery 至少有一个
        if (table_.empty() && !from_subquery_.has_value()) {
            throw CppqError::build_error("SELECT: table name or from_subquery is required");
        }
        // 校验: limit/offset 不能为负
        if (limit_.has_value() && *limit_ < 0) {
            throw CppqError::build_error("SELECT: limit must be non-negative");
        }
        if (offset_.has_value() && *offset_ < 0) {
            throw CppqError::build_error("SELECT: offset must be non-negative");
        }

        // CTE (WITH 子句) — 参数最先合并
        std::string sql;
        if (!ctes_.empty()) {
            std::vector<std::string> cte_parts;
            cte_parts.reserve(ctes_.size());
            for (const auto& [name, sub] : ctes_) {
                std::string inner = merge_subquery(sub, params);
                cte_parts.push_back(std::format("{} AS ({})", name, inner));
            }
            auto joined = std::ranges::views::all(cte_parts)
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();
            sql = std::format("WITH {} ", joined);
        }

        // SELECT [DISTINCT] columns
        auto col_list = columns_.empty()
            ? std::string("*")
            : std::ranges::views::all(columns_)
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();

        sql += std::format("SELECT {}{}", distinct_ ? "DISTINCT " : "", col_list);

        // FROM table [alias] 或 FROM (subquery) AS alias
        if (from_subquery_.has_value()) {
            std::string inner = merge_subquery(*from_subquery_, params);
            sql += std::format(" FROM ({}) AS {}", inner, from_subquery_alias_);
        } else {
            sql += std::format(" FROM {}", table_);
            if (!table_alias_.empty()) {
                sql += std::format(" {}", table_alias_);
            }
        }

        // JOINs
        for (const auto& j : joins_) {
            sql += std::format(" {} {}", join_type_str(j.type), j.table);
            if (!j.alias.empty()) {
                sql += std::format(" {}", j.alias);
            }
            if (j.type != JoinType::Cross && j.on) {
                sql += std::format(" ON {}", j.on->to_sql(params));
            }
        }

        // WHERE
        if (where_expr_) {
            sql += std::format(" WHERE {}", where_expr_->to_sql(params));
        }

        // GROUP BY
        if (!group_by_.empty()) {
            auto joined = std::ranges::views::all(group_by_)
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();
            sql += std::format(" GROUP BY {}", joined);
        }

        // HAVING
        if (having_expr_) {
            sql += std::format(" HAVING {}", having_expr_->to_sql(params));
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
    std::string_view table_alias_;
    std::optional<Query> from_subquery_;
    std::string_view from_subquery_alias_;
    std::vector<std::pair<std::string_view, Query>> ctes_;
    std::vector<JoinClause> joins_;
    ExprPtr where_expr_;
    std::vector<std::string_view> group_by_;
    ExprPtr having_expr_;
    bool distinct_ = false;
    std::vector<std::pair<std::string_view, Order>> order_by_;
    std::optional<int64_t> limit_;
    std::optional<int64_t> offset_;
};

// 便捷工厂
[[nodiscard]] inline SelectBuilder select() { return {}; }

} // namespace cppq
