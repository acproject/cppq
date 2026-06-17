#pragma once

#include <cppq/core/Query.hpp>

#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace cppq {

// 集合操作类型
enum class SetOp {
    Union,        // UNION (去重)
    UnionAll,     // UNION ALL (保留重复)
    Intersect,    // INTERSECT
    IntersectAll, // INTERSECT ALL
    Except,       // EXCEPT
    ExceptAll     // EXCEPT ALL
};

[[nodiscard]] inline constexpr std::string_view set_op_str(SetOp op) {
    switch (op) {
        using enum SetOp;
        case Union:        return "UNION";
        case UnionAll:     return "UNION ALL";
        case Intersect:    return "INTERSECT";
        case IntersectAll: return "INTERSECT ALL";
        case Except:       return "EXCEPT";
        case ExceptAll:    return "EXCEPT ALL";
    }
    return "UNION";
}

// 将多个查询用集合操作组合
// 例: union_all({q1, q2, q3}) -> "SELECT ... UNION ALL SELECT ... UNION ALL SELECT ..."
[[nodiscard]] inline Query set_operation(SetOp op, std::vector<Query> queries) {
    if (queries.empty()) {
        return {};
    }
    if (queries.size() == 1) {
        return std::move(queries[0]);
    }

    std::vector<Param> all_params;
    std::string sql;

    for (size_t i = 0; i < queries.size(); ++i) {
        if (i > 0) {
            sql += std::format(" {} ", set_op_str(op));
        }

        // 重编号子查询的占位符
        int offset = static_cast<int>(all_params.size());
        sql += renumber_placeholders(queries[i].sql, offset);

        // 追加参数
        for (auto& p : queries[i].params) {
            all_params.push_back(std::move(p));
        }
    }

    return {.sql = std::move(sql), .params = std::move(all_params)};
}

// 便捷工厂
[[nodiscard]] inline Query union_(std::vector<Query> queries) {
    return set_operation(SetOp::Union, std::move(queries));
}

[[nodiscard]] inline Query union_all(std::vector<Query> queries) {
    return set_operation(SetOp::UnionAll, std::move(queries));
}

[[nodiscard]] inline Query intersect(std::vector<Query> queries) {
    return set_operation(SetOp::Intersect, std::move(queries));
}

[[nodiscard]] inline Query except(std::vector<Query> queries) {
    return set_operation(SetOp::Except, std::move(queries));
}

} // namespace cppq
