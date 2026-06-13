#!/usr/bin/env python3
"""cppq JSON/JSONB Python binding tests."""

import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "bindings", "python"))
from cppq import select, insert, update, connect

CONN_INFO = "host=localhost dbname=postgres user=postgres password=ac123456"

passed = 0
failed = 0

def test(name):
    def decorator(fn):
        global passed, failed
        try:
            fn()
            print(f"  {name}... PASS")
            passed += 1
        except Exception as e:
            print(f"  {name}... FAIL: {e}")
            failed += 1
    return decorator

def run_sql(sql):
    """Run SQL via psql."""
    result = subprocess.run(
        ["/usr/local/pgsql/bin/psql", "-h", "localhost", "-U", "postgres",
         "-d", "postgres", "-c", sql],
        capture_output=True, text=True,
        env={**os.environ, "PGPASSWORD": "ac123456"}
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip())
    return result.stdout.strip()

@test("json_python_full_workflow")
def _():
    run_sql("DROP TABLE IF EXISTS cppq_py_json")
    run_sql(
        "CREATE TABLE cppq_py_json ("
        "  id SERIAL PRIMARY KEY, name VARCHAR(100), data JSON, data_b JSONB)"
    )

    conn = connect(CONN_INFO)
    try:
        # Insert with JSON value (auto-serializes dict)
        q = insert("cppq_py_json", "name", "data", "data_b") \
            .value_str("Alice") \
            .value_json({"age": 25, "city": "Beijing"}) \
            .value_jsonb({"role": "admin", "tags": ["x", "y"]}) \
            .returning("id")
        res = conn.execute(q)
        assert res.row_count == 1

        # Read back - JSON columns auto-parsed to dict
        sel = select("name", "data", "data_b").from_("cppq_py_json")
        res = conn.execute(sel)
        assert res.row_count == 1
        row = list(res)[0]
        assert row["name"] == "Alice"
        assert isinstance(row["data"], dict), f"Expected dict, got {type(row['data'])}"
        assert row["data"]["age"] == 25
        assert isinstance(row["data_b"], dict), f"Expected dict, got {type(row['data_b'])}"
        assert row["data_b"]["role"] == "admin"
        print(f"    row: {row}")
    finally:
        conn.close()
        run_sql("DROP TABLE IF EXISTS cppq_py_json")

@test("json_python_contains_query")
def _():
    run_sql("DROP TABLE IF EXISTS cppq_py_json")
    run_sql(
        "CREATE TABLE cppq_py_json ("
        "  id SERIAL PRIMARY KEY, name VARCHAR(100), data_b JSONB)"
    )

    for name, jb in [("Alice", '{"role":"admin"}'), ("Bob", '{"role":"user"}'), ("Charlie", '{"role":"admin"}')]:
        run_sql(f"INSERT INTO cppq_py_json (name, data_b) VALUES ('{name}', '{jb}')")

    conn = connect(CONN_INFO)
    try:
        sel = select("name").from_("cppq_py_json") \
            .where_json_contains("data_b", {"role": "admin"}) \
            .order_by("name")

        print(f"    SQL: {sel.sql}")
        res = conn.execute(sel)
        assert res.row_count == 2
        rows = list(res)
        assert rows[0]["name"] == "Alice"
        assert rows[1]["name"] == "Charlie"
        print(f"    rows: {rows}")
    finally:
        conn.close()
        run_sql("DROP TABLE IF EXISTS cppq_py_json")

@test("json_python_field_eq_query")
def _():
    run_sql("DROP TABLE IF EXISTS cppq_py_json")
    run_sql(
        "CREATE TABLE cppq_py_json ("
        "  id SERIAL PRIMARY KEY, name VARCHAR(100), data_b JSONB)"
    )

    for name, city in [("Alice", "Beijing"), ("Bob", "Shanghai"), ("Charlie", "Beijing")]:
        run_sql(f"INSERT INTO cppq_py_json (name, data_b) VALUES ('{name}', '{{\"city\":\"{city}\"}}')")

    conn = connect(CONN_INFO)
    try:
        sel = select("name").from_("cppq_py_json") \
            .where_json_field_eq("data_b", "city", "Beijing") \
            .order_by("name")

        print(f"    SQL: {sel.sql}")
        res = conn.execute(sel)
        assert res.row_count == 2
        rows = list(res)
        assert rows[0]["name"] == "Alice"
        assert rows[1]["name"] == "Charlie"
    finally:
        conn.close()
        run_sql("DROP TABLE IF EXISTS cppq_py_json")

@test("json_python_update")
def _():
    run_sql("DROP TABLE IF EXISTS cppq_py_json")
    run_sql(
        "CREATE TABLE cppq_py_json ("
        "  id SERIAL PRIMARY KEY, name VARCHAR(100), data_b JSONB)"
    )
    run_sql("INSERT INTO cppq_py_json (name, data_b) VALUES ('Alice', '{\"role\":\"user\"}')")

    conn = connect(CONN_INFO)
    try:
        upd = update("cppq_py_json") \
            .set_jsonb("data_b", {"role": "admin", "level": 5}) \
            .where_eq_str("name", "Alice")
        print(f"    SQL: {upd.sql}")
        conn.execute(upd)

        sel = select("data_b").from_("cppq_py_json").where_eq_str("name", "Alice")
        res = conn.execute(sel)
        rows = list(res)
        assert len(rows) == 1
        assert isinstance(rows[0]["data_b"], dict)
        assert rows[0]["data_b"]["role"] == "admin"
        assert rows[0]["data_b"]["level"] == 5
        print(f"    updated: {rows[0]}")
    finally:
        conn.close()
        run_sql("DROP TABLE IF EXISTS cppq_py_json")

@test("json_python_col_type_detection")
def _():
    run_sql("DROP TABLE IF EXISTS cppq_py_json")
    run_sql(
        "CREATE TABLE cppq_py_json ("
        "  id SERIAL PRIMARY KEY, name VARCHAR(100), data JSON, data_b JSONB)"
    )
    run_sql("INSERT INTO cppq_py_json (name, data, data_b) VALUES ('Test', '{\"a\":1}', '{\"b\":2}')")

    conn = connect(CONN_INFO)
    try:
        sel = select("name", "data", "data_b").from_("cppq_py_json")
        res = conn.execute(sel)
        assert res.is_json(0) == False  # name is VARCHAR
        assert res.is_json(1) == True   # data is JSON
        assert res.is_json(2) == True   # data_b is JSONB
        print(f"    col_types: name={res.col_type(0)}, data={res.col_type(1)}, data_b={res.col_type(2)}")

        # get_json helper
        parsed = res.get_json(0, 1)
        assert isinstance(parsed, dict)
        assert parsed["a"] == 1
    finally:
        conn.close()
        run_sql("DROP TABLE IF EXISTS cppq_py_json")

print("=== cppq Python JSON/JSONB Tests ===")
print(f"\nResults: {passed} passed, {failed} failed")
sys.exit(1 if failed > 0 else 0)
