# cppq Usage Examples

## SELECT

### Basic query

```cpp
using namespace cppq;

auto q = select().columns({"id", "name", "email"})
    .from("users")
    .where(eq(col("active"), Param(true)))
    .order_by("name", Order::Asc)
    .limit(10)
    .build();
// SQL: SELECT id, name, email FROM users WHERE active=$1 ORDER BY name ASC LIMIT $2
// Params: {true, 10}
```

### Multiple WHERE conditions

```cpp
auto q = select().from("users")
    .where(and_(
        eq(col("active"), Param(true)),
        or_(
            like(col("name"), Param("A%")),
            is_null(col("deleted_at"))
        )
    ))
    .build();
// SQL: SELECT * FROM users WHERE (active=$1 AND (name LIKE $2 OR deleted_at IS NULL))
```

### IN with multiple values

```cpp
auto q = select().from("users")
    .where(in(col("role"), {Param("admin"), Param("editor"), Param("moderator")}))
    .build();
// SQL: SELECT * FROM users WHERE role IN ($1, $2, $3)
```

### BETWEEN

```cpp
auto q = select().from("products")
    .where(between(col("price"), Param(10.0), Param(100.0)))
    .build();
// SQL: SELECT * FROM products WHERE price BETWEEN $1 AND $2
```

### JOIN

```cpp
auto q = select().columns({"u.name", "o.total"})
    .from("users").from_alias("u")
    .inner_join("orders", col_eq(col("u", "id"), col("orders", "user_id")))
    .where(gt(col("o", "total"), Param(100.0)))
    .build();
// SQL: SELECT u.name, o.total FROM users u INNER JOIN orders ON u.id=orders.user_id WHERE o.total>$1
```

### LEFT JOIN with alias

```cpp
auto q = select().columns({"u.name", "p.title"})
    .from("users").from_alias("u")
    .join_as(JoinType::Left, "posts", "p",
        col_eq(col("u", "id"), col("p", "user_id")))
    .build();
// SQL: SELECT u.name, p.title FROM users u LEFT JOIN posts p ON u.id=p.user_id
```

### GROUP BY + HAVING + Aggregate

```cpp
using namespace cppq;

auto q = select()
    .columns({"user_id", count_as("*", "order_count"), sum_as("total", "total_spent")})
    .from("orders")
    .group_by({"user_id"})
    .having(gt(col("order_count"), Param(5)))
    .order_by("total_spent", Order::Desc)
    .build();
// SQL: SELECT user_id, COUNT(*) AS order_count, SUM(total) AS total_spent
//      FROM orders GROUP BY user_id HAVING order_count>$1 ORDER BY total_spent DESC
```

### DISTINCT

```cpp
auto q = select().columns({"category"}).from("products").distinct().build();
// SQL: SELECT DISTINCT category FROM products
```

### Subquery — EXISTS

```cpp
auto sub = select().columns({"1"}).from("orders")
    .where(eq(col("user_id"), Param(42))).build();

auto q = select().columns({"name"}).from("users")
    .where(exists(std::move(sub)))
    .build();
// SQL: SELECT name FROM users WHERE EXISTS (SELECT 1 FROM orders WHERE user_id=$1)
```

### Subquery — IN

```cpp
auto sub = select().columns({"id"}).from("banned_users").build();

auto q = select().columns({"name"}).from("users")
    .where(not_in_subquery(col("id"), std::move(sub)))
    .build();
// SQL: SELECT name FROM users WHERE id NOT IN (SELECT id FROM banned_users)
```

### Subquery — parameter renumbering

```cpp
// Parent has 1 param, subquery has 2 params
// Subquery $1,$2 → renumbered to $2,$3
auto sub = select().columns({"1"}).from("orders")
    .where(and_(
        eq(col("user_id"), Param(1)),
        eq(col("status"), Param("paid"))
    )).build();

auto q = select().columns({"name"}).from("users")
    .where(and_(
        eq(col("active"), Param(true)),
        exists(std::move(sub))
    ))
    .build();
// SQL: SELECT name FROM users WHERE (active=$1 AND EXISTS (
//      SELECT 1 FROM orders WHERE (user_id=$2 AND status=$3)))
```

### FROM subquery

```cpp
auto sub = select().columns({"id", "name"}).from("users")
    .where(eq(col("active"), Param(true))).build();

auto q = select().columns({"id", "name"})
    .from_subquery(std::move(sub), "u")
    .order_by("name")
    .build();
// SQL: SELECT id, name FROM (SELECT id, name FROM users WHERE active=$1) AS u ORDER BY name ASC
```

### CTE (WITH clause)

```cpp
auto paid_orders = select().columns({"id", "amount"}).from("orders")
    .where(eq(col("status"), Param("paid"))).build();

auto q = select()
    .with("paid_orders", std::move(paid_orders))
    .columns({"id", "amount"})
    .from("paid_orders")
    .where(gt(col("amount"), Param(100)))
    .build();
// SQL: WITH paid_orders AS (SELECT id, amount FROM orders WHERE status=$1)
//      SELECT id, amount FROM paid_orders WHERE amount>$2
```

### Multiple CTEs

```cpp
auto q = select()
    .with("u", select().columns({"id"}).from("users").build())
    .with("o", select().columns({"id"}).from("orders").build())
    .columns({"*"}).from("u")
    .build();
// SQL: WITH u AS (SELECT id FROM users), o AS (SELECT id FROM orders) SELECT * FROM u
```

### JSON/JSONB queries

```cpp
// data @> '{"role":"admin"}'
auto q1 = select().from("users")
    .where(json_contains(col("data"), jsonb(R"({"role":"admin"})")))
    .build();

// data->>'email' = 'test@example.com'
auto q2 = select().from("users")
    .where(json_field_eq(col("data"), "email", Param("test@example.com")))
    .build();

// data ? 'phone' (key exists)
auto q3 = select().from("users")
    .where(json_exists(col("data"), "phone"))
    .build();
```

## INSERT

### Single row

```cpp
auto q = insert().into("users")
    .columns({"name", "email", "age"})
    .values({Param("Alice"), Param("alice@example.com"), Param(30)})
    .build();
// SQL: INSERT INTO users (name, email, age) VALUES ($1, $2, $3)
```

### Batch insert

```cpp
auto q = insert().into("users")
    .columns({"name", "email"})
    .values_batch({
        {Param("Alice"), Param("alice@example.com")},
        {Param("Bob"), Param("bob@example.com")},
        {Param("Carol"), Param("carol@example.com")}
    })
    .build();
// SQL: INSERT INTO users (name, email) VALUES ($1, $2), ($3, $4), ($5, $6)
```

### INSERT with RETURNING

```cpp
auto q = insert().into("users")
    .columns({"name"})
    .values({Param("Alice")})
    .returning({"id", "created_at"})
    .build();
// SQL: INSERT INTO users (name) VALUES ($1) RETURNING id, created_at
```

### Upsert (ON CONFLICT)

```cpp
// DO NOTHING
auto q1 = insert().into("users")
    .columns({"email", "name"})
    .values({Param("alice@example.com"), Param("Alice")})
    .on_conflict_do_nothing("email")
    .build();
// SQL: INSERT INTO users (email, name) VALUES ($1, $2) ON CONFLICT (email) DO NOTHING

// DO UPDATE
auto q2 = insert().into("users")
    .columns({"email", "name"})
    .values({Param("alice@example.com"), Param("Alice Updated")})
    .on_conflict_do_update("email", {
        {"name", Param("Alice Updated")},
        {"updated_at", Param("now()")}
    })
    .build();
// SQL: INSERT INTO users (email, name) VALUES ($1, $2)
//      ON CONFLICT (email) DO UPDATE SET name=$3, updated_at=$4
```

## UPDATE

```cpp
auto q = update("users")
    .set("name", Param("Alice Smith"))
    .set("age", Param(31))
    .where(eq(col("id"), Param(42)))
    .returning({"id", "name"})
    .build();
// SQL: UPDATE users SET name=$1, age=$2 WHERE id=$3 RETURNING id, name
```

## DELETE

```cpp
auto q = delete_from()
    .from("users")
    .where(eq(col("id"), Param(42)))
    .returning({"name"})
    .build();
// SQL: DELETE FROM users WHERE id=$1 RETURNING name
```

## UNION / INTERSECT / EXCEPT

```cpp
// UNION ALL with parameters
auto q1 = select().columns({"id"}).from("users")
    .where(eq(col("active"), Param(true))).build();
auto q2 = select().columns({"id"}).from("users")
    .where(eq(col("admin"), Param(true))).build();

auto combined = union_all({std::move(q1), std::move(q2)});
// SQL: SELECT id FROM users WHERE active=$1 UNION ALL SELECT id FROM users WHERE admin=$2

// INTERSECT
auto a = select().columns({"user_id"}).from("orders").build();
auto b = select().columns({"user_id"}).from("reviews").build();
auto result = intersect({std::move(a), std::move(b)});
// SQL: SELECT user_id FROM orders INTERSECT SELECT user_id FROM reviews
```

## Connection + Transaction + Result

```cpp
using namespace cppq;

// Connect
Connection conn("postgresql://localhost/mydb");
if (auto r = conn.connect(); !r) {
    std::cerr << "Connect failed: " << r.error().message << "\n";
    return;
}

// RAII Transaction
{
    Transaction txn(conn);

    // Insert
    auto ins = insert().into("users")
        .columns({"name", "email"})
        .values({Param("Alice"), Param("alice@example.com")})
        .returning({"id"})
        .build();

    auto result = conn.execute(ins);
    if (!result) {
        std::cerr << "Insert failed: " << result.error().message << "\n";
        return;  // txn destructor auto-rolls back
    }

    // Read returned ID
    auto id = (*result).get(0, 0);
    std::cout << "Created user id: " << id.value_or("?") << "\n";

    // Update related table
    auto upd = update("audit_log")
        .set("last_user_id", Param(42))
        .where(eq(col("id"), Param(1)))
        .build();

    if (auto r2 = conn.execute(upd); !r2) {
        std::cerr << "Update failed\n";
        return;  // auto rollback
    }

    txn.commit();  // commit both operations
}

// Query with result iteration
auto q = select().columns({"id", "name", "email"})
    .from("users")
    .where(eq(col("active"), Param(true)))
    .order_by("name")
    .build();

auto result = conn.execute(q);
if (!result) {
    std::cerr << "Query failed: " << result.error().message << "\n";
    return;
}

for (auto& row : *result) {
    auto id = row.get_int64(0);
    auto name = row.by("name");
    auto email = row.get_or(2, "N/A");
    std::cout << "id=" << id.value_or(0)
              << " name=" << name.value_or("?")
              << " email=" << email << "\n";
}
```

## Identifier Safety

```cpp
using namespace cppq;

// Safe table/column names (prevents SQL injection on identifiers)
std::string table = quote_ident("order");  // "order" (keyword-safe)
std::string col = quote_ident("my column"); // "my column"

// For dynamic table names:
auto q = select().from(quote_ident(user_table))
    .where(eq(col(quote_ident(user_column)), Param(value)))
    .build();
```
