#pragma once

#include <cppq/core/Param.hpp>

#include <cctype>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace cppq {

// 参数化查询结果：SQL + 参数列表，值永远不会内联到SQL中
struct Query {
    std::string sql;              // e.g. "SELECT ... WHERE phone=$1"
    std::vector<Param> params;    // e.g. ["13800001234"]
};

// 将子查询的 SQL 占位符重编号 ($1 -> $offset+1, $2 -> $offset+2, ...)
// 用于将子查询嵌入父查询时保持参数编号连续
[[nodiscard]] inline std::string renumber_placeholders(std::string_view sql, int offset) {
    std::string result;
    result.reserve(sql.size());
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == '$' && i + 1 < sql.size() && std::isdigit(static_cast<unsigned char>(sql[i + 1]))) {
            size_t j = i + 1;
            int num = 0;
            while (j < sql.size() && std::isdigit(static_cast<unsigned char>(sql[j]))) {
                num = num * 10 + (sql[j] - '0');
                ++j;
            }
            result += std::format("${}", num + offset);
            i = j - 1;
        } else {
            result += sql[i];
        }
    }
    return result;
}

} // namespace cppq
