#include <cppq/pg/Connection.hpp>
#include <cppq/pg/Result.hpp>
#include <cppq/sql/Select.hpp>
#include <cppq/sql/Insert.hpp>
#include <cppq/sql/Update.hpp>
#include <cppq/sql/Delete.hpp>

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

static const char* CONN_INFO = "host=localhost dbname=postgres user=postgres password=ac123456";

static Connection make_conn() {
    Connection conn(CONN_INFO);
    auto r = conn.connect();
    if (!r.has_value()) {
        throw std::runtime_error("Connection failed: " + r.error().message);
    }
    return conn;
}

static void setup_table(Connection& conn) {
    auto drop = conn.execute(Query{.sql = "DROP TABLE IF EXISTS cppq_test_users", .params = {}});
    ASSERT_TRUE(drop.has_value());

    auto create = conn.execute(Query{
        .sql = "CREATE TABLE cppq_test_users ("
               "  id SERIAL PRIMARY KEY,"
               "  name VARCHAR(100) NOT NULL,"
               "  phone VARCHAR(20),"
               "  age INT,"
               "  email VARCHAR(200),"
               "  created_at TIMESTAMP DEFAULT NOW()"
               ")",
        .params = {}
    });
    ASSERT_TRUE(create.has_value());
}

static void teardown_table(Connection& conn) {
    conn.execute(Query{.sql = "DROP TABLE IF EXISTS cppq_test_users", .params = {}});
}

// ============================================================
// Phase 3: Connection Tests
// ============================================================

TEST(conn_connect_disconnect) {
    auto conn = make_conn();
    ASSERT_TRUE(conn.is_connected());
    conn.disconnect();
    ASSERT_TRUE(!conn.is_connected());
}

TEST(conn_bad_password) {
    Connection conn("host=localhost dbname=postgres user=postgres password=wrong");
    auto r = conn.connect();
    ASSERT_TRUE(!r.has_value());
    ASSERT_TRUE(r.error().code == ErrorCode::ConnectionFailed);
}

TEST(conn_transaction_commit) {
    auto conn = make_conn();
    ASSERT_TRUE(conn.begin().has_value());
    ASSERT_TRUE(conn.commit().has_value());
}

TEST(conn_transaction_rollback) {
    auto conn = make_conn();
    ASSERT_TRUE(conn.begin().has_value());
    ASSERT_TRUE(conn.rollback().has_value());
}

// ============================================================
// Phase 4: Integration CRUD Tests
// ============================================================

TEST(integration_insert_select) {
    auto conn = make_conn();
    setup_table(conn);

    // INSERT
    auto ins_q = insert()
        .into("cppq_test_users")
        .columns({"name", "phone", "age"})
        .values({Param(std::string("Alice")), Param(std::string("13800001234")), Param(int32_t{25})})
        .returning({"id"})
        .build();

    auto ins_res = conn.execute(ins_q);
    ASSERT_TRUE(ins_res.has_value());
    ASSERT_EQ(ins_res->rows(), 1);
    auto id = ins_res->get(0, 0).value();

    // SELECT
    auto sel_q = select()
        .columns({"id", "name", "phone", "age"})
        .from("cppq_test_users")
        .where(eq(col("phone"), Param(std::string("13800001234"))))
        .build();

    auto sel_res = conn.execute(sel_q);
    ASSERT_TRUE(sel_res.has_value());
    ASSERT_EQ(sel_res->rows(), 1);
    ASSERT_EQ(sel_res->get(0, 1).value(), std::string("Alice"));
    ASSERT_EQ(sel_res->get(0, 2).value(), std::string("13800001234"));
    ASSERT_EQ(sel_res->get(0, 3).value(), std::string("25"));

    teardown_table(conn);
}

TEST(integration_update) {
    auto conn = make_conn();
    setup_table(conn);

    // Insert
    auto ins = insert()
        .into("cppq_test_users")
        .columns({"name", "age"})
        .values({Param(std::string("Alice")), Param(int32_t{25})})
        .returning({"id"})
        .build();
    auto ins_res = conn.execute(ins);
    ASSERT_TRUE(ins_res.has_value());
    auto id = std::stoll(ins_res->get(0, 0).value());

    // Update
    auto upd = update("cppq_test_users")
        .set("name", Param(std::string("Alice Updated")))
        .set("age", Param(int32_t{26}))
        .where(eq(col("id"), Param(int64_t{id})))
        .build();

    auto upd_res = conn.execute(upd);
    ASSERT_TRUE(upd_res.has_value());

    // Verify
    auto sel = select().columns({"name", "age"})
        .from("cppq_test_users")
        .where(eq(col("id"), Param(int64_t{id})))
        .build();
    auto sel_res = conn.execute(sel);
    ASSERT_TRUE(sel_res.has_value());
    ASSERT_EQ(sel_res->get(0, 0).value(), std::string("Alice Updated"));
    ASSERT_EQ(sel_res->get(0, 1).value(), std::string("26"));

    teardown_table(conn);
}

TEST(integration_delete) {
    auto conn = make_conn();
    setup_table(conn);

    // Insert 2 rows
    for (const auto& [n, p] : std::vector<std::pair<std::string, std::string>>{
        {"Alice", "111"}, {"Bob", "222"}}) {
        auto q = insert().into("cppq_test_users")
            .columns({"name", "phone"})
            .values({Param(n), Param(p)}).build();
        ASSERT_TRUE(conn.execute(q).has_value());
    }

    // Delete Alice
    auto del = delete_from().from("cppq_test_users")
        .where(eq(col("name"), Param(std::string("Alice")))).build();
    ASSERT_TRUE(conn.execute(del).has_value());

    // Verify
    auto sel = select().columns({"name"}).from("cppq_test_users").build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 1);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Bob"));

    teardown_table(conn);
}

TEST(integration_limit_offset) {
    auto conn = make_conn();
    setup_table(conn);

    for (int i = 1; i <= 5; ++i) {
        auto q = insert().into("cppq_test_users")
            .columns({"name", "age"})
            .values({Param(std::string("User") + std::to_string(i)), Param(int32_t{20 + i})}).build();
        ASSERT_TRUE(conn.execute(q).has_value());
    }

    auto sel = select().columns({"name"})
        .from("cppq_test_users")
        .order_by("id", Order::Asc)
        .limit(2).offset(1).build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 2);
    ASSERT_EQ(res->get(0, 0).value(), std::string("User2"));
    ASSERT_EQ(res->get(1, 0).value(), std::string("User3"));

    teardown_table(conn);
}

TEST(integration_like) {
    auto conn = make_conn();
    setup_table(conn);

    for (const auto& n : {"Alice", "Alicia", "Bob"}) {
        auto q = insert().into("cppq_test_users")
            .columns({"name"}).values({Param(std::string(n))}).build();
        ASSERT_TRUE(conn.execute(q).has_value());
    }

    auto sel = select().columns({"name"})
        .from("cppq_test_users")
        .where(like(col("name"), std::string("Ali%")))
        .order_by("name", Order::Asc).build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 2);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Alice"));
    ASSERT_EQ(res->get(1, 0).value(), std::string("Alicia"));

    teardown_table(conn);
}

TEST(integration_null_handling) {
    auto conn = make_conn();
    setup_table(conn);

    auto ins = insert().into("cppq_test_users")
        .columns({"name", "email"})
        .values({Param(std::string("Alice")), Param(std::monostate{})}).build();
    ASSERT_TRUE(conn.execute(ins).has_value());

    auto sel = select().columns({"name", "email"}).from("cppq_test_users").build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->get(0, 0).value(), std::string("Alice"));
    ASSERT_TRUE(!res->get(0, 1).has_value()); // email is NULL

    auto null_q = select().columns({"name"}).from("cppq_test_users")
        .where(is_null(col("email"))).build();
    auto null_res = conn.execute(null_q);
    ASSERT_TRUE(null_res.has_value());
    ASSERT_EQ(null_res->rows(), 1);

    teardown_table(conn);
}

TEST(integration_sql_injection_prevention) {
    auto conn = make_conn();
    setup_table(conn);

    auto ins = insert().into("cppq_test_users")
        .columns({"name", "phone"})
        .values({Param(std::string("Alice")), Param(std::string("123"))}).build();
    ASSERT_TRUE(conn.execute(ins).has_value());

    // Evil string passed as PARAMETER, never inline
    auto evil = select().columns({"name"}).from("cppq_test_users")
        .where(eq(col("phone"), Param(std::string("123' OR '1'='1")))).build();

    // Verify SQL uses placeholder, not inline value
    ASSERT_EQ(evil.sql, std::string("SELECT name FROM cppq_test_users WHERE phone=$1"));
    ASSERT_EQ(evil.params.size(), 1UL);

    auto res = conn.execute(evil);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 0); // No match for the evil string

    teardown_table(conn);
}

TEST(integration_transaction_rollback) {
    auto conn = make_conn();
    setup_table(conn);

    // Insert permanent row
    auto ins1 = insert().into("cppq_test_users")
        .columns({"name"}).values({Param(std::string("Permanent"))}).build();
    ASSERT_TRUE(conn.execute(ins1).has_value());

    // Transaction: insert then rollback
    conn.begin();
    auto ins2 = insert().into("cppq_test_users")
        .columns({"name"}).values({Param(std::string("RolledBack"))}).build();
    ASSERT_TRUE(conn.execute(ins2).has_value());
    conn.rollback();

    auto sel = select().columns({"name"}).from("cppq_test_users").build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 1);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Permanent"));

    teardown_table(conn);
}

int main() {
    std::cout << "=== cppq Integration Tests (PostgreSQL) ===\n";
    // Tests auto-registered via static initializers
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
