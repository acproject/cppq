#pragma once

#include <libpq-fe.h>

#include <charconv>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace cppq {

// C++23: libpq 结果封装，使用 std::optional 表示可空值
class Result {
public:
    explicit Result(PGresult* res) : res_(res) {}

    ~Result() {
        if (res_) {
            PQclear(res_);
        }
    }

    // Move-only
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&& other) noexcept : res_(other.res_) { other.res_ = nullptr; }
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            if (res_) PQclear(res_);
            res_ = other.res_;
            other.res_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] int rows() const {
        return res_ ? PQntuples(res_) : 0;
    }

    [[nodiscard]] int cols() const {
        return res_ ? PQnfields(res_) : 0;
    }

    // C++23 std::optional: 返回可空结果值
    [[nodiscard]] std::optional<std::string> get(int row, int col) const {
        if (!res_ || PQgetisnull(res_, row, col)) {
            return std::nullopt;
        }
        return std::string(PQgetvalue(res_, row, col));
    }

    [[nodiscard]] std::string get_or(int row, int col, std::string default_val) const {
        auto val = get(row, col);
        return val.value_or(std::move(default_val));
    }

    // 返回 libpq 内部指针（FFI 用），指针在 PQclear 前一直有效，无需拷贝
    [[nodiscard]] const char* get_raw(int row, int col) const {
        if (!res_ || row < 0 || col < 0 || row >= PQntuples(res_) || col >= PQnfields(res_)) {
            return nullptr;
        }
        if (PQgetisnull(res_, row, col)) {
            return nullptr;
        }
        return PQgetvalue(res_, row, col);
    }

    // ---- 类型化 getter (C++23 std::from_chars) ----

    [[nodiscard]] std::optional<int32_t> get_int32(int row, int col) const {
        const char* raw = get_raw(row, col);
        if (!raw) return std::nullopt;
        int32_t val{};
        auto [ptr, ec] = std::from_chars(raw, raw + std::strlen(raw), val);
        if (ec != std::errc{}) return std::nullopt;
        return val;
    }

    [[nodiscard]] std::optional<int64_t> get_int64(int row, int col) const {
        const char* raw = get_raw(row, col);
        if (!raw) return std::nullopt;
        int64_t val{};
        auto [ptr, ec] = std::from_chars(raw, raw + std::strlen(raw), val);
        if (ec != std::errc{}) return std::nullopt;
        return val;
    }

    [[nodiscard]] std::optional<double> get_double(int row, int col) const {
        const char* raw = get_raw(row, col);
        if (!raw) return std::nullopt;
        // std::from_chars for double may not be available on all compilers;
        // fallback to std::stod
        try {
            size_t pos{};
            double val = std::stod(std::string(raw), &pos);
            return val;
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::optional<bool> get_bool(int row, int col) const {
        const char* raw = get_raw(row, col);
        if (!raw) return std::nullopt;
        // PostgreSQL boolean: t/f, true/false, 1/0
        char c = raw[0];
        return (c == 't' || c == 'T' || c == '1');
    }

    // 获取列名
    [[nodiscard]] std::string col_name(int col) const {
        if (!res_) return "";
        return std::string(PQfname(res_, col));
    }

    // 直接返回列名指针（FFI 用，在 PQclear 前有效）
    [[nodiscard]] const char* col_name_raw(int col) const {
        if (!res_) return "";
        return PQfname(res_, col);
    }

    // 获取列的 PostgreSQL 类型 OID
    [[nodiscard]] unsigned int col_type(int col) const {
        if (!res_) return 0;
        return PQftype(res_, col);
    }

    // 判断列是否为 JSON 类型 (OID 114)
    [[nodiscard]] bool is_json(int col) const {
        return col_type(col) == 114;
    }

    // 判断列是否为 JSONB 类型 (OID 3802)
    [[nodiscard]] bool is_jsonb(int col) const {
        return col_type(col) == 3802;
    }

    // 判断列是否为 JSON 或 JSONB
    [[nodiscard]] bool is_json_type(int col) const {
        auto t = col_type(col);
        return t == 114 || t == 3802;
    }

    // 影响行数（用于 INSERT/UPDATE/DELETE，即 PGRES_COMMAND_OK 时）
    [[nodiscard]] std::string affected_rows() const {
        if (!res_) return "0";
        char* tuples = PQcmdTuples(res_);
        if (!tuples || tuples[0] == '\0') return "0";
        return std::string(tuples);
    }

    // ---- 行迭代器支持 ----

    // 行代理: 提供按列访问的单行视图
    class Row {
        const Result& result_;
        int row_;
    public:
        Row(const Result& r, int row) : result_(r), row_(row) {}

        [[nodiscard]] std::optional<std::string> get(int col) const {
            return result_.get(row_, col);
        }
        [[nodiscard]] const char* get_raw(int col) const {
            return result_.get_raw(row_, col);
        }
        [[nodiscard]] std::optional<int32_t> get_int32(int col) const {
            return result_.get_int32(row_, col);
        }
        [[nodiscard]] std::optional<int64_t> get_int64(int col) const {
            return result_.get_int64(row_, col);
        }
        [[nodiscard]] std::optional<double> get_double(int col) const {
            return result_.get_double(row_, col);
        }
        [[nodiscard]] std::optional<bool> get_bool(int col) const {
            return result_.get_bool(row_, col);
        }
        [[nodiscard]] std::string get_or(int col, std::string default_val) const {
            return result_.get_or(row_, col, std::move(default_val));
        }
        [[nodiscard]] int col_count() const { return result_.cols(); }
        [[nodiscard]] std::string col_name(int col) const { return result_.col_name(col); }
        [[nodiscard]] bool is_json_type(int col) const { return result_.is_json_type(col); }

        // 按列名查找列索引
        [[nodiscard]] int col_index(std::string_view name) const {
            for (int i = 0; i < result_.cols(); ++i) {
                if (result_.col_name(i) == name) return i;
            }
            return -1;
        }

        // 按列名获取值
        [[nodiscard]] std::optional<std::string> by(std::string_view name) const {
            int idx = col_index(name);
            if (idx < 0) return std::nullopt;
            return get(idx);
        }
    };

    // 迭代器
    class Iterator {
        const Result& result_;
        int current_;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Row;
        using difference_type = int;
        using pointer = Row*;
        using reference = Row;

        Iterator(const Result& r, int idx) : result_(r), current_(idx) {}

        reference operator*() const { return Row(result_, current_); }
        Iterator& operator++() { ++current_; return *this; }
        Iterator operator++(int) { auto tmp = *this; ++current_; return tmp; }
        bool operator==(const Iterator& o) const { return current_ == o.current_; }
        bool operator!=(const Iterator& o) const { return current_ != o.current_; }
    };

    [[nodiscard]] Iterator begin() const { return Iterator(*this, 0); }
    [[nodiscard]] Iterator end() const { return Iterator(*this, rows()); }

private:
    PGresult* res_ = nullptr;
};

} // namespace cppq
