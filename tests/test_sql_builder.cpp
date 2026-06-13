#include <cppq/sql/Select.hpp>
#include <cppq/sql/Insert.hpp>
#include <cppq/sql/Update.hpp>
#include <cppq/sql/Delete.hpp>
#include <cppq/core/Column.hpp>
#include <cppq/core/Param.hpp>

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

TEST(param_to_string_all_types) {
    ASSERT_EQ(param_to_string(Param(std::monostate{})), std::string("NULL"));
    ASSERT_EQ(param_to_string(Param(true)), std::string("true"));
    ASSERT_EQ(param_to_string(Param(false)), std::string("false"));
    ASSERT_EQ(param_to_string(Param(int32_t{42})), std::string("42"));
    ASSERT_EQ(param_to_string(Param(int64_t{999})), std::string("999"));
    ASSERT_EQ(param_to_string(Param(std::string("hi"))), std::string("hi"));
}

int main() {
    std::cout << "=== cppq SQL Builder Unit Tests ===\n";
    // Tests are auto-registered via static initializers above
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
