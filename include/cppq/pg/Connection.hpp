#pragma once

#include <cppq/core/Error.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/pg/Result.hpp>

#include <expected>
#include <libpq-fe.h>
#include <string>
#include <utility>
#include <vector>

namespace cppq {

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
            return std::unexpected(CppqError{
                .code = ErrorCode::ConnectionFailed,
                .message = std::move(msg)
            });
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
            return std::unexpected(CppqError{
                .code = ErrorCode::ConnectionFailed,
                .message = "Not connected"
            });
        }

        // 将 Param 转换为 libpq 需要的 C 字符串数组
        std::vector<std::string> str_params;
        std::vector<const char*> c_params;
        str_params.reserve(query.params.size());
        c_params.reserve(query.params.size());

        for (const auto& p : query.params) {
            str_params.push_back(param_to_string(p));
        }
        for (const auto& s : str_params) {
            c_params.push_back(s.c_str());
        }

        PGresult* res = PQexecParams(
            conn_,
            query.sql.c_str(),
            static_cast<int>(c_params.size()),
            nullptr,  // 让服务器推断参数类型
            c_params.data(),
            nullptr,  // paramLengths
            nullptr,  // paramFormats (text)
            0         // resultFormat (text)
        );

        if (!res) {
            return std::unexpected(CppqError{
                .code = ErrorCode::QueryFailed,
                .message = "PQexecParams returned null"
            });
        }

        auto status = PQresultStatus(res);
        if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
            std::string msg = PQresultErrorMessage(res);
            PQclear(res);
            return std::unexpected(CppqError{
                .code = ErrorCode::QueryFailed,
                .message = std::move(msg)
            });
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
            return std::unexpected(CppqError{
                .code = ErrorCode::ConnectionFailed,
                .message = "Not connected"
            });
        }
        PGresult* res = PQexec(conn_, sql);
        auto status = PQresultStatus(res);
        PQclear(res);
        if (status != PGRES_COMMAND_OK) {
            return std::unexpected(CppqError{
                .code = ErrorCode::QueryFailed,
                .message = PQerrorMessage(conn_)
            });
        }
        return {};
    }

    PGconn* conn_ = nullptr;
    std::string conn_info_;
};

} // namespace cppq
