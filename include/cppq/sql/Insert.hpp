#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Param.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/sql/Expression.hpp>

#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppq {

class InsertBuilder {
public:
    InsertBuilder& into(std::string_view table) {
        table_ = table;
        return *this;
    }

    InsertBuilder& columns(std::vector<std::string_view> cols) {
        columns_ = std::move(cols);
        return *this;
    }

    InsertBuilder& values(std::vector<Param> vals) {
        values_ = std::move(vals);
        return *this;
    }

    InsertBuilder& returning(std::vector<std::string_view> cols) {
        returning_ = std::move(cols);
        return *this;
    }

    [[nodiscard]] Query build() const {
        ParamList params;

        auto col_list = std::ranges::views::all(columns_)
            | std::views::join_with(std::string_view(", "))
            | std::ranges::to<std::string>();

        // 构建 VALUES 占位符
        std::vector<std::string> placeholders;
        placeholders.reserve(values_.size());
        for (const auto& v : values_) {
            placeholders.push_back(params.add(v));
        }
        auto val_list = std::ranges::views::all(placeholders)
            | std::views::join_with(std::string_view(", "))
            | std::ranges::to<std::string>();

        auto sql = std::format("INSERT INTO {} ({}) VALUES ({})",
                               table_, col_list, val_list);

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
    std::vector<std::string_view> columns_;
    std::vector<Param> values_;
    std::vector<std::string_view> returning_;
};

// 便捷工厂
[[nodiscard]] inline InsertBuilder insert() { return {}; }

} // namespace cppq
