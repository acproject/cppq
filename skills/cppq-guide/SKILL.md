---
name: cppq-guide
description: Build parameterized PostgreSQL SQL queries using the cppq C++23 library. Use when writing code that interacts with PostgreSQL through cppq, building SELECT/INSERT/UPDATE/DELETE queries, managing transactions, handling results, extending the library with new expressions or builders, or working with the cppq FFI layer for Python/Java bindings.
---

# cppq Library Guide

cppq is a C++23 PostgreSQL query builder library with type-safe parameterized queries, SQL injection prevention, and `std::expected`-based error handling.

## Architecture

```
third_party/cppq/
├── include/cppq/
│   ├── core/           # Foundation: Param, Column, Query, Error, Identifier
│   ├── sql/            # SQL builders: Select, Insert, Update, Delete, Expression, Aggregate, SetOp
│   ├── pg/             # PostgreSQL: Connection, Result
│   └── ffi/            # C ABI for Python/Java bindings
├── src/                # Implementation files
├── bindings/           # Python (ctypes) and Java (JNI) bindings
├── tests/              # Unit tests (60 tests, no DB needed for SQL builder tests)
└── examples/           # Usage examples
```

## Quick Reference

### Include what you need

```cpp
#include <cppq/sql/Select.hpp>    // SELECT builder
#include <cppq/sql/Insert.hpp>    // INSERT builder
#include <cppq/sql/Update.hpp>    // UPDATE builder
#include <cppq/sql/Delete.hpp>    // DELETE builder
#include <cppq/sql/Aggregate.hpp> // count(), sum(), avg()
#include <cppq/sql/SetOp.hpp>     // union_(), intersect(), except()
#include <cppq/core/Identifier.hpp> // quote_ident()
#include <cppq/pg/Connection.hpp> // Connection + Transaction
```

### Core types

| Type | Header | Purpose |
|------|--------|---------|
| `Param` | `core/Param.hpp` | `std::variant<monostate, bool, int32_t, int64_t, double, string, JsonParam>` |
| `Query` | `core/Query.hpp` | `{std::string sql; std::vector<Param> params;}` |
| `ColumnRef` | `core/Column.hpp` | `{string_view table_name; string_view column_name;}` |
| `ExprPtr` | `sql/Expression.hpp` | `std::unique_ptr<Expr>` — WHERE conditions |
| `CppqError` | `core/Error.hpp` | Inherits `std::exception`, has `code` + `message` |
| `Connection` | `pg/Connection.hpp` | PostgreSQL connection, move-only |
| `Result` | `pg/Result.hpp` | Query result, iterable via `begin()`/`end()` |
| `Transaction` | `pg/Connection.hpp` | RAII transaction guard |

### Factory functions

| Category | Functions |
|----------|-----------|
| **Builders** | `select()`, `insert()`, `update(table)`, `delete_from()` |
| **Comparison** | `eq`, `ne`, `gt`, `lt`, `ge`, `le`, `col_eq` |
| **Logic** | `and_`, `or_`, `and_many`, `or_many`, `not_` |
| **Null/IN** | `is_null`, `is_not_null`, `in` |
| **Range** | `between`, `not_between` |
| **LIKE** | `like` |
| **JSON** | `json_contains`, `json_contained_by`, `json_exists`, `json_exists_any`, `json_exists_all`, `json_field_eq`, `json_field_cmp` |
| **Subquery** | `exists`, `not_exists`, `in_subquery`, `not_in_subquery`, `scalar_subquery_cmp` |
| **Aggregate** | `count`, `sum`, `avg`, `min`, `max`, `count_as`, `sum_as`, `avg_as` |
| **Set ops** | `union_`, `union_all`, `intersect`, `except` |
| **Identifier** | `quote_ident`, `quote_literal` |
| **Column** | `col(name)`, `col(table, name)` |

### Error handling pattern

```cpp
// All Connection methods return std::expected<T, CppqError>
auto result = conn.execute(query);
if (!result) {
    // result.error() is CppqError with .code and .message
    return; // handle error
}
// result.value() is Result

// Builder .build() throws CppqError on validation failure
// (empty table, column/value mismatch, etc.)
```

## Key Patterns

### 1. Build then execute

```cpp
using namespace cppq;
auto q = select().columns({"id", "name"})
    .from("users")
    .where(eq(col("active"), Param(true)))
    .order_by("name")
    .limit(10)
    .build();
// q.sql = "SELECT id, name FROM users WHERE active=$1 ORDER BY name ASC LIMIT $2"
// q.params = {true, 10}

auto result = conn.execute(q);
```

### 2. RAII Transaction

```cpp
{
    Transaction txn(conn);  // BEGIN
    conn.execute(insert().into("users").columns({"name"}).values({Param("Alice")}).build());
    conn.execute(insert().into("logs").columns({"msg"}).values({Param("user created")}).build());
    txn.commit();           // COMMIT (forget this → auto ROLLBACK on destruct)
}
```

### 3. Result iteration

```cpp
auto result = conn.execute(q);
for (auto& row : *result) {
    auto name = row.by("name");          // by column name
    auto id   = row.get_int64(0);        // by index, typed
    auto raw  = row.get_raw(1);          // raw const char*
}
```

### 4. Subquery with auto-renumbering

```cpp
auto sub = select().columns({"1"}).from("orders")
    .where(eq(col("user_id"), Param(1))).build();
auto q = select().columns({"name"}).from("users")
    .where(exists(std::move(sub))).build();
// Parameters from subquery are auto-renumbered ($1→$2, etc.)
```

### 5. CTE + UNION

```cpp
auto q = select()
    .with("active_users", select().from("users").where(...).build())
    .from("active_users").build();

auto combined = union_all({select().from("users").build(),
                            select().from("archived").build()});
```

## Extending the Library

### Adding a new expression type

1. Define struct inheriting `Expr` in `sql/Expression.hpp`
2. Implement `to_sql(ParamList& params) const` override
3. Add factory function returning `ExprPtr`
4. Use `make_unique` + field assignment (not aggregate init — `Expr` has virtual functions)

```cpp
struct MyExpr final : Expr {
    ColumnRef column;
    Param value;
    [[nodiscard]] std::string to_sql(ParamList& params) const override {
        return std::format("MY_OP({}, {})", column.name(), params.add(value));
    }
};
inline ExprPtr my_op(ColumnRef c, Param v) {
    auto e = std::make_unique<MyExpr>();
    e->column = c;
    e->value = std::move(v);
    return e;
}
```

### Adding a new SQL builder

1. Create `include/cppq/sql/MyBuilder.hpp`
2. Follow the builder pattern: fluent methods return `*this`, `build()` returns `Query`
3. Add validation in `build()` that throws `CppqError::build_error(msg)`
4. Add factory function
5. Add tests in `tests/test_sql_builder.cpp`

### C++23 gotchas in this codebase

- `CppqError` inherits `std::exception` (virtual `what()`) → NOT an aggregate → use constructors/factory methods, not designated initializers
- `Expr` has virtual `to_sql()` → structs inheriting it are NOT aggregates → use `make_unique` + field assignment
- `initializer_list<unique_ptr>` doesn't work (const elements can't be moved) → use `vector<ExprPtr>` + `push_back`
- CMake `CACHE BOOL "" FORCE` in `cmake/FindDependencies.cmake` overrides `CPPQ_BUILD_TESTS` → build cppq standalone for tests

## Testing

```powershell
# Build tests standalone (avoids main project's CPPQ_BUILD_TESTS=OFF override)
cd third_party/cppq
cmake -B build_test -S . -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=Release -DCPPQ_BUILD_TESTS=ON -DCPPQ_BUILD_FFI=OFF `
    -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
    -DPG_ROOT="M:/postgresql-18.4-2-windows-x64-binaries/pgsql"
cmake --build build_test --config Release --target test_sql_builder
.\build_test\tests\Release\test_sql_builder.exe
```

## Additional Resources

- For complete API details, see [reference.md](reference.md)
- For usage examples, see [examples.md](examples.md)
