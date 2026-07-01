#ifndef CPPQ_FFI_H
#define CPPQ_FFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handles */
typedef struct cppq_query  cppq_query;
typedef struct cppq_conn   cppq_conn;
typedef struct cppq_result cppq_result;

/* ============================================================
 * SELECT Builder
 * ============================================================ */
cppq_query* cppq_select(const char** columns, int col_count);
void        cppq_select_from(cppq_query* q, const char* table);
void        cppq_select_from_alias(cppq_query* q, const char* table, const char* alias);
void        cppq_select_where_eq_str(cppq_query* q, const char* col, const char* val);
void        cppq_select_where_eq_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_ne_str(cppq_query* q, const char* col, const char* val);
void        cppq_select_where_ne_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_gt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_lt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_ge_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_le_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_like(cppq_query* q, const char* col, const char* pattern);
void        cppq_select_where_is_null(cppq_query* q, const char* col);
void        cppq_select_where_is_not_null(cppq_query* q, const char* col);
void        cppq_select_where_in_strs(cppq_query* q, const char* col, const char** vals, int count);
void        cppq_select_where_in_ints(cppq_query* q, const char* col, const int64_t* vals, int count);
void        cppq_select_where_between_int(cppq_query* q, const char* col, int64_t low, int64_t high);
void        cppq_select_where_json_contains(cppq_query* q, const char* col, const char* json_str);
void        cppq_select_where_json_field_eq(cppq_query* q, const char* col, const char* field, const char* val);

/* SELECT JOIN */
void        cppq_select_inner_join(cppq_query* q, const char* table, const char* on_left, const char* on_right);
void        cppq_select_left_join(cppq_query* q, const char* table, const char* on_left, const char* on_right);

/* SELECT GROUP BY / HAVING / DISTINCT */
void        cppq_select_group_by(cppq_query* q, const char** cols, int count);
void        cppq_select_distinct(cppq_query* q, int enabled);

void        cppq_select_order_by(cppq_query* q, const char* col, int asc);
void        cppq_select_limit(cppq_query* q, int64_t limit);
void        cppq_select_offset(cppq_query* q, int64_t offset);

/* ============================================================
 * INSERT Builder
 * ============================================================ */
cppq_query* cppq_insert(const char* table, const char** columns, int col_count);
void        cppq_insert_value_str(cppq_query* q, const char* val);
void        cppq_insert_value_int(cppq_query* q, int64_t val);
void        cppq_insert_value_double(cppq_query* q, double val);
void        cppq_insert_value_null(cppq_query* q);
void        cppq_insert_value_json(cppq_query* q, const char* json_str);
void        cppq_insert_value_jsonb(cppq_query* q, const char* json_str);
void        cppq_insert_returning(cppq_query* q, const char** columns, int col_count);

/* INSERT batch: 结束当前行，开始新行 */
void        cppq_insert_next_row(cppq_query* q);

/* INSERT ON CONFLICT */
void        cppq_insert_on_conflict_do_nothing(cppq_query* q, const char* col);
void        cppq_insert_on_conflict_do_update(cppq_query* q, const char* conflict_col,
                                               const char** set_cols, const char** set_vals, int count);

/* ============================================================
 * UPDATE Builder
 * ============================================================ */
cppq_query* cppq_update(const char* table);
void        cppq_update_set_str(cppq_query* q, const char* col, const char* val);
void        cppq_update_set_int(cppq_query* q, const char* col, int64_t val);
void        cppq_update_set_double(cppq_query* q, const char* col, double val);
void        cppq_update_set_null(cppq_query* q, const char* col);
void        cppq_update_set_json(cppq_query* q, const char* col, const char* json_str);
void        cppq_update_set_jsonb(cppq_query* q, const char* col, const char* json_str);
void        cppq_update_where_eq_str(cppq_query* q, const char* col, const char* val);
void        cppq_update_where_eq_int(cppq_query* q, const char* col, int64_t val);
void        cppq_update_where_ne_str(cppq_query* q, const char* col, const char* val);
void        cppq_update_where_gt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_update_where_lt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_update_where_like(cppq_query* q, const char* col, const char* pattern);
void        cppq_update_where_in_strs(cppq_query* q, const char* col, const char** vals, int count);
void        cppq_update_returning(cppq_query* q, const char** columns, int col_count);

/* ============================================================
 * DELETE Builder
 * ============================================================ */
cppq_query* cppq_delete(const char* table);
void        cppq_delete_where_eq_str(cppq_query* q, const char* col, const char* val);
void        cppq_delete_where_eq_int(cppq_query* q, const char* col, int64_t val);
void        cppq_delete_where_ne_str(cppq_query* q, const char* col, const char* val);
void        cppq_delete_where_gt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_delete_where_lt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_delete_where_like(cppq_query* q, const char* col, const char* pattern);
void        cppq_delete_where_in_strs(cppq_query* q, const char* col, const char** vals, int count);
void        cppq_delete_returning(cppq_query* q, const char** columns, int col_count);

/* ============================================================
 * Query Introspection (SQL + Params)
 * ============================================================ */
const char* cppq_query_sql(const cppq_query* q);
int         cppq_query_param_count(const cppq_query* q);
const char* cppq_query_param_str(const cppq_query* q, int index);
int64_t     cppq_query_param_int(const cppq_query* q, int index);
double      cppq_query_param_double(const cppq_query* q, int index);
int         cppq_query_param_is_null(const cppq_query* q, int index);

/* ============================================================
 * Connection
 * ============================================================ */
cppq_conn*   cppq_connect(const char* conn_info);
void         cppq_disconnect(cppq_conn* conn);
int          cppq_is_connected(const cppq_conn* conn);
const char*  cppq_last_error(const cppq_conn* conn);
cppq_result* cppq_execute(cppq_conn* conn, const cppq_query* q);

/* ============================================================
 * Raw SQL Execution (DDL: CREATE TABLE / INDEX / etc.)
 * ============================================================ */
int cppq_execute_raw(cppq_conn* conn, const char* sql);

/* ============================================================
 * Transaction
 * ============================================================ */
int cppq_begin(cppq_conn* conn);
int cppq_commit(cppq_conn* conn);
int cppq_rollback(cppq_conn* conn);

/* ============================================================
 * Result
 * ============================================================ */
int          cppq_result_rows(const cppq_result* r);
int          cppq_result_cols(const cppq_result* r);
const char*  cppq_result_get(const cppq_result* r, int row, int col);
int          cppq_result_is_null(const cppq_result* r, int row, int col);
const char*  cppq_result_col_name(const cppq_result* r, int col);
unsigned int cppq_result_col_type(const cppq_result* r, int col);
int          cppq_result_is_json(const cppq_result* r, int col);
int64_t      cppq_result_get_int64(const cppq_result* r, int row, int col);
double       cppq_result_get_double(const cppq_result* r, int row, int col);
int          cppq_result_get_bool(const cppq_result* r, int row, int col);
const char*  cppq_result_affected_rows(const cppq_result* r);

/* ============================================================
 * Memory Management
 * ============================================================ */
void         cppq_query_free(cppq_query* q);
void         cppq_result_free(cppq_result* r);

#ifdef __cplusplus
}
#endif

#endif /* CPPQ_FFI_H */
