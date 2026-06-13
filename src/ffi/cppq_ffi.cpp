#include <cppq/ffi/cppq_ffi.h>

#include <cppq/core/Column.hpp>
#include <cppq/core/Param.hpp>
#include <cppq/core/Query.hpp>
#include <cppq/pg/Connection.hpp>
#include <cppq/pg/Result.hpp>
#include <cppq/sql/Delete.hpp>
#include <cppq/sql/Expression.hpp>
#include <cppq/sql/Insert.hpp>
#include <cppq/sql/Select.hpp>
#include <cppq/sql/Update.hpp>

#include <cstring>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// ============================================================
// Internal opaque structures wrapping C++ objects
// ============================================================

// Query type tag to know which builder to use
enum class QueryType { Select, Insert, Update, Delete };

struct cppq_query {
    QueryType type;

    // Builder state (only one is active based on type)
    std::unique_ptr<cppq::SelectBuilder> select_builder;
    std::unique_ptr<cppq::InsertBuilder> insert_builder;
    std::unique_ptr<cppq::UpdateBuilder> update_builder;
    std::unique_ptr<cppq::DeleteBuilder> delete_builder;

    // Cached build result
    std::optional<cppq::Query> built_query;

    // String cache for returning C strings
    std::string sql_cache;
    std::vector<std::string> param_str_cache;

    // Persistent string storage: all strings from FFI calls are copied here
    // so that string_view references remain valid for the query's lifetime.
    // Uses std::list because it never invalidates references/pointers on insertion.
    std::list<std::string> str_pool;

    // INSERT-specific: temporary value accumulator
    std::vector<cppq::Param> insert_values;
    // INSERT/SELECT column list accumulator
    std::vector<std::string> col_strings;
    std::vector<std::string_view> col_views;
    // INSERT returning columns
    std::vector<std::string> ret_strings;
    std::vector<std::string_view> ret_views;
};

struct cppq_conn {
    std::unique_ptr<cppq::Connection> conn;
    std::string last_error;
};

struct cppq_result {
    std::unique_ptr<cppq::Result> result;
};

// ============================================================
// Helper: ensure query is built (call build() once)
// ============================================================
static void ensure_built(cppq_query* q) {
    if (q->built_query.has_value()) return;

    switch (q->type) {
        case QueryType::Select:
            q->built_query = q->select_builder->build();
            break;
        case QueryType::Insert:
            // Apply accumulated values before build
            q->insert_builder->values(std::move(q->insert_values));
            if (!q->ret_views.empty()) {
                q->insert_builder->returning(q->ret_views);
            }
            q->built_query = q->insert_builder->build();
            break;
        case QueryType::Update:
            q->built_query = q->update_builder->build();
            break;
        case QueryType::Delete:
            q->built_query = q->delete_builder->build();
            break;
    }
}

// Helper: copy a C string into the query's persistent string pool
// Returns a string_view into the pooled copy (valid for query lifetime)
static std::string_view pool_str(cppq_query* q, const char* s) {
    q->str_pool.emplace_back(s ? s : "");
    return std::string_view(q->str_pool.back());
}

// ============================================================
// SELECT Builder
// ============================================================

cppq_query* cppq_select(const char** columns, int col_count) {
    auto* q = new cppq_query();
    q->type = QueryType::Select;
    q->select_builder = std::make_unique<cppq::SelectBuilder>();

    if (columns && col_count > 0) {
        q->col_strings.reserve(static_cast<size_t>(col_count));
        q->col_views.reserve(static_cast<size_t>(col_count));
        for (int i = 0; i < col_count; ++i) {
            q->col_strings.emplace_back(columns[i]);
        }
        for (const auto& s : q->col_strings) {
            q->col_views.push_back(s);
        }
        q->select_builder->columns(q->col_views);
    }
    return q;
}

void cppq_select_from(cppq_query* q, const char* table) {
    if (!q || !table || q->type != QueryType::Select) return;
    q->select_builder->from(pool_str(q, table));
}

void cppq_select_where_eq_str(cppq_query* q, const char* col, const char* val) {
    if (!q || !col || !val || q->type != QueryType::Select) return;
    q->select_builder->where(cppq::eq(cppq::col(pool_str(q, col)), cppq::Param(std::string(val))));
}

void cppq_select_where_eq_int(cppq_query* q, const char* col, int64_t val) {
    if (!q || !col || q->type != QueryType::Select) return;
    q->select_builder->where(cppq::eq(cppq::col(pool_str(q, col)), cppq::Param(val)));
}

void cppq_select_where_gt_int(cppq_query* q, const char* col, int64_t val) {
    if (!q || !col || q->type != QueryType::Select) return;
    q->select_builder->where(cppq::gt(cppq::col(pool_str(q, col)), cppq::Param(val)));
}

void cppq_select_where_lt_int(cppq_query* q, const char* col, int64_t val) {
    if (!q || !col || q->type != QueryType::Select) return;
    q->select_builder->where(cppq::lt(cppq::col(pool_str(q, col)), cppq::Param(val)));
}

void cppq_select_where_like(cppq_query* q, const char* col, const char* pattern) {
    if (!q || !col || !pattern || q->type != QueryType::Select) return;
    q->select_builder->where(cppq::like(cppq::col(pool_str(q, col)), std::string(pattern)));
}

void cppq_select_order_by(cppq_query* q, const char* col, int asc) {
    if (!q || !col || q->type != QueryType::Select) return;
    q->select_builder->order_by(pool_str(q, col), asc ? cppq::Order::Asc : cppq::Order::Desc);
}

void cppq_select_limit(cppq_query* q, int64_t limit) {
    if (!q || q->type != QueryType::Select) return;
    q->select_builder->limit(limit);
}

void cppq_select_offset(cppq_query* q, int64_t offset) {
    if (!q || q->type != QueryType::Select) return;
    q->select_builder->offset(offset);
}

// ============================================================
// INSERT Builder
// ============================================================

cppq_query* cppq_insert(const char* table, const char** columns, int col_count) {
    auto* q = new cppq_query();
    q->type = QueryType::Insert;
    q->insert_builder = std::make_unique<cppq::InsertBuilder>();
    q->insert_builder->into(pool_str(q, table));

    if (columns && col_count > 0) {
        q->col_strings.reserve(static_cast<size_t>(col_count));
        q->col_views.reserve(static_cast<size_t>(col_count));
        for (int i = 0; i < col_count; ++i) {
            q->col_strings.emplace_back(columns[i]);
        }
        for (const auto& s : q->col_strings) {
            q->col_views.push_back(s);
        }
        q->insert_builder->columns(q->col_views);
    }
    return q;
}

void cppq_insert_value_str(cppq_query* q, const char* val) {
    if (!q || !val || q->type != QueryType::Insert) return;
    q->insert_values.emplace_back(std::string(val));
}

void cppq_insert_value_int(cppq_query* q, int64_t val) {
    if (!q || q->type != QueryType::Insert) return;
    q->insert_values.emplace_back(val);
}

void cppq_insert_value_double(cppq_query* q, double val) {
    if (!q || q->type != QueryType::Insert) return;
    q->insert_values.emplace_back(val);
}

void cppq_insert_value_null(cppq_query* q) {
    if (!q || q->type != QueryType::Insert) return;
    q->insert_values.emplace_back(std::monostate{});
}

void cppq_insert_returning(cppq_query* q, const char** columns, int col_count) {
    if (!q || !columns || q->type != QueryType::Insert) return;
    q->ret_strings.reserve(static_cast<size_t>(col_count));
    q->ret_views.reserve(static_cast<size_t>(col_count));
    for (int i = 0; i < col_count; ++i) {
        q->ret_strings.emplace_back(columns[i]);
    }
    for (const auto& s : q->ret_strings) {
        q->ret_views.push_back(s);
    }
}

// ============================================================
// UPDATE Builder
// ============================================================

cppq_query* cppq_update(const char* table) {
    auto* q = new cppq_query();
    q->type = QueryType::Update;
    q->update_builder = std::make_unique<cppq::UpdateBuilder>(pool_str(q, table));
    return q;
}

void cppq_update_set_str(cppq_query* q, const char* col, const char* val) {
    if (!q || !col || !val || q->type != QueryType::Update) return;
    q->update_builder->set(pool_str(q, col), cppq::Param(std::string(val)));
}

void cppq_update_set_int(cppq_query* q, const char* col, int64_t val) {
    if (!q || !col || q->type != QueryType::Update) return;
    q->update_builder->set(pool_str(q, col), cppq::Param(val));
}

void cppq_update_set_double(cppq_query* q, const char* col, double val) {
    if (!q || !col || q->type != QueryType::Update) return;
    q->update_builder->set(pool_str(q, col), cppq::Param(val));
}

void cppq_update_set_null(cppq_query* q, const char* col) {
    if (!q || !col || q->type != QueryType::Update) return;
    q->update_builder->set(pool_str(q, col), cppq::Param(std::monostate{}));
}

void cppq_update_where_eq_str(cppq_query* q, const char* col, const char* val) {
    if (!q || !col || !val || q->type != QueryType::Update) return;
    q->update_builder->where(cppq::eq(cppq::col(pool_str(q, col)), cppq::Param(std::string(val))));
}

void cppq_update_where_eq_int(cppq_query* q, const char* col, int64_t val) {
    if (!q || !col || q->type != QueryType::Update) return;
    q->update_builder->where(cppq::eq(cppq::col(pool_str(q, col)), cppq::Param(val)));
}

// ============================================================
// DELETE Builder
// ============================================================

cppq_query* cppq_delete(const char* table) {
    auto* q = new cppq_query();
    q->type = QueryType::Delete;
    q->delete_builder = std::make_unique<cppq::DeleteBuilder>();
    q->delete_builder->from(pool_str(q, table));
    return q;
}

void cppq_delete_where_eq_str(cppq_query* q, const char* col, const char* val) {
    if (!q || !col || !val || q->type != QueryType::Delete) return;
    q->delete_builder->where(cppq::eq(cppq::col(pool_str(q, col)), cppq::Param(std::string(val))));
}

void cppq_delete_where_eq_int(cppq_query* q, const char* col, int64_t val) {
    if (!q || !col || q->type != QueryType::Delete) return;
    q->delete_builder->where(cppq::eq(cppq::col(pool_str(q, col)), cppq::Param(val)));
}

// ============================================================
// Query Introspection
// ============================================================

const char* cppq_query_sql(const cppq_query* q) {
    if (!q) return "";
    auto* mq = const_cast<cppq_query*>(q);
    ensure_built(mq);
    mq->sql_cache = mq->built_query->sql;
    return mq->sql_cache.c_str();
}

int cppq_query_param_count(const cppq_query* q) {
    if (!q) return 0;
    auto* mq = const_cast<cppq_query*>(q);
    ensure_built(mq);
    return static_cast<int>(mq->built_query->params.size());
}

const char* cppq_query_param_str(const cppq_query* q, int index) {
    if (!q) return "";
    auto* mq = const_cast<cppq_query*>(q);
    ensure_built(mq);
    if (index < 0 || static_cast<size_t>(index) >= mq->built_query->params.size()) return "";

    // Ensure cache is big enough
    if (mq->param_str_cache.size() < mq->built_query->params.size()) {
        mq->param_str_cache.resize(mq->built_query->params.size());
    }
    mq->param_str_cache[static_cast<size_t>(index)] =
        cppq::param_to_string(mq->built_query->params[static_cast<size_t>(index)]);
    return mq->param_str_cache[static_cast<size_t>(index)].c_str();
}

int64_t cppq_query_param_int(const cppq_query* q, int index) {
    if (!q) return 0;
    auto* mq = const_cast<cppq_query*>(q);
    ensure_built(mq);
    if (index < 0 || static_cast<size_t>(index) >= mq->built_query->params.size()) return 0;

    const auto& p = mq->built_query->params[static_cast<size_t>(index)];
    if (auto* v = std::get_if<int64_t>(&p)) return *v;
    if (auto* v = std::get_if<int32_t>(&p)) return static_cast<int64_t>(*v);
    return 0;
}

double cppq_query_param_double(const cppq_query* q, int index) {
    if (!q) return 0.0;
    auto* mq = const_cast<cppq_query*>(q);
    ensure_built(mq);
    if (index < 0 || static_cast<size_t>(index) >= mq->built_query->params.size()) return 0.0;

    const auto& p = mq->built_query->params[static_cast<size_t>(index)];
    if (auto* v = std::get_if<double>(&p)) return *v;
    return 0.0;
}

int cppq_query_param_is_null(const cppq_query* q, int index) {
    if (!q) return 1;
    auto* mq = const_cast<cppq_query*>(q);
    ensure_built(mq);
    if (index < 0 || static_cast<size_t>(index) >= mq->built_query->params.size()) return 1;
    return std::holds_alternative<std::monostate>(mq->built_query->params[static_cast<size_t>(index)]) ? 1 : 0;
}

// ============================================================
// Connection
// ============================================================

cppq_conn* cppq_connect(const char* conn_info) {
    auto* c = new cppq_conn();
    c->conn = std::make_unique<cppq::Connection>(conn_info ? conn_info : "");
    auto result = c->conn->connect();
    if (!result.has_value()) {
        c->last_error = result.error().message;
        // Keep the conn object alive so caller can check last_error
    }
    return c;
}

void cppq_disconnect(cppq_conn* conn) {
    if (!conn) return;
    if (conn->conn) {
        conn->conn->disconnect();
    }
    delete conn;
}

int cppq_is_connected(const cppq_conn* conn) {
    if (!conn || !conn->conn) return 0;
    return conn->conn->is_connected() ? 1 : 0;
}

const char* cppq_last_error(const cppq_conn* conn) {
    if (!conn) return "";
    return conn->last_error.c_str();
}

cppq_result* cppq_execute(cppq_conn* conn, const cppq_query* q) {
    if (!conn || !conn->conn || !q) return nullptr;

    auto* mq = const_cast<cppq_query*>(q);
    ensure_built(mq);

    auto result = conn->conn->execute(*mq->built_query);
    if (!result.has_value()) {
        conn->last_error = result.error().message;
        return nullptr;
    }

    auto* r = new cppq_result();
    r->result = std::make_unique<cppq::Result>(std::move(result.value()));
    return r;
}

// ============================================================
// Transaction
// ============================================================

int cppq_begin(cppq_conn* conn) {
    if (!conn || !conn->conn) return -1;
    auto r = conn->conn->begin();
    if (!r.has_value()) { conn->last_error = r.error().message; return -1; }
    return 0;
}

int cppq_commit(cppq_conn* conn) {
    if (!conn || !conn->conn) return -1;
    auto r = conn->conn->commit();
    if (!r.has_value()) { conn->last_error = r.error().message; return -1; }
    return 0;
}

int cppq_rollback(cppq_conn* conn) {
    if (!conn || !conn->conn) return -1;
    auto r = conn->conn->rollback();
    if (!r.has_value()) { conn->last_error = r.error().message; return -1; }
    return 0;
}

// ============================================================
// Result
// ============================================================

int cppq_result_rows(const cppq_result* r) {
    if (!r || !r->result) return 0;
    return r->result->rows();
}

int cppq_result_cols(const cppq_result* r) {
    if (!r || !r->result) return 0;
    return r->result->cols();
}

const char* cppq_result_get(const cppq_result* r, int row, int col) {
    if (!r || !r->result) return nullptr;
    auto val = r->result->get(row, col);
    if (!val.has_value()) return nullptr;
    // Note: returns pointer to internal buffer, valid until next call or free
    // For a safer API, copy the string out. For FFI simplicity we return the pointer.
    // This works because PGresult data is stable until PQclear.
    static thread_local std::string cached;
    cached = val.value();
    return cached.c_str();
}

int cppq_result_is_null(const cppq_result* r, int row, int col) {
    if (!r || !r->result) return 1;
    return r->result->get(row, col).has_value() ? 0 : 1;
}

const char* cppq_result_col_name(const cppq_result* r, int col) {
    if (!r || !r->result) return "";
    static thread_local std::string cached;
    cached = r->result->col_name(col);
    return cached.c_str();
}

// ============================================================
// Memory Management
// ============================================================

void cppq_query_free(cppq_query* q) {
    delete q;
}

void cppq_result_free(cppq_result* r) {
    delete r;
}
