#pragma once

#include <cppq/core/Column.hpp>

#include <string_view>

namespace cppq {

// C++23 string_view + designated initializers
struct TableDef {
    std::string_view name;
    std::string_view alias = "";

    [[nodiscard]] constexpr ColumnRef column(std::string_view col_name) const {
        return {.table_name = name, .column_name = col_name};
    }
};

} // namespace cppq
