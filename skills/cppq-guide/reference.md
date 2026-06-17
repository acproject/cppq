# cppq API Reference

## Core Types

### Param (`core/Param.hpp`)

```cpp
using Param = std::variant<
    std::monostate,   // NULL
    bool,
    int32_t,
    int64_t,
    double,
    std::string,
    JsonParam         // JSON / JSONB
>;
```

Factory helpers:
- `json(string)` → `JsonParam{.is_jsonb = false}`
- `jsonb(string)` → `JsonParam{.is_jsonb = true}`
- `param_to_string(Param)` → string representation

### ColumnRef (`core/Column.hpp`)

```cpp
struct ColumnRef {
    std::string_view table_name;    // optional
    std::string_view column_name;
    std::string qualified() const;  // "table.column"
    std::string_view name() const;  // column_name
};
```

Factory:
- `col("name")` → ColumnRef without table
- `col("table", "name")` → ColumnRef with table

### Query (`core/Query.hpp`)

```cpp
struct Query {
    std::string sql;
    std::vector<Param> params;
};
```

Utilities:
- `renumber_placeholders(sql, offset)` — renumber `$1`→`$(offset+1)`, etc.
- `merge_subquery(sub, params)` — merge subquery params into parent, return renumbered SQL

### CppqError (`core/Error.hpp`)

```cpp
struct CppqError : std::exception {
    ErrorCode code;
    std::string message;
    // Factory methods:
    static CppqError connection_failed(std::string msg);
    static CppqError query_failed(std::string msg);
    static CppqError build_error(std::string msg);
};
```

ErrorCode values: `ConnectionFailed`, `QueryFailed`, `InvalidParam`, `SyntaxError`, `BuildError`

### Identifier (`core/Identifier.hpp`)

- `quote_ident("users")` → `"users"` (double-quoted, escapes internal `"`)
- `quote_literal("it's")` → `'it''s'` (single-quoted, escapes internal `'`)

---

## SQL Builders

### SelectBuilder (`sql/Select.hpp`)

```cpp
select()
    .columns({"col1", "col2"})              // default: "*"
    .from("table")                          // table name
    .from_alias("t")                        // FROM table t
    .from_subquery(sub_query, "alias")      // FROM (SELECT ...) AS alias
    .with("cte_name", sub_query)            // WITH cte_name AS (SELECT ...)
    .join(JoinType::Inner, "t2", on_expr)   // JOIN
    .join_as(JoinType::Left, "t2", "a", on) // JOIN with alias
    .inner_join("t2", on_expr)              // convenience
    .left_join("t2", on_expr)               // convenience
    .where(expr)                            // WHERE
    .group_by({"col1", "col2"})             // GROUP BY
    .having(expr)                           // HAVING
    .distinct(true)                         // DISTINCT
    .order_by("col", Order::Desc)           // ORDER BY (chainable)
    .limit(10)                              // LIMIT $N
    .offset(20)                             // OFFSET $N
    .build()                                // → Query
```

JoinType: `Inner`, `Left`, `Right`, `Full`, `Cross`
Order: `Asc`, `Desc`

Validation: throws `CppqError::build_error` if table or from_subquery is missing, or limit/offset is negative.

### InsertBuilder (`sql/Insert.hpp`)

```cpp
insert()
    .into("table")
    .columns({"col1", "col2"})
    .values({Param(1), Param("text")})       // single row
    .values_batch({                          // multi-row
        {Param(1), Param("a")},
        {Param(2), Param("b")}
    })
    .on_conflict_do_nothing("id")            // ON CONFLICT (id) DO NOTHING
    .on_conflict_do_update("id", {           // ON CONFLICT (id) DO UPDATE SET
        {"name", Param("updated")},
        {"count", Param(0)}
    })
    .returning({"id", "created_at"})
    .build()
```

Validation: throws if table empty, no values, or column/value count mismatch.

### UpdateBuilder (`sql/Update.hpp`)

```cpp
update("table")
    .set("col1", Param(val1))   // chainable
    .set("col2", Param(val2))
    .where(expr)
    .returning({"id"})
    .build()
```

Constructor takes table name directly. Validation: throws if table empty or no SET clauses.

### DeleteBuilder (`sql/Delete.hpp`)

```cpp
delete_from()
    .from("table")
    .where(expr)
    .returning({"id"})
    .build()
```

### SetOp (`sql/SetOp.hpp`)

```cpp
union_(queries)         // UNION
union_all(queries)      // UNION ALL
intersect(queries)      // INTERSECT
except(queries)         // EXCEPT
set_operation(SetOp::UnionAll, queries)  // generic
```

SetOp enum: `Union`, `UnionAll`, `Intersect`, `IntersectAll`, `Except`, `ExceptAll`

---

## Expressions (`sql/Expression.hpp`)

All factory functions return `ExprPtr` (`unique_ptr<Expr>`).

### Comparison

| Function | SQL | Notes |
|----------|-----|-------|
| `eq(col, val)` | `col=$N` | equals |
| `ne(col, val)` | `col<>$N` | not equals |
| `gt(col, val)` | `col>$N` | greater than |
| `lt(col, val)` | `col<$N` | less than |
| `ge(col, val)` | `col>=$N` | greater or equal |
| `le(col, val)` | `col<=$N` | less or equal |
| `col_eq(colL, colR)` | `colL=colR` | column-to-column (JOIN ON) |
| `like(col, pattern)` | `col LIKE $N` | pattern matching |

### Null / IN

| Function | SQL |
|----------|-----|
| `is_null(col)` | `col IS NULL` |
| `is_not_null(col)` | `col IS NOT NULL` |
| `in(col, {v1, v2})` | `col IN ($N, $N+1)` |

### Logic

| Function | SQL |
|----------|-----|
| `and_(a, b)` | `(a AND b)` |
| `or_(a, b)` | `(a OR b)` |
| `and_many(vector)` | `(a AND b AND c)` |
| `or_many(vector)` | `(a OR b OR c)` |
| `not_(expr)` | `NOT (expr)` |

### Range

| Function | SQL |
|----------|-----|
| `between(col, low, high)` | `col BETWEEN $N AND $N+1` |
| `not_between(col, low, high)` | `col NOT BETWEEN $N AND $N+1` |

### JSON/JSONB

| Function | SQL | Notes |
|----------|-----|-------|
| `json_contains(col, val)` | `col @> $N` | contains |
| `json_contained_by(col, val)` | `col <@ $N` | contained by |
| `json_exists(col, key)` | `col ? $N` | key exists |
| `json_exists_any(col, keys)` | `col ?| $N` | any key exists |
| `json_exists_all(col, keys)` | `col ?& $N` | all keys exist |
| `json_field_eq(col, field, val)` | `col->>$N = $N+1` | field equals |
| `json_field_cmp(col, field, op, val)` | `col->>$N <op> $N+1` | field comparison |

### Subquery

| Function | SQL |
|----------|-----|
| `exists(sub)` | `EXISTS (subquery)` |
| `not_exists(sub)` | `NOT EXISTS (subquery)` |
| `in_subquery(col, sub)` | `col IN (subquery)` |
| `not_in_subquery(col, sub)` | `col NOT IN (subquery)` |
| `scalar_subquery_cmp(col, op, sub)` | `col <op> (subquery)` |

Subquery params are auto-renumbered via `merge_subquery()`.

---

## Aggregate Helpers (`sql/Aggregate.hpp`)

Return `std::string` for use in `.columns()`.

| Function | Output |
|----------|--------|
| `count()` / `count("*")` | `COUNT(*)` |
| `count("col")` | `COUNT(col)` |
| `sum("col")` | `SUM(col)` |
| `avg("col")` | `AVG(col)` |
| `min("col")` | `MIN(col)` |
| `max("col")` | `MAX(col)` |
| `count_as("*", "cnt")` | `COUNT(*) AS cnt` |
| `sum_as("col", "total")` | `SUM(col) AS total` |
| `avg_as("col", "avg_col")` | `AVG(col) AS avg_col` |

---

## PostgreSQL Layer

### Connection (`pg/Connection.hpp`)

```cpp
Connection conn("postgresql://localhost/mydb");
auto r = conn.connect();                    // std::expected<void, CppqError>
auto result = conn.execute(query);          // std::expected<Result, CppqError>
conn.begin() / conn.commit() / conn.rollback();
conn.is_connected();
conn.disconnect();
```

Move-only. JSON params get correct OID (json=114, jsonb=3802). NULL via `std::monostate`→`nullptr`.

### Transaction (`pg/Connection.hpp`)

```cpp
Transaction txn(conn);    // BEGIN on construct
txn.commit();             // COMMIT (sets active=false)
txn.rollback();           // ROLLBACK (sets active=false)
txn.is_active();
// Destructor: auto ROLLBACK if still active
```

Non-copyable, non-movable. Holds `Connection&` reference.

### Result (`pg/Result.hpp`)

```cpp
result.rows();                    // int row count
result.cols();                    // int column count
result.col_name(col);             // std::string
result.col_type(col);             // Oid
result.is_json_type(col);         // bool (OID 114 or 3802)
result.affected_rows();           // std::string (for INSERT/UPDATE/DELETE)

// Typed getters (return std::optional)
result.get(row, col);             // std::optional<std::string>
result.get_raw(row, col);         // const char* (valid until PQclear)
result.get_int32(row, col);       // std::optional<int32_t>
result.get_int64(row, col);       // std::optional<int64_t>
result.get_double(row, col);      // std::optional<double>
result.get_bool(row, col);        // std::optional<bool>
result.get_or(row, col, default); // std::string with fallback

// Row iteration
for (auto& row : result) {
    row.get(col);                 // std::optional<std::string>
    row.get_int64(col);
    row.get_double(col);
    row.get_bool(col);
    row.get_raw(col);
    row.get_or(col, "default");
    row.by("column_name");        // std::optional<std::string> by name
    row.col_index("name");        // int (-1 if not found)
    row.col_count();
    row.col_name(col);
    row.is_json_type(col);
}
```

---

## FFI Layer (`ffi/cppq_ffi.h`)

C ABI for Python (ctypes) and Java (JNI) bindings. Key functions:

- `cppq_connect(conninfo)` → connection handle
- `cppq_execute(conn, sql, params, n)` → result handle
- `cppq_result_rows / cppq_result_cols / cppq_result_get / cppq_result_col_name`
- `cppq_result_get_int32 / int64 / double / bool / affected_rows`
- Builder functions: `cppq_select_*`, `cppq_insert_*`, `cppq_update_*`, `cppq_delete_*`
- `cppq_query_build` → produces SQL string + param count
