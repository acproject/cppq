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
// JSON/JSONB 操作符表达式
// ============================================================
enum class JsonOp {
    Get,       // ->  (返回 JSON)
    GetText,   // ->> (返回 TEXT)
    Path,      // #>  (通过路径获取 JSON)
    PathText,  // #>> (通过路径获取 TEXT)
    Contains,  // @>  (包含)
    ContainedBy, // <@ (被包含)
    Exists,    // ?   (键存在)
    ExistsAny, // ?| (任一存在)
    ExistsAll, // ?& (全部存在)
};

[[nodiscard]] inline constexpr std::string_view json_op_str(JsonOp op) {
    switch (op) {
        using enum JsonOp;
        case Get:        return "->";
        case GetText:    return "->>";
        case Path:       return "#>";
        case PathText:   return "#>>";
        case Contains:   return "@>";
        case ContainedBy: return "<@";
        case Exists:     return "?";
        case ExistsAny:  return "?|";
        case ExistsAll:  return "?&";
    }
    return "->";
}

// 列 operator 参数值（如：data @> $1，data->>'key' = $1）
struct JsonOpExpr final : Expr {
    ColumnRef column;
    JsonOp op;
    Param value;

    JsonOpExpr(ColumnRef c, JsonOp o, Param v) : column(c), op(o), value(std::move(v)) {}

    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        return std::format("{} {} {}", column.name(), json_op_str(op), params.add(value));
    }
};

// JSON 字段提取表达式（用于 SELECT 或 WHERE 子条件）
// 如：data->>'name' = $1
struct JsonFieldExpr final : Expr {
    ColumnRef column;
    std::string field;
    CmpOp cmp;
    Param value;

    JsonFieldExpr(ColumnRef c, std::string f, CmpOp o, Param v)
        : column(c), field(std::move(f)), cmp(o), value(std::move(v)) {}

    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        return std::format("{}->>{} {} {}", column.name(),
            params.add(Param(field)), cmp_op_str(cmp), params.add(value));
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

// ============================================================
// JSON/JSONB 工厂函数
// ============================================================

// data @> $1 (包含)
inline ExprPtr json_contains(ColumnRef c, Param json_val) {
    return std::make_unique<JsonOpExpr>(JsonOpExpr{c, JsonOp::Contains, std::move(json_val)});
}

// data <@ $1 (被包含)
inline ExprPtr json_contained_by(ColumnRef c, Param json_val) {
    return std::make_unique<JsonOpExpr>(JsonOpExpr{c, JsonOp::ContainedBy, std::move(json_val)});
}

// data ? $1 (键存在)
inline ExprPtr json_exists(ColumnRef c, std::string key) {
    return std::make_unique<JsonOpExpr>(JsonOpExpr{c, JsonOp::Exists, Param(std::move(key))});
}

// data ?| $1 (任一键存在)
inline ExprPtr json_exists_any(ColumnRef c, std::string keys) {
    return std::make_unique<JsonOpExpr>(JsonOpExpr{c, JsonOp::ExistsAny, Param(std::move(keys))});
}

// data ?& $1 (全部键存在)
inline ExprPtr json_exists_all(ColumnRef c, std::string keys) {
    return std::make_unique<JsonOpExpr>(JsonOpExpr{c, JsonOp::ExistsAll, Param(std::move(keys))});
}

// data->>'field' = $1 (JSON 字段等于值)
inline ExprPtr json_field_eq(ColumnRef c, std::string field, Param val) {
    return std::make_unique<JsonFieldExpr>(
        JsonFieldExpr{c, std::move(field), CmpOp::Eq, std::move(val)});
}

// data->>'field' <op> $1 (JSON 字段通用比较)
inline ExprPtr json_field_cmp(ColumnRef c, std::string field, CmpOp op, Param val) {
    return std::make_unique<JsonFieldExpr>(
        JsonFieldExpr{c, std::move(field), op, std::move(val)});
}

} // namespace cppq
