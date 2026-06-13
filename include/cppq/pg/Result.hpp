#pragma once

#include <libpq-fe.h>

#include <optional>
#include <string>

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

    // 获取列名
    [[nodiscard]] std::string col_name(int col) const {
        if (!res_) return "";
        return std::string(PQfname(res_, col));
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

private:
    PGresult* res_ = nullptr;
};

} // namespace cppq
