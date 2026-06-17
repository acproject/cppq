#pragma once

#include <cppq/core/Error.hpp>
#include <cppq/core/Param.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/pg/Result.hpp>

#include <expected>
#include <libpq-fe.h>
#include <string>
#include <utility>
#include <vector>

namespace cppq {

// PostgreSQL 类型 OID 常量
constexpr unsigned int PG_OID_JSON  = 114;
constexpr unsigned int PG_OID_JSONB = 3802;
constexpr unsigned int PG_OID_TEXT  = 25;

// C++23 std::expected: 错误处理替代异常
class Connection {
public:
    explicit Connection(std::string conn_info)
        : conn_info_(std::move(conn_info)) {}

    ~Connection() { disconnect(); }

    // Move-only
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&& other) noexcept
        : conn_(other.conn_), conn_info_(std::move(other.conn_info_)) {
        other.conn_ = nullptr;
    }
    Connection& operator=(Connection&& other) noexcept {
        if (this != &other) {
            disconnect();
            conn_ = other.conn_;
            conn_info_ = std::move(other.conn_info_);
            other.conn_ = nullptr;
        }
        return *this;
    }

    // C++23 std::expected: 连接成功返回void，失败返回CppqError
    [[nodiscard]] std::expected<void, CppqError> connect() {
        conn_ = PQconnectdb(conn_info_.c_str());
        if (PQstatus(conn_) != CONNECTION_OK) {
            std::string msg = PQerrorMessage(conn_);
            PQfinish(conn_);
            conn_ = nullptr;
            return std::unexpected(CppqError::connection_failed(std::move(msg)));
        }
        return {};
    }

    void disconnect() {
        if (conn_) {
            PQfinish(conn_);
            conn_ = nullptr;
        }
    }

    // 执行参数化查询 - 核心方法
    // 所有值通过 $1, $2... 占位符传递，防止 SQL 注入
    [[nodiscard]] std::expected<Result, CppqError> execute(const Query& query) {
        if (!conn_) {
            return std::unexpected(CppqError::connection_failed("Not connected"));
        }

        // 将 Param 转换为 libpq 需要的 C 字符串数组
        // std::monostate -> nullptr (SQL NULL)
        // JsonParam -> 指定正确的 OID (json=114, jsonb=3802)
        std::vector<std::string> str_params;
        std::vector<const char*> c_params;
        std::vector<unsigned int> param_types;
        str_params.reserve(query.params.size());
        c_params.reserve(query.params.size());
        param_types.reserve(query.params.size());

        bool has_json = false;
        for (const auto& p : query.params) {
            if (std::holds_alternative<std::monostate>(p)) {
                str_params.emplace_back();
                c_params.push_back(nullptr);
                param_types.push_back(0); // let server infer
            } else if (auto* jp = std::get_if<JsonParam>(&p)) {
                str_params.push_back(jp->data);
                c_params.push_back(str_params.back().c_str());
                param_types.push_back(jp->is_jsonb ? PG_OID_JSONB : PG_OID_JSON);
                has_json = true;
            } else {
                str_params.push_back(param_to_string(p));
                c_params.push_back(str_params.back().c_str());
                param_types.push_back(0); // let server infer
            }
        }

        // 只在有 JSON 参数时才传 paramTypes，否则让服务器推断
        const unsigned int* types_ptr = has_json ? param_types.data() : nullptr;

        PGresult* res = PQexecParams(
            conn_,
            query.sql.c_str(),
            static_cast<int>(c_params.size()),
            types_ptr,
            c_params.data(),
            nullptr,  // paramLengths
            nullptr,  // paramFormats (text)
            0         // resultFormat (text)
        );

        if (!res) {
            return std::unexpected(CppqError::query_failed("PQexecParams returned null"));
        }

        auto status = PQresultStatus(res);
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            std::string msg = PQresultErrorMessage(res);
            PQclear(res);
            return std::unexpected(CppqError::query_failed(std::move(msg)));
        }

        return Result(res);
    }

    // 事务支持
    [[nodiscard]] std::expected<void, CppqError> begin() {
        return exec_simple("BEGIN");
    }

    [[nodiscard]] std::expected<void, CppqError> commit() {
        return exec_simple("COMMIT");
    }

    [[nodiscard]] std::expected<void, CppqError> rollback() {
        return exec_simple("ROLLBACK");
    }

    [[nodiscard]] bool is_connected() const {
        return conn_ && PQstatus(conn_) == CONNECTION_OK;
    }

private:
    [[nodiscard]] std::expected<void, CppqError> exec_simple(const char* sql) {
        if (!conn_) {
            return std::unexpected(CppqError::connection_failed("Not connected"));
        }
        PGresult* res = PQexec(conn_, sql);
        if (!res) {
            // PQexec 返回 null 表示严重错误（如内存不足）
            return std::unexpected(CppqError::query_failed(PQerrorMessage(conn_)));
        }
        auto status = PQresultStatus(res);
        PQclear(res);
        if (status != PGRES_COMMAND_OK) {
            return std::unexpected(CppqError::query_failed(PQerrorMessage(conn_)));
        }
        return {};
    }

    PGconn* conn_ = nullptr;
    std::string conn_info_;
};

// ============================================================
// RAII 事务守卫: 作用域内自动管理事务
// 构造时 BEGIN, 显式 commit() 后正常析构
// 未 commit 就析构 (含异常展开) 则自动 ROLLBACK
// ============================================================
class Transaction {
    Connection& conn_;
    bool active_ = true;

public:
    explicit Transaction(Connection& conn) : conn_(conn) {
        (void)conn_.begin();
    }

    ~Transaction() {
        if (active_) {
            (void)conn_.rollback();  // 异常或忘记 commit 时自动回滚
        }
    }

    // 不可拷贝、不可移动
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;

    // 显式提交
    [[nodiscard]] std::expected<void, CppqError> commit() {
        auto r = conn_.commit();
        if (r.has_value()) {
            active_ = false;
        }
        return r;
    }

    // 显式回滚
    [[nodiscard]] std::expected<void, CppqError> rollback() {
        auto r = conn_.rollback();
        active_ = false;
        return r;
    }

    [[nodiscard]] bool is_active() const { return active_; }
};

} // namespace cppq
