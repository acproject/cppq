#pragma once

#include <cppq/core/Column.hpp>
#include <cppq/core/Error.hpp>
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

// ON CONFLICT 动作
enum class ConflictAction {
    DoNothing,  // ON CONFLICT DO NOTHING
    DoUpdate    // ON CONFLICT DO UPDATE SET ...
};

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

    // 单行 values（保持向后兼容）
    InsertBuilder& values(std::vector<Param> vals) {
        rows_.push_back(std::move(vals));
        return *this;
    }

    // 批量 values: 添加多行
    InsertBuilder& values_batch(std::vector<std::vector<Param>> batch) {
        for (auto& row : batch) {
            rows_.push_back(std::move(row));
        }
        return *this;
    }

    InsertBuilder& returning(std::vector<std::string_view> cols) {
        returning_ = std::move(cols);
        return *this;
    }

    // ON CONFLICT (col) DO NOTHING
    InsertBuilder& on_conflict_do_nothing(std::string_view col) {
        conflict_col_ = col;
        conflict_action_ = ConflictAction::DoNothing;
        return *this;
    }

    // ON CONFLICT (col) DO UPDATE SET col=expr, ...
    // conflict_col: 冲突检测列, updates: SET 子句 (column, Param)
    InsertBuilder& on_conflict_do_update(std::string_view col,
                                          std::vector<std::pair<std::string_view, Param>> updates) {
        conflict_col_ = col;
        conflict_action_ = ConflictAction::DoUpdate;
        conflict_updates_ = std::move(updates);
        return *this;
    }

    [[nodiscard]] Query build() const {
        ParamList params;

        // 校验: table 不能为空
        if (table_.empty()) {
            throw CppqError::build_error("INSERT: table name is required");
        }
        // 校验: 至少有一行 values
        if (rows_.empty()) {
            throw CppqError::build_error("INSERT: no values provided");
        }
        // 校验: 列数与值数匹配
        if (!columns_.empty()) {
            for (size_t i = 0; i < rows_.size(); ++i) {
                if (rows_[i].size() != columns_.size()) {
                    throw CppqError::build_error(std::format(
                        "INSERT: column count ({}) does not match value count ({}) in row {}",
                        columns_.size(), rows_[i].size(), i));
                }
            }
        }

        auto col_list = columns_.empty()
            ? std::string("")
            : std::format("({})",
                std::ranges::views::all(columns_)
                    | std::views::join_with(std::string_view(", "))
                    | std::ranges::to<std::string>());

        // 构建 VALUES 占位符 (支持多行)
        std::vector<std::string> row_placeholders;
        row_placeholders.reserve(rows_.size());
        for (const auto& row : rows_) {
            std::vector<std::string> ph;
            ph.reserve(row.size());
            for (const auto& v : row) {
                ph.push_back(params.add(v));
            }
            auto joined = std::ranges::views::all(ph)
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();
            row_placeholders.push_back(std::format("({})", joined));
        }

        auto val_list = std::ranges::views::all(row_placeholders)
            | std::views::join_with(std::string_view(", "))
            | std::ranges::to<std::string>();

        auto sql = std::format("INSERT INTO {} {} VALUES {}",
                               table_, col_list, val_list);

        // ON CONFLICT
        if (conflict_action_.has_value()) {
            sql += std::format(" ON CONFLICT ({})", conflict_col_);
            if (*conflict_action_ == ConflictAction::DoNothing) {
                sql += " DO NOTHING";
            } else {
                // DO UPDATE SET
                std::vector<std::string> set_parts;
                set_parts.reserve(conflict_updates_.size());
                for (const auto& [col, val] : conflict_updates_) {
                    set_parts.push_back(std::format("{}={}", col, params.add(val)));
                }
                auto set_clause = std::ranges::views::all(set_parts)
                    | std::views::join_with(std::string_view(", "))
                    | std::ranges::to<std::string>();
                sql += std::format(" DO UPDATE SET {}", set_clause);
            }
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
    std::vector<std::string_view> columns_;
    std::vector<std::vector<Param>> rows_;  // 支持多行
    std::vector<std::string_view> returning_;

    // ON CONFLICT 支持
    std::string_view conflict_col_;
    std::optional<ConflictAction> conflict_action_;
    std::vector<std::pair<std::string_view, Param>> conflict_updates_;
};

// 便捷工厂
[[nodiscard]] inline InsertBuilder insert() { return {}; }

} // namespace cppq
