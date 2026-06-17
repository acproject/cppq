#include <cppq/sql/Select.hpp>
#include <cppq/sql/Insert.hpp>
#include <cppq/sql/Update.hpp>
#include <cppq/sql/Delete.hpp>
#include <cppq/sql/Aggregate.hpp>
#include <cppq/sql/SetOp.hpp>
#include <cppq/core/Column.hpp>
#include <cppq/core/Identifier.hpp>
#include <cppq/core/Param.hpp>
#include <cppq/core/Error.hpp>

#include <cassert>
#include <iostream>
#include <string>

using namespace cppq;

static int passed = 0;
static int failed = 0;

#define TEST(name) \
    static void test_##name(); \
    static struct Register_##name { \
        Register_##name() { \
            std::cout << "  " << #name << "... "; \
            try { test_##name(); std::cout << "PASS\n"; passed++; } \
            catch (const std::exception& e) { \
                std::cout << "FAIL: " << e.what() << "\n"; failed++; \
            } \
        } \
    } register_##name; \
    static void test_##name()

#define ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a != _b) { \
            throw std::runtime_error( \
                std::format("Expected: {}\n    Actual: {}\n    at {}:{}", _b, _a, __FILE__, __LINE__)); \
        } \
    } while(0)

#define ASSERT_TRUE(x) \
    do { if (!(x)) throw std::runtime_error(std::format("Assert failed at {}:{}", __FILE__, __LINE__)); } while(0)

#define ASSERT_THROWS(expr) \
    do { \
        bool caught = false; \
        try { expr; } catch (const cppq::CppqError&) { caught = true; } \
        if (!caught) throw std::runtime_error(std::format("Expected CppqError at {}:{}", __FILE__, __LINE__)); \
    } while(0)

// ============================================================
// SELECT Builder Tests
// ============================================================

TEST(select_basic) {
    auto q = select()
        .columns({"id", "name", "phone"})
        .from("users")
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT id, name, phone FROM users"));
    ASSERT_TRUE(q.params.empty());
}

TEST(select_where_eq) {
    auto q = select()
        .columns({"id", "name"})
        .from("users")
        .where(eq(col("phone"), Param(std::string("13800001234"))))
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT id, name FROM users WHERE phone=$1"));
    ASSERT_EQ(q.params.size(), 1UL);
    ASSERT_EQ(param_to_string(q.params[0]), std::string("13800001234"));
}

TEST(select_where_and) {
    auto q = select()
        .columns({"id"})
        .from("users")
        .where(and_(
            eq(col("status"), Param(std::string("active"))),
            gt(col("age"), Param(int32_t{18}))
        ))
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT id FROM users WHERE (status=$1 AND age>$2)"));
    ASSERT_EQ(q.params.size(), 2UL);
}

TEST(select_order_limit_offset) {
    auto q = select()
        .columns({"id", "name"})
        .from("users")
        .where(eq(col("phone"), Param(std::string("138"))))
        .order_by("name", Order::Asc)
        .limit(10)
        .offset(20)
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT id, name FROM users WHERE phone=$1 ORDER BY name ASC LIMIT $2 OFFSET $3"));
    ASSERT_EQ(q.params.size(), 3UL);
}

TEST(select_star) {
    auto q = select().from("users").build();
    ASSERT_EQ(q.sql, std::string("SELECT * FROM users"));
}

TEST(select_multi_order) {
    auto q = select()
        .columns({"id"})
        .from("users")
        .order_by("name", Order::Asc)
        .order_by("id", Order::Desc)
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT id FROM users ORDER BY name ASC, id DESC"));
}

// ---- 新增: JOIN / GROUP BY / DISTINCT / 表别名 ----

TEST(select_inner_join) {
    auto q = select()
        .columns({"u.name", "w.title"})
        .from("users")
        .from_alias("u")
        .inner_join("workouts w",
            col_eq(col("u.id"), col("w.user_id")))
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT u.name, w.title FROM users u INNER JOIN workouts w ON u.id=w.user_id"));
    ASSERT_TRUE(q.params.empty());
}

TEST(select_left_join) {
    auto q = select()
        .columns({"u.name"})
        .from("users")
        .from_alias("u")
        .left_join("profiles p",
            col_eq(col("u.id"), col("p.user_id")))
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT u.name FROM users u LEFT JOIN profiles p ON u.id=p.user_id"));
}

TEST(select_group_by) {
    auto q = select()
        .columns({"user_id", "COUNT(*)"})
        .from("workouts")
        .group_by({"user_id"})
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT user_id, COUNT(*) FROM workouts GROUP BY user_id"));
}

TEST(select_distinct) {
    auto q = select()
        .columns({"name"})
        .from("users")
        .distinct()
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT DISTINCT name FROM users"));
}

TEST(select_distinct_off) {
    auto q = select()
        .columns({"name"})
        .from("users")
        .distinct(false)
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT name FROM users"));
}

TEST(select_table_alias) {
    auto q = select()
        .columns({"id"})
        .from("users")
        .from_alias("u")
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT id FROM users u"));
}

// ---- 新增: 构建校验 ----

TEST(select_empty_table_throws) {
    ASSERT_THROWS(select().columns({"id"}).build());
}

TEST(select_negative_limit_throws) {
    ASSERT_THROWS(select().from("users").limit(-1).build());
}

// ============================================================
// INSERT Builder Tests
// ============================================================

TEST(insert_basic) {
    auto q = insert()
        .into("users")
        .columns({"name", "phone", "age"})
        .values({Param(std::string("Alice")), Param(std::string("138")), Param(int32_t{25})})
        .build();
    ASSERT_EQ(q.sql, std::string("INSERT INTO users (name, phone, age) VALUES ($1, $2, $3)"));
    ASSERT_EQ(q.params.size(), 3UL);
}

TEST(insert_returning) {
    auto q = insert()
        .into("users")
        .columns({"name"})
        .values({Param(std::string("Bob"))})
        .returning({"id", "created_at"})
        .build();
    ASSERT_EQ(q.sql, std::string("INSERT INTO users (name) VALUES ($1) RETURNING id, created_at"));
}

// ---- 新增: 批量 INSERT ----

TEST(insert_batch) {
    auto q = insert()
        .into("users")
        .columns({"name", "age"})
        .values_batch({
            {Param(std::string("Alice")), Param(int32_t{25})},
            {Param(std::string("Bob")), Param(int32_t{30})},
            {Param(std::string("Charlie")), Param(int32_t{35})}
        })
        .build();
    ASSERT_EQ(q.sql, std::string("INSERT INTO users (name, age) VALUES ($1, $2), ($3, $4), ($5, $6)"));
    ASSERT_EQ(q.params.size(), 6UL);
}

// ---- 新增: ON CONFLICT ----

TEST(insert_on_conflict_do_nothing) {
    auto q = insert()
        .into("users")
        .columns({"email", "name"})
        .values({Param(std::string("a@x.com")), Param(std::string("Alice"))})
        .on_conflict_do_nothing("email")
        .build();
    ASSERT_EQ(q.sql, std::string("INSERT INTO users (email, name) VALUES ($1, $2) ON CONFLICT (email) DO NOTHING"));
}

TEST(insert_on_conflict_do_update) {
    auto q = insert()
        .into("users")
        .columns({"email", "name"})
        .values({Param(std::string("a@x.com")), Param(std::string("Alice"))})
        .on_conflict_do_update("email", {
            {"name", Param(std::string("Alice Updated"))}
        })
        .build();
    ASSERT_EQ(q.sql, std::string(
        "INSERT INTO users (email, name) VALUES ($1, $2) "
        "ON CONFLICT (email) DO UPDATE SET name=$3"));
    ASSERT_EQ(q.params.size(), 3UL);
}

// ---- 新增: 构建校验 ----

TEST(insert_col_value_mismatch_throws) {
    ASSERT_THROWS(insert()
        .into("users")
        .columns({"name", "age"})
        .values({Param(std::string("Alice"))})  // 只有一个值，但有两列
        .build());
}

TEST(insert_empty_table_throws) {
    ASSERT_THROWS(insert()
        .columns({"name"})
        .values({Param(std::string("Alice"))})
        .build());
}

// ============================================================
// UPDATE Builder Tests
// ============================================================

TEST(update_basic) {
    auto q = update("users")
        .set("name", Param(std::string("Bob")))
        .set("age", Param(int32_t{30}))
        .where(eq(col("id"), Param(int64_t{42})))
        .build();
    ASSERT_EQ(q.sql, std::string("UPDATE users SET name=$1, age=$2 WHERE id=$3"));
    ASSERT_EQ(q.params.size(), 3UL);
}

TEST(update_no_where) {
    auto q = update("users")
        .set("status", Param(std::string("inactive")))
        .build();
    ASSERT_EQ(q.sql, std::string("UPDATE users SET status=$1"));
}

// ---- 新增: UPDATE RETURNING ----

TEST(update_returning) {
    auto q = update("users")
        .set("name", Param(std::string("Bob")))
        .where(eq(col("id"), Param(int64_t{42})))
        .returning({"id", "name"})
        .build();
    ASSERT_EQ(q.sql, std::string("UPDATE users SET name=$1 WHERE id=$2 RETURNING id, name"));
    ASSERT_EQ(q.params.size(), 2UL);
}

// ---- 新增: 构建校验 ----

TEST(update_empty_set_throws) {
    ASSERT_THROWS(update("users").build());
}

// ============================================================
// DELETE Builder Tests
// ============================================================

TEST(delete_basic) {
    auto q = delete_from()
        .from("users")
        .where(eq(col("id"), Param(int64_t{42})))
        .build();
    ASSERT_EQ(q.sql, std::string("DELETE FROM users WHERE id=$1"));
    ASSERT_EQ(q.params.size(), 1UL);
}

TEST(delete_no_where) {
    auto q = delete_from().from("logs").build();
    ASSERT_EQ(q.sql, std::string("DELETE FROM logs"));
    ASSERT_TRUE(q.params.empty());
}

// ---- 新增: DELETE RETURNING ----

TEST(delete_returning) {
    auto q = delete_from()
        .from("users")
        .where(eq(col("id"), Param(int64_t{42})))
        .returning({"id", "name"})
        .build();
    ASSERT_EQ(q.sql, std::string("DELETE FROM users WHERE id=$1 RETURNING id, name"));
}

// ============================================================
// Expression Tests
// ============================================================

TEST(expr_eq) {
    ParamList params;
    auto sql = eq(col("name"), Param(std::string("Alice")))->to_sql(params);
    ASSERT_EQ(sql, std::string("name=$1"));
    ASSERT_EQ(params.size(), 1UL);
}

TEST(expr_cmp_ops) {
    ParamList params;
    ASSERT_EQ(ne(col("a"), Param(int32_t{1}))->to_sql(params), std::string("a<>$1"));
    ASSERT_EQ(gt(col("b"), Param(int32_t{2}))->to_sql(params), std::string("b>$2"));
    ASSERT_EQ(lt(col("c"), Param(int32_t{3}))->to_sql(params), std::string("c<$3"));
    ASSERT_EQ(ge(col("d"), Param(int32_t{4}))->to_sql(params), std::string("d>=$4"));
    ASSERT_EQ(le(col("e"), Param(int32_t{5}))->to_sql(params), std::string("e<=$5"));
}

TEST(expr_like) {
    ParamList params;
    auto sql = like(col("name"), "%Alice%")->to_sql(params);
    ASSERT_EQ(sql, std::string("name LIKE $1"));
}

TEST(expr_is_null) {
    ParamList params;
    ASSERT_EQ(is_null(col("email"))->to_sql(params), std::string("email IS NULL"));
    ASSERT_EQ(is_not_null(col("phone"))->to_sql(params), std::string("phone IS NOT NULL"));
    ASSERT_EQ(params.size(), 0UL);
}

TEST(expr_in) {
    ParamList params;
    auto sql = in(col("status"), {Param(std::string("a")), Param(std::string("b"))})->to_sql(params);
    ASSERT_EQ(sql, std::string("status IN ($1, $2)"));
}

TEST(expr_and_or) {
    ParamList params;
    auto sql = and_(
        eq(col("a"), Param(int32_t{1})),
        or_(eq(col("b"), Param(int32_t{2})), eq(col("c"), Param(int32_t{3})))
    )->to_sql(params);
    ASSERT_EQ(sql, std::string("(a=$1 AND (b=$2 OR c=$3))"));
}

// ---- 新增: BETWEEN ----

TEST(expr_between) {
    ParamList params;
    auto sql = between(col("age"), Param(int32_t{18}), Param(int32_t{65}))->to_sql(params);
    ASSERT_EQ(sql, std::string("age BETWEEN $1 AND $2"));
    ASSERT_EQ(params.size(), 2UL);
}

TEST(expr_not_between) {
    ParamList params;
    auto sql = not_between(col("age"), Param(int32_t{18}), Param(int32_t{65}))->to_sql(params);
    ASSERT_EQ(sql, std::string("age NOT BETWEEN $1 AND $2"));
}

// ---- 新增: NOT ----

TEST(expr_not) {
    ParamList params;
    auto sql = not_(eq(col("status"), Param(std::string("inactive"))))->to_sql(params);
    ASSERT_EQ(sql, std::string("NOT (status=$1)"));
}

// ---- 新增: 多参数 and/or ----

TEST(expr_and_many) {
    ParamList params;
    std::vector<ExprPtr> exprs;
    exprs.push_back(eq(col("a"), Param(int32_t{1})));
    exprs.push_back(eq(col("b"), Param(int32_t{2})));
    exprs.push_back(eq(col("c"), Param(int32_t{3})));
    auto sql = and_many(std::move(exprs))->to_sql(params);
    ASSERT_EQ(sql, std::string("(a=$1 AND b=$2 AND c=$3)"));
}

TEST(expr_or_many) {
    ParamList params;
    std::vector<ExprPtr> exprs;
    exprs.push_back(eq(col("a"), Param(int32_t{1})));
    exprs.push_back(eq(col("b"), Param(int32_t{2})));
    exprs.push_back(eq(col("c"), Param(int32_t{3})));
    auto sql = or_many(std::move(exprs))->to_sql(params);
    ASSERT_EQ(sql, std::string("(a=$1 OR b=$2 OR c=$3)"));
}

TEST(param_to_string_all_types) {
    ASSERT_EQ(param_to_string(Param(std::monostate{})), std::string("NULL"));
    ASSERT_EQ(param_to_string(Param(true)), std::string("true"));
    ASSERT_EQ(param_to_string(Param(false)), std::string("false"));
    ASSERT_EQ(param_to_string(Param(int32_t{42})), std::string("42"));
    ASSERT_EQ(param_to_string(Param(int64_t{999})), std::string("999"));
    ASSERT_EQ(param_to_string(Param(std::string("hi"))), std::string("hi"));
}

// ============================================================
// 第二轮增强测试
// ============================================================

TEST(quote_ident_basic) {
    ASSERT_EQ(quote_ident("users"), std::string("\"users\""));
    ASSERT_EQ(quote_ident("order"), std::string("\"order\""));
}

TEST(quote_ident_escape) {
    // 双引号内部转义为两个双引号
    ASSERT_EQ(quote_ident("my\"col"), std::string("\"my\"\"col\""));
}

TEST(quote_literal_basic) {
    ASSERT_EQ(quote_literal("hello"), std::string("'hello'"));
    ASSERT_EQ(quote_literal("it's"), std::string("'it''s'"));
}

TEST(aggregate_count_star) {
    ASSERT_EQ(count(), std::string("COUNT(*)"));
    ASSERT_EQ(count("*"), std::string("COUNT(*)"));
}

TEST(aggregate_sum) {
    ASSERT_EQ(sum("amount"), std::string("SUM(amount)"));
}

TEST(aggregate_avg_as) {
    ASSERT_EQ(avg_as("price", "avg_price"), std::string("AVG(price) AS avg_price"));
}

TEST(aggregate_count_as) {
    ASSERT_EQ(count_as("*", "cnt"), std::string("COUNT(*) AS cnt"));
}

TEST(select_with_aggregate) {
    auto q = select()
        .columns({"user_id", count_as("*", "cnt")})
        .from("orders")
        .group_by({"user_id"})
        .build();
    ASSERT_EQ(q.sql, std::string("SELECT user_id, COUNT(*) AS cnt FROM orders GROUP BY user_id"));
}

TEST(subquery_exists) {
    auto sub = select().columns({"1"}).from("orders")
        .where(eq(col("user_id"), Param(int32_t{1}))).build();
    auto q = select().columns({"name"}).from("users")
        .where(exists(std::move(sub))).build();
    ASSERT_EQ(q.sql, std::string("SELECT name FROM users WHERE EXISTS (SELECT 1 FROM orders WHERE user_id=$1)"));
    ASSERT_EQ(q.params.size(), 1u);
}

TEST(subquery_not_exists) {
    auto sub = select().columns({"1"}).from("orders").build();
    auto q = select().columns({"name"}).from("users")
        .where(not_exists(std::move(sub))).build();
    ASSERT_EQ(q.sql, std::string("SELECT name FROM users WHERE NOT EXISTS (SELECT 1 FROM orders)"));
}

TEST(subquery_in) {
    auto sub = select().columns({"id"}).from("orders").build();
    auto q = select().columns({"name"}).from("users")
        .where(in_subquery(col("id"), std::move(sub))).build();
    ASSERT_EQ(q.sql, std::string("SELECT name FROM users WHERE id IN (SELECT id FROM orders)"));
}

TEST(subquery_not_in) {
    auto sub = select().columns({"id"}).from("orders").build();
    auto q = select().columns({"name"}).from("users")
        .where(not_in_subquery(col("id"), std::move(sub))).build();
    ASSERT_EQ(q.sql, std::string("SELECT name FROM users WHERE id NOT IN (SELECT id FROM orders)"));
}

TEST(subquery_exists_with_params_renumber) {
    // 父查询有1个参数, 子查询有2个参数
    // 子查询的 $1,$2 应重编号为 $2,$3
    auto sub = select().columns({"1"}).from("orders")
        .where(and_(
            eq(col("user_id"), Param(int32_t{1})),
            eq(col("status"), Param(std::string("paid")))
        )).build();
    auto q = select().columns({"name"}).from("users")
        .where(and_(
            eq(col("active"), Param(true)),
            exists(std::move(sub))
        )).build();
    ASSERT_EQ(q.sql, std::string(
        "SELECT name FROM users WHERE (active=$1 AND EXISTS ("
        "SELECT 1 FROM orders WHERE (user_id=$2 AND status=$3)))"));
    ASSERT_EQ(q.params.size(), 3u);
}

TEST(from_subquery) {
    auto sub = select().columns({"id", "name"}).from("users")
        .where(eq(col("active"), Param(true))).build();
    auto q = select().columns({"id", "name"})
        .from_subquery(std::move(sub), "u").build();
    ASSERT_EQ(q.sql, std::string(
        "SELECT id, name FROM (SELECT id, name FROM users WHERE active=$1) AS u"));
    ASSERT_EQ(q.params.size(), 1u);
}

TEST(cte_with) {
    auto cte = select().columns({"id", "amount"}).from("orders")
        .where(eq(col("status"), Param(std::string("paid")))).build();
    auto q = select()
        .with("paid_orders", std::move(cte))
        .columns({"id", "amount"})
        .from("paid_orders")
        .where(gt(col("amount"), Param(int32_t{100})))
        .build();
    ASSERT_EQ(q.sql, std::string(
        "WITH paid_orders AS (SELECT id, amount FROM orders WHERE status=$1) "
        "SELECT id, amount FROM paid_orders WHERE amount>$2"));
    ASSERT_EQ(q.params.size(), 2u);
}

TEST(cte_multiple) {
    auto cte1 = select().columns({"id"}).from("users").build();
    auto cte2 = select().columns({"id"}).from("orders").build();
    auto q = select()
        .with("u", std::move(cte1))
        .with("o", std::move(cte2))
        .columns({"*"}).from("u").build();
    ASSERT_EQ(q.sql, std::string(
        "WITH u AS (SELECT id FROM users), o AS (SELECT id FROM orders) "
        "SELECT * FROM u"));
}

TEST(union_basic) {
    auto q1 = select().columns({"id", "name"}).from("users").build();
    auto q2 = select().columns({"id", "name"}).from("archived_users").build();
    std::vector<Query> queries;
    queries.push_back(std::move(q1));
    queries.push_back(std::move(q2));
    auto result = union_(std::move(queries));
    ASSERT_EQ(result.sql, std::string(
        "SELECT id, name FROM users UNION SELECT id, name FROM archived_users"));
}

TEST(union_all_with_params) {
    auto q1 = select().columns({"id"}).from("users")
        .where(eq(col("active"), Param(true))).build();
    auto q2 = select().columns({"id"}).from("users")
        .where(eq(col("admin"), Param(true))).build();
    std::vector<Query> queries;
    queries.push_back(std::move(q1));
    queries.push_back(std::move(q2));
    auto result = union_all(std::move(queries));
    ASSERT_EQ(result.sql, std::string(
        "SELECT id FROM users WHERE active=$1 UNION ALL SELECT id FROM users WHERE admin=$2"));
    ASSERT_EQ(result.params.size(), 2u);
}

TEST(intersect_basic) {
    auto q1 = select().columns({"user_id"}).from("orders").build();
    auto q2 = select().columns({"user_id"}).from("reviews").build();
    std::vector<Query> queries;
    queries.push_back(std::move(q1));
    queries.push_back(std::move(q2));
    auto result = intersect(std::move(queries));
    ASSERT_EQ(result.sql, std::string(
        "SELECT user_id FROM orders INTERSECT SELECT user_id FROM reviews"));
}

TEST(except_basic) {
    auto q1 = select().columns({"id"}).from("users").build();
    auto q2 = select().columns({"id"}).from("banned_users").build();
    std::vector<Query> queries;
    queries.push_back(std::move(q1));
    queries.push_back(std::move(q2));
    auto result = except(std::move(queries));
    ASSERT_EQ(result.sql, std::string(
        "SELECT id FROM users EXCEPT SELECT id FROM banned_users"));
}

int main() {
    std::cout << "=== cppq SQL Builder Unit Tests ===\n";
    // Tests are auto-registered via static initializers above
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
