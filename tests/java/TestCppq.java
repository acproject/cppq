package com.cppq;

import com.cppq.Cppq.*;
import java.io.*;
import java.util.*;

/**
 * Comprehensive tests for cppq Java binding.
 * Covers: SQL builder, query introspection, connection, CRUD, JSON/JSONB.
 *
 * Run with:
 *   java --enable-preview --enable-native-access=ALL-UNNAMED TestCppq
 */
public class TestCppq {

    static int passed = 0;
    static int failed = 0;
    static final String CONN_INFO = "host=localhost dbname=postgres user=postgres password=ac123456";

    // ============================================================
    // Test helpers
    // ============================================================
    interface TestFn { void run() throws Exception; }

    static void test(String name, TestFn fn) {
        System.out.print("  " + name + "... ");
        try {
            fn.run();
            System.out.println("PASS");
            passed++;
        } catch (Throwable e) {
            System.out.println("FAIL: " + e.getMessage());
            failed++;
        }
    }

    static void assertEquals(Object actual, Object expected) {
        if (!Objects.equals(actual, expected)) {
            throw new AssertionError("Expected: " + expected + ", Actual: " + actual);
        }
    }

    static void assertTrue(boolean cond) {
        if (!cond) throw new AssertionError("Assertion failed");
    }

    static void assertContains(String haystack, String needle) {
        if (haystack == null || !haystack.contains(needle)) {
            throw new AssertionError("Expected '" + haystack + "' to contain '" + needle + "'");
        }
    }

    static void runSql(String sql) throws Exception {
        var env = new HashMap<>(System.getenv());
        env.put("PGPASSWORD", "ac123456");
        var pb = new ProcessBuilder("/usr/local/pgsql/bin/psql",
            "-h", "localhost", "-U", "postgres", "-d", "postgres", "-c", sql);
        pb.environment().putAll(env);
        pb.redirectErrorStream(true);
        var proc = pb.start();
        proc.getInputStream().readAllBytes();
        int rc = proc.waitFor();
        if (rc != 0 && !sql.contains("DROP TABLE IF EXISTS")) {
            throw new RuntimeException("psql failed: " + sql);
        }
    }

    // ============================================================
    // SQL Builder Tests (no database needed)
    // ============================================================
    static void sqlBuilderTests() {
        test("select_basic", () -> {
            try (var q = Cppq.select("id", "name").from("users").limit(10)) {
                assertEquals(q.getSql(), "SELECT id, name FROM users LIMIT $1");
                assertEquals(q.getParamCount(), 1);
                assertEquals(q.getParamStr(0), "10");
            }
        });

        test("select_where_eq", () -> {
            try (var q = Cppq.select("id", "name").from("users").whereEqStr("phone", "13800001234")) {
                assertEquals(q.getSql(), "SELECT id, name FROM users WHERE phone=$1");
                assertEquals(q.getParamCount(), 1);
                assertEquals(q.getParamStr(0), "13800001234");
            }
        });

        test("select_where_int", () -> {
            try (var q = Cppq.select("*").from("users").whereGtInt("id", 100)) {
                assertContains(q.getSql(), "WHERE id>$1");
                assertEquals(q.getParamStr(0), "100");
            }
        });

        test("select_order_limit_offset", () -> {
            try (var q = Cppq.select("name").from("users").orderBy("name", true).limit(5).offset(10)) {
                assertContains(q.getSql(), "ORDER BY name ASC");
                assertContains(q.getSql(), "LIMIT $");
                assertContains(q.getSql(), "OFFSET $");
            }
        });

        test("select_where_like", () -> {
            try (var q = Cppq.select("name").from("users").whereLike("name", "Ali%")) {
                assertContains(q.getSql(), "LIKE");
                assertEquals(q.getParamStr(0), "Ali%");
            }
        });

        test("insert_basic", () -> {
            try (var q = Cppq.insert("users", "name", "age")
                    .valueStr("Alice").valueInt(25)) {
                assertContains(q.getSql(), "INSERT INTO users");
                assertContains(q.getSql(), "VALUES");
                assertEquals(q.getParamStr(0), "Alice");
                assertEquals(q.getParamStr(1), "25");
            }
        });

        test("insert_returning", () -> {
            try (var q = Cppq.insert("users", "name")
                    .valueStr("Alice").returning("id")) {
                assertContains(q.getSql(), "RETURNING id");
            }
        });

        test("insert_null_and_json", () -> {
            try (var q = Cppq.insert("users", "name", "email", "data")
                    .valueStr("Alice").valueNull().valueJsonb("{\"role\":\"admin\"}")) {
                assertEquals(q.getParamCount(), 3);
                assertTrue(q.isParamNull(1));
                assertEquals(q.getParamStr(2), "{\"role\":\"admin\"}");
            }
        });

        test("update_basic", () -> {
            try (var q = Cppq.update("users")
                    .setStr("name", "Bob").setInt("age", 30).whereEqInt("id", 1)) {
                assertContains(q.getSql(), "UPDATE users SET");
                assertContains(q.getSql(), "WHERE id=$");
            }
        });

        test("delete_basic", () -> {
            try (var q = Cppq.delete("users").whereEqStr("name", "Alice")) {
                assertContains(q.getSql(), "DELETE FROM users");
                assertContains(q.getSql(), "WHERE name=$1");
                assertEquals(q.getParamStr(0), "Alice");
            }
        });

        test("json_contains_query", () -> {
            try (var q = Cppq.select("name").from("users")
                    .whereJsonContains("data", "{\"role\":\"admin\"}")) {
                assertContains(q.getSql(), "@>");
                assertEquals(q.getParamStr(0), "{\"role\":\"admin\"}");
            }
        });

        test("json_field_eq_query", () -> {
            try (var q = Cppq.select("name").from("users")
                    .whereJsonFieldEq("data", "city", "Beijing")) {
                assertContains(q.getSql(), "->>");
            }
        });
    }

    // ============================================================
    // Integration Tests (needs PostgreSQL)
    // ============================================================
    static void integrationTests() throws Exception {
        // Setup table
        runSql("DROP TABLE IF EXISTS cppq_java_test");
        runSql("CREATE TABLE cppq_java_test (" +
               "  id SERIAL PRIMARY KEY," +
               "  name VARCHAR(100)," +
               "  phone VARCHAR(20)," +
               "  age INT," +
               "  data JSONB" +
               ")");

        test("java_connect_disconnect", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                assertTrue(conn.isConnected());
            }
        });

        test("java_insert_and_select", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                // Insert
                try (var q = Cppq.insert("cppq_java_test", "name", "phone", "age")
                        .valueStr("Alice").valueStr("13800001234").valueInt(25)
                        .returning("id")) {
                    try (var res = conn.execute(q)) {
                        assertEquals(res.getRowCount(), 1);
                        String id = res.get(0, 0);
                        assertTrue(id != null && !id.isEmpty());
                    }
                }
                // Select
                try (var q = Cppq.select("name", "phone", "age")
                        .from("cppq_java_test").whereEqStr("phone", "13800001234")) {
                    try (var res = conn.execute(q)) {
                        assertEquals(res.getRowCount(), 1);
                        assertEquals(res.get(0, 0), "Alice");
                        assertEquals(res.get(0, 1), "13800001234");
                        assertEquals(res.get(0, 2), "25");
                    }
                }
            }
        });

        test("java_update", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                // Insert
                try (var q = Cppq.insert("cppq_java_test", "name", "age")
                        .valueStr("Bob").valueInt(20).returning("id")) {
                    try (var res = conn.execute(q)) {
                        long id = Long.parseLong(res.get(0, 0));
                        // Update
                        try (var upd = Cppq.update("cppq_java_test")
                                .setStr("name", "Bob Updated").setInt("age", 21)
                                .whereEqInt("id", id)) {
                            conn.execute(upd).close();
                        }
                        // Verify
                        try (var sel = Cppq.select("name", "age")
                                .from("cppq_java_test").whereEqInt("id", id)) {
                            try (var res2 = conn.execute(sel)) {
                                assertEquals(res2.get(0, 0), "Bob Updated");
                                assertEquals(res2.get(0, 1), "21");
                            }
                        }
                    }
                }
            }
        });

        test("java_delete", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                // Insert
                try (var q = Cppq.insert("cppq_java_test", "name")
                        .valueStr("ToDelete")) {
                    conn.execute(q).close();
                }
                // Delete
                try (var q = Cppq.delete("cppq_java_test").whereEqStr("name", "ToDelete")) {
                    conn.execute(q).close();
                }
                // Verify
                try (var q = Cppq.select("name").from("cppq_java_test")
                        .whereEqStr("name", "ToDelete")) {
                    try (var res = conn.execute(q)) {
                        assertEquals(res.getRowCount(), 0);
                    }
                }
            }
        });

        test("java_null_handling", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                try (var q = Cppq.insert("cppq_java_test", "name", "phone")
                        .valueStr("NullPhone").valueNull()) {
                    conn.execute(q).close();
                }
                try (var q = Cppq.select("name", "phone").from("cppq_java_test")
                        .whereEqStr("name", "NullPhone")) {
                    try (var res = conn.execute(q)) {
                        assertEquals(res.getRowCount(), 1);
                        assertEquals(res.get(0, 0), "NullPhone");
                        assertEquals(res.get(0, 1), null);
                    }
                }
            }
        });

        test("java_transaction_rollback", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                conn.begin();
                try (var q = Cppq.insert("cppq_java_test", "name")
                        .valueStr("RolledBack")) {
                    conn.execute(q).close();
                }
                conn.rollback();
                // Verify row was not committed
                try (var q = Cppq.select("name").from("cppq_java_test")
                        .whereEqStr("name", "RolledBack")) {
                    try (var res = conn.execute(q)) {
                        assertEquals(res.getRowCount(), 0);
                    }
                }
            }
        });

        test("java_jsonb_insert_and_query", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                // Insert with JSONB
                try (var q = Cppq.insert("cppq_java_test", "name", "data")
                        .valueStr("JsonUser")
                        .valueJsonb("{\"role\":\"admin\",\"city\":\"Beijing\"}")
                        .returning("id")) {
                    try (var res = conn.execute(q)) {
                        assertEquals(res.getRowCount(), 1);
                    }
                }
                // Read back
                try (var q = Cppq.select("name", "data").from("cppq_java_test")
                        .whereEqStr("name", "JsonUser")) {
                    try (var res = conn.execute(q)) {
                        assertEquals(res.getRowCount(), 1);
                        assertEquals(res.get(0, 0), "JsonUser");
                        String data = res.get(0, 1);
                        assertContains(data, "admin");
                        assertContains(data, "Beijing");
                        // Check JSON column type detection
                        assertTrue(res.isJson(1));
                    }
                }
                // JSONB @> contains query
                try (var q = Cppq.select("name").from("cppq_java_test")
                        .whereJsonContains("data", "{\"role\":\"admin\"}")) {
                    try (var res = conn.execute(q)) {
                        assertTrue(res.getRowCount() >= 1);
                        assertEquals(res.get(0, 0), "JsonUser");
                    }
                }
                // JSON field equality
                try (var q = Cppq.select("name").from("cppq_java_test")
                        .whereJsonFieldEq("data", "city", "Beijing")) {
                    try (var res = conn.execute(q)) {
                        assertTrue(res.getRowCount() >= 1);
                    }
                }
            }
        });

        test("java_result_to_list", () -> {
            try (var conn = Cppq.connect(CONN_INFO)) {
                try (var q = Cppq.select("name").from("cppq_java_test").orderBy("name", true).limit(3)) {
                    try (var res = conn.execute(q)) {
                        var rows = res.toList();
                        assertTrue(rows.size() >= 1);
                        assertTrue(rows.get(0).containsKey("name"));
                    }
                }
            }
        });

        // Cleanup
        runSql("DROP TABLE IF EXISTS cppq_java_test");
    }

    // ============================================================
    // Main
    // ============================================================
    public static void main(String[] args) throws Exception {
        System.out.println("=== cppq Java Binding Tests ===\n");
        System.out.println("--- SQL Builder Tests (no DB) ---");
        sqlBuilderTests();
        System.out.println("\n--- Integration Tests (PostgreSQL) ---");
        integrationTests();
        System.out.println("\nResults: " + passed + " passed, " + failed + " failed");
        if (failed > 0) System.exit(1);
    }
}
