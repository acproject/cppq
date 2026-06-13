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
void        cppq_select_where_eq_str(cppq_query* q, const char* col, const char* val);
void        cppq_select_where_eq_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_gt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_lt_int(cppq_query* q, const char* col, int64_t val);
void        cppq_select_where_like(cppq_query* q, const char* col, const char* pattern);
void        cppq_select_where_json_contains(cppq_query* q, const char* col, const char* json_str);
void        cppq_select_where_json_field_eq(cppq_query* q, const char* col, const char* field, const char* val);
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

/* ============================================================
 * DELETE Builder
 * ============================================================ */
cppq_query* cppq_delete(const char* table);
void        cppq_delete_where_eq_str(cppq_query* q, const char* col, const char* val);
void        cppq_delete_where_eq_int(cppq_query* q, const char* col, int64_t val);

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

/* ============================================================
 * Memory Management
 * ============================================================ */
void         cppq_query_free(cppq_query* q);
void         cppq_result_free(cppq_result* r);

#ifdef __cplusplus
}
#endif

#endif /* CPPQ_FFI_H */
