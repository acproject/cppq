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

static void setup_json_table(Connection& conn) {
    auto drop = conn.execute(Query{.sql = "DROP TABLE IF EXISTS cppq_test_json", .params = {}});
    ASSERT_TRUE(drop.has_value());

    auto create = conn.execute(Query{
        .sql = "CREATE TABLE cppq_test_json ("
               "  id SERIAL PRIMARY KEY,"
               "  name VARCHAR(100),"
               "  data JSON,"
               "  data_b JSONB"
               ")",
        .params = {}
    });
    ASSERT_TRUE(create.has_value());
}

static void teardown_json_table(Connection& conn) {
    conn.execute(Query{.sql = "DROP TABLE IF EXISTS cppq_test_json", .params = {}});
}

// ============================================================
// JSON/JSONB Tests
// ============================================================

TEST(json_insert_and_read) {
    auto conn = make_conn();
    setup_json_table(conn);

    // Insert with json() and jsonb() helpers
    auto ins = insert()
        .into("cppq_test_json")
        .columns({"name", "data", "data_b"})
        .values({
            Param(std::string("Alice")),
            json("{\"age\":25,\"city\":\"Beijing\"}"),
            jsonb("{\"role\":\"admin\",\"tags\":[\"a\",\"b\"]}")
        })
        .returning({"id"})
        .build();

    auto ins_res = conn.execute(ins);
    ASSERT_TRUE(ins_res.has_value());
    ASSERT_EQ(ins_res->rows(), 1);

    // Read back
    auto sel = select()
        .columns({"name", "data", "data_b"})
        .from("cppq_test_json")
        .build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 1);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Alice"));
    // JSON data returned as string
    auto data = res->get(0, 1).value();
    ASSERT_TRUE(data.find("age") != std::string::npos);
    ASSERT_TRUE(data.find("25") != std::string::npos);

    auto data_b = res->get(0, 2).value();
    ASSERT_TRUE(data_b.find("role") != std::string::npos);
    ASSERT_TRUE(data_b.find("admin") != std::string::npos);

    teardown_json_table(conn);
}

TEST(json_column_type_detection) {
    auto conn = make_conn();
    setup_json_table(conn);

    auto ins = insert()
        .into("cppq_test_json")
        .columns({"name", "data", "data_b"})
        .values({
            Param(std::string("Bob")),
            json("{\"x\":1}"),
            jsonb("{\"y\":2}")
        })
        .build();
    ASSERT_TRUE(conn.execute(ins).has_value());

    auto sel = select().columns({"name", "data", "data_b"}).from("cppq_test_json").build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());

    // Column type OID detection
    // col 0 = name (VARCHAR), not JSON
    ASSERT_TRUE(!res->is_json_type(0));
    // col 1 = data (JSON, OID 114)
    ASSERT_TRUE(res->is_json(1));
    ASSERT_TRUE(res->is_json_type(1));
    ASSERT_TRUE(!res->is_jsonb(1));
    // col 2 = data_b (JSONB, OID 3802)
    ASSERT_TRUE(res->is_jsonb(2));
    ASSERT_TRUE(res->is_json_type(2));
    ASSERT_TRUE(!res->is_json(2));

    teardown_json_table(conn);
}

TEST(jsonb_contains_query) {
    auto conn = make_conn();
    setup_json_table(conn);

    // Insert test data
    for (const auto& [name, jb] : std::vector<std::pair<std::string, std::string>>{
        {"Alice", "{\"role\":\"admin\",\"level\":3}"},
        {"Bob", "{\"role\":\"user\",\"level\":1}"},
        {"Charlie", "{\"role\":\"admin\",\"level\":5}"}}) {
        auto q = insert().into("cppq_test_json")
            .columns({"name", "data_b"})
            .values({Param(name), jsonb(jb)}).build();
        ASSERT_TRUE(conn.execute(q).has_value());
    }

    // JSONB @> contains query
    auto sel = select().columns({"name"})
        .from("cppq_test_json")
        .where(json_contains(col("data_b"), jsonb("{\"role\":\"admin\"}")))
        .order_by("name", Order::Asc)
        .build();

    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 2);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Alice"));
    ASSERT_EQ(res->get(1, 0).value(), std::string("Charlie"));

    teardown_json_table(conn);
}

TEST(json_field_extract_query) {
    auto conn = make_conn();
    setup_json_table(conn);

    for (const auto& [name, jb] : std::vector<std::pair<std::string, std::string>>{
        {"Alice", "{\"city\":\"Beijing\"}"},
        {"Bob", "{\"city\":\"Shanghai\"}"},
        {"Charlie", "{\"city\":\"Beijing\"}"}}) {
        auto q = insert().into("cppq_test_json")
            .columns({"name", "data_b"})
            .values({Param(name), jsonb(jb)}).build();
        ASSERT_TRUE(conn.execute(q).has_value());
    }

    // data_b->>'city' = 'Beijing'
    auto sel = select().columns({"name"})
        .from("cppq_test_json")
        .where(json_field_eq(col("data_b"), "city", Param(std::string("Beijing"))))
        .order_by("name", Order::Asc)
        .build();

    // Verify SQL generation
    ASSERT_EQ(sel.sql, std::string(
        "SELECT name FROM cppq_test_json WHERE data_b->>$1 = $2 ORDER BY name ASC"));
    ASSERT_EQ(sel.params.size(), 2UL);
    // First param = field name "city", second = "Beijing"

    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 2);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Alice"));
    ASSERT_EQ(res->get(1, 0).value(), std::string("Charlie"));

    teardown_json_table(conn);
}

TEST(json_update) {
    auto conn = make_conn();
    setup_json_table(conn);

    auto ins = insert()
        .into("cppq_test_json")
        .columns({"name", "data_b"})
        .values({Param(std::string("Alice")), jsonb("{\"role\":\"user\"}")})
        .returning({"id"})
        .build();
    auto ins_res = conn.execute(ins);
    ASSERT_TRUE(ins_res.has_value());
    auto id = std::stoll(ins_res->get(0, 0).value());

    // Update JSONB field
    auto upd = update("cppq_test_json")
        .set("data_b", jsonb("{\"role\":\"admin\",\"level\":5}"))
        .where(eq(col("id"), Param(int64_t{id})))
        .build();
    ASSERT_TRUE(conn.execute(upd).has_value());

    // Verify
    auto sel = select().columns({"data_b"}).from("cppq_test_json")
        .where(eq(col("id"), Param(int64_t{id}))).build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    auto val = res->get(0, 0).value();
    ASSERT_TRUE(val.find("admin") != std::string::npos);
    ASSERT_TRUE(val.find("5") != std::string::npos);

    teardown_json_table(conn);
}

TEST(json_exists_key) {
    auto conn = make_conn();
    setup_json_table(conn);

    for (const auto& [name, jb] : std::vector<std::pair<std::string, std::string>>{
        {"Alice", "{\"email\":\"a@x.com\",\"phone\":\"111\"}"},
        {"Bob", "{\"email\":\"b@x.com\"}"},
        {"Charlie", "{\"phone\":\"333\"}"}}) {
        auto q = insert().into("cppq_test_json")
            .columns({"name", "data_b"})
            .values({Param(name), jsonb(jb)}).build();
        ASSERT_TRUE(conn.execute(q).has_value());
    }

    // data_b ? 'phone' - rows that have "phone" key
    auto sel = select().columns({"name"})
        .from("cppq_test_json")
        .where(json_exists(col("data_b"), "phone"))
        .order_by("name", Order::Asc)
        .build();

    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 2);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Alice"));
    ASSERT_EQ(res->get(1, 0).value(), std::string("Charlie"));

    teardown_json_table(conn);
}

TEST(json_sql_generation) {
    // Test SQL generation without database - pure unit test
    auto sel = select().columns({"id", "data_b"})
        .from("test_table")
        .where(and_(
            json_contains(col("data_b"), jsonb("{\"status\":\"active\"}")),
            json_field_eq(col("data_b"), "city", Param(std::string("Beijing")))
        ))
        .build();

    // Verify SQL uses parameters, never inlines JSON
    ASSERT_TRUE(sel.sql.find("@>") != std::string::npos);
    ASSERT_TRUE(sel.sql.find("->>") != std::string::npos);
    ASSERT_EQ(sel.params.size(), 3UL); // jsonb value, field name, field value
}

TEST(json_null_handling) {
    auto conn = make_conn();
    setup_json_table(conn);

    // Insert NULL JSON
    auto ins = insert()
        .into("cppq_test_json")
        .columns({"name", "data", "data_b"})
        .values({
            Param(std::string("NullTest")),
            Param(std::monostate{}),
            Param(std::monostate{})
        })
        .build();
    ASSERT_TRUE(conn.execute(ins).has_value());

    auto sel = select().columns({"name", "data", "data_b"}).from("cppq_test_json").build();
    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 1);
    ASSERT_EQ(res->get(0, 0).value(), std::string("NullTest"));
    ASSERT_TRUE(!res->get(0, 1).has_value()); // data is NULL
    ASSERT_TRUE(!res->get(0, 2).has_value()); // data_b is NULL

    teardown_json_table(conn);
}

TEST(json_combined_where) {
    auto conn = make_conn();
    setup_json_table(conn);

    for (const auto& [name, jb] : std::vector<std::pair<std::string, std::string>>{
        {"Alice", "{\"role\":\"admin\",\"active\":true}"},
        {"Bob", "{\"role\":\"admin\",\"active\":false}"},
        {"Charlie", "{\"role\":\"user\",\"active\":true}"}}) {
        auto q = insert().into("cppq_test_json")
            .columns({"name", "data_b"})
            .values({Param(name), jsonb(jb)}).build();
        ASSERT_TRUE(conn.execute(q).has_value());
    }

    // Combined: regular WHERE + JSON WHERE
    auto sel = select().columns({"name"})
        .from("cppq_test_json")
        .where(and_(
            json_contains(col("data_b"), jsonb("{\"role\":\"admin\"}")),
            json_field_eq(col("data_b"), "active", Param(std::string("true")))
        ))
        .build();

    auto res = conn.execute(sel);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->rows(), 1);
    ASSERT_EQ(res->get(0, 0).value(), std::string("Alice"));

    teardown_json_table(conn);
}

int main() {
    std::cout << "=== cppq JSON/JSONB Tests ===\n";
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
