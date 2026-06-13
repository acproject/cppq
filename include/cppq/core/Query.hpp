#pragma once

#include <cppq/core/Param.hpp>

#include <string>
#include <vector>

namespace cppq {

// 参数化查询结果：SQL + 参数列表，值永远不会内联到SQL中
struct Query {
    std::string sql;              // e.g. "SELECT ... WHERE phone=$1"
    std::vector<Param> params;    // e.g. ["13800001234"]
};

} // namespace cppq
