#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Param.hpp>

#include <algorithm>
#include <format>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

namespace cppq {

// ============================================================
// ParamList: 管理参数编号 ($1, $2, ...)
// ============================================================
class ParamList {
    std::vector<Param> params_;

public:
    // 添加参数，返回 "$N" 占位符
    std::string add(Param value) {
        params_.push_back(std::move(value));
        return std::format("${}", params_.size());
    }

    [[nodiscard]] std::vector<Param> release() { return std::move(params_); }
    [[nodiscard]] std::size_t size() const { return params_.size(); }
};

// ============================================================
// Expr: 表达式基类
// ============================================================
struct Expr {
    [[nodiscard]] virtual std::string to_sql(ParamList& params) const = 0;
    virtual ~Expr() = default;
};

using ExprPtr = std::unique_ptr<Expr>;

// ============================================================
// 二元比较表达式
// ============================================================
enum class CmpOp { Eq, Ne, Gt, Lt, Ge, Le };

[[nodiscard]] inline constexpr std::string_view cmp_op_str(CmpOp op) {
    switch (op) {
        using enum CmpOp;
        case Eq: return "=";
        case Ne: return "<>";
        case Gt: return ">";
        case Lt: return "<";
        case Ge: return ">=";
        case Le: return "<=";
    }
    return "=";
}

struct CmpExpr final : Expr {
    ColumnRef column;
    CmpOp op;
    Param value;

    CmpExpr(ColumnRef c, CmpOp o, Param v) : column(c), op(o), value(std::move(v)) {}

    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        return std::format("{}{}{}", column.name(), cmp_op_str(op), params.add(value));
    }
};

// ============================================================
// LIKE 表达式
// ============================================================
struct LikeExpr final : Expr {
    ColumnRef column;
    std::string pattern;

    LikeExpr(ColumnRef c, std::string p) : column(c), pattern(std::move(p)) {}

    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        return std::format("{} LIKE {}", column.name(), params.add(Param(pattern)));
    }
};

// ============================================================
// IS NULL / IS NOT NULL
// ============================================================
struct IsNullExpr final : Expr {
    ColumnRef column;
    bool negated;

    IsNullExpr(ColumnRef c, bool n) : column(c), negated(n) {}

    [[nodiscard]] std::string to_sql(ParamList& /*params*/) const override {
        return std::format("{} IS {}NULL", column.name(), negated ? "NOT " : "");
    }
};

// ============================================================
// IN 表达式
// ============================================================
struct InExpr final : Expr {
    ColumnRef column;
    std::vector<Param> values;

    InExpr(ColumnRef c, std::vector<Param> v) : column(c), values(std::move(v)) {}

    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        std::vector<std::string> placeholders;
        placeholders.reserve(values.size());
        for (auto& v : values) {
            placeholders.push_back(params.add(v));
        }
        auto joined = std::ranges::views::all(placeholders)
            | std::views::join_with(std::string_view(", "))
            | std::ranges::to<std::string>();
        return std::format("{} IN ({})", column.name(), joined);
    }
};

// ============================================================
// AND / OR 逻辑组合
// ============================================================
struct AndExpr final : Expr {
    std::vector<ExprPtr> children;

    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        std::vector<std::string> parts;
        parts.reserve(children.size());
        for (const auto& child : children) {
            parts.push_back(child->to_sql(params));
        }
        auto joined = std::ranges::views::all(parts)
            | std::views::join_with(std::string_view(" AND "))
            | std::ranges::to<std::string>();
        return std::format("({})", joined);
    }
};

struct OrExpr final : Expr {
    std::vector<ExprPtr> children;

    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        std::vector<std::string> parts;
        parts.reserve(children.size());
        for (const auto& child : children) {
            parts.push_back(child->to_sql(params));
        }
        auto joined = std::ranges::views::all(parts)
            | std::views::join_with(std::string_view(" OR "))
            | std::ranges::to<std::string>();
        return std::format("({})", joined);
    }
};

// ============================================================
// 工厂函数
// ============================================================

inline ExprPtr eq(ColumnRef c, Param val) {
    return std::make_unique<CmpExpr>(CmpExpr{c, CmpOp::Eq, std::move(val)});
}
inline ExprPtr ne(ColumnRef c, Param val) {
    return std::make_unique<CmpExpr>(CmpExpr{c, CmpOp::Ne, std::move(val)});
}
inline ExprPtr gt(ColumnRef c, Param val) {
    return std::make_unique<CmpExpr>(CmpExpr{c, CmpOp::Gt, std::move(val)});
}
inline ExprPtr lt(ColumnRef c, Param val) {
    return std::make_unique<CmpExpr>(CmpExpr{c, CmpOp::Lt, std::move(val)});
}
inline ExprPtr ge(ColumnRef c, Param val) {
    return std::make_unique<CmpExpr>(CmpExpr{c, CmpOp::Ge, std::move(val)});
}
inline ExprPtr le(ColumnRef c, Param val) {
    return std::make_unique<CmpExpr>(CmpExpr{c, CmpOp::Le, std::move(val)});
}
inline ExprPtr like(ColumnRef c, std::string pattern) {
    return std::make_unique<LikeExpr>(LikeExpr{c, std::move(pattern)});
}
inline ExprPtr is_null(ColumnRef c) {
    return std::make_unique<IsNullExpr>(IsNullExpr{c, false});
}
inline ExprPtr is_not_null(ColumnRef c) {
    return std::make_unique<IsNullExpr>(IsNullExpr{c, true});
}
inline ExprPtr in(ColumnRef c, std::vector<Param> values) {
    return std::make_unique<InExpr>(InExpr{c, std::move(values)});
}
inline ExprPtr and_(ExprPtr a, ExprPtr b) {
    auto expr = std::make_unique<AndExpr>();
    expr->children.push_back(std::move(a));
    expr->children.push_back(std::move(b));
    return expr;
}
inline ExprPtr or_(ExprPtr a, ExprPtr b) {
    auto expr = std::make_unique<OrExpr>();
    expr->children.push_back(std::move(a));
    expr->children.push_back(std::move(b));
    return expr;
}

} // namespace cppq
