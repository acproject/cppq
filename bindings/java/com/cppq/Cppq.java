package com.cppq;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Path;
import java.util.*;

/**
 * Java binding for cppq PostgreSQL DSL library.
 * Uses Java 21 Foreign Function & Memory API (JEP 454) - zero dependencies.
 *
 * Example:
 *   var q = Cppq.select("id", "name").from("users").whereEqStr("phone", "138").limit(10);
 *   System.out.println(q.getSql());    // SELECT id, name FROM users WHERE phone=$1 LIMIT $2
 *   System.out.println(q.getParams()); // [13800001234, 10]
 */
public class Cppq implements AutoCloseable {

    // ============================================================
    // Native interface
    // ============================================================
    private static final Linker LINKER = Linker.nativeLinker();
    private static final SymbolLookup LIB;
    private static final ValueLayout C_INT = ValueLayout.JAVA_INT;
    private static final ValueLayout C_INT64 = ValueLayout.JAVA_LONG;
    private static final ValueLayout C_DOUBLE = ValueLayout.JAVA_DOUBLE;
    private static final AddressLayout ADDR = ValueLayout.ADDRESS;
    private static final Arena GLOBAL = Arena.ofAuto();

    static {
        String libName = System.mapLibraryName("cppq_ffi"); // libcppq_ffi.dylib on macOS
        Path baseDir = Path.of(Cppq.class.getProtectionDomain().getCodeSource().getLocation().getPath())
                           .getParent();
        Path libPath = null;
        for (String sub : new String[]{"build/lib", "build/src", "build", "."}) {
            Path p = baseDir.resolve(sub).resolve(libName);
            if (p.toFile().exists()) { libPath = p; break; }
        }
        if (libPath == null) {
            // Try absolute fallback
            Path fallback = Path.of("build/lib").resolve(libName).toAbsolutePath();
            if (fallback.toFile().exists()) libPath = fallback;
        }
        if (libPath == null) throw new RuntimeException("Cannot find " + libName);
        LIB = SymbolLookup.libraryLookup(libPath, GLOBAL);
    }

    private static MethodHandle func(String name, ValueLayout ret, ValueLayout... params) {
        var list = new java.util.ArrayList<MemoryLayout>(params.length);
        for (var p : params) list.add(p);
        var desc = FunctionDescriptor.of(ret, list.toArray(new MemoryLayout[0]));
        return LINKER.downcallHandle(LIB.find(name).orElseThrow(() ->
            new RuntimeException("Symbol not found: " + name)), desc);
    }
    private static MethodHandle func(String name, ValueLayout ret) {
        var desc = FunctionDescriptor.of(ret);
        return LINKER.downcallHandle(LIB.find(name).orElseThrow(() ->
            new RuntimeException("Symbol not found: " + name)), desc);
    }
    private static MethodHandle funcVoid(String name, ValueLayout... params) {
        var list = new java.util.ArrayList<MemoryLayout>(params.length);
        for (var p : params) list.add(p);
        var desc = FunctionDescriptor.ofVoid(list.toArray(new MemoryLayout[0]));
        return LINKER.downcallHandle(LIB.find(name).orElseThrow(() ->
            new RuntimeException("Symbol not found: " + name)), desc);
    }
    private static MethodHandle funcVoid(String name) {
        var desc = FunctionDescriptor.ofVoid();
        return LINKER.downcallHandle(LIB.find(name).orElseThrow(() ->
            new RuntimeException("Symbol not found: " + name)), desc);
    }

    // -- Function handles --
    // SELECT (void return functions use funcVoid)
    private static final MethodHandle H_SELECT           = func("cppq_select", ADDR, ADDR, C_INT);
    private static final MethodHandle H_SELECT_FROM      = funcVoid("cppq_select_from", ADDR, ADDR);
    private static final MethodHandle H_SEL_WHERE_EQ_S   = funcVoid("cppq_select_where_eq_str", ADDR, ADDR, ADDR);
    private static final MethodHandle H_SEL_WHERE_EQ_I   = funcVoid("cppq_select_where_eq_int", ADDR, ADDR, C_INT64);
    private static final MethodHandle H_SEL_WHERE_GT     = funcVoid("cppq_select_where_gt_int", ADDR, ADDR, C_INT64);
    private static final MethodHandle H_SEL_WHERE_LT     = funcVoid("cppq_select_where_lt_int", ADDR, ADDR, C_INT64);
    private static final MethodHandle H_SEL_WHERE_LIKE   = funcVoid("cppq_select_where_like", ADDR, ADDR, ADDR);
    private static final MethodHandle H_SEL_WHERE_JCONT  = funcVoid("cppq_select_where_json_contains", ADDR, ADDR, ADDR);
    private static final MethodHandle H_SEL_WHERE_JFLD   = funcVoid("cppq_select_where_json_field_eq", ADDR, ADDR, ADDR, ADDR);
    private static final MethodHandle H_SEL_ORDER        = funcVoid("cppq_select_order_by", ADDR, ADDR, C_INT);
    private static final MethodHandle H_SEL_LIMIT        = funcVoid("cppq_select_limit", ADDR, C_INT64);
    private static final MethodHandle H_SEL_OFFSET       = funcVoid("cppq_select_offset", ADDR, C_INT64);
    // INSERT
    private static final MethodHandle H_INSERT           = func("cppq_insert", ADDR, ADDR, ADDR, C_INT);
    private static final MethodHandle H_INS_VAL_STR      = funcVoid("cppq_insert_value_str", ADDR, ADDR);
    private static final MethodHandle H_INS_VAL_INT      = funcVoid("cppq_insert_value_int", ADDR, C_INT64);
    private static final MethodHandle H_INS_VAL_DBL      = funcVoid("cppq_insert_value_double", ADDR, C_DOUBLE);
    private static final MethodHandle H_INS_VAL_NULL     = funcVoid("cppq_insert_value_null", ADDR);
    private static final MethodHandle H_INS_VAL_JSON     = funcVoid("cppq_insert_value_json", ADDR, ADDR);
    private static final MethodHandle H_INS_VAL_JSONB    = funcVoid("cppq_insert_value_jsonb", ADDR, ADDR);
    private static final MethodHandle H_INS_RETURNING    = funcVoid("cppq_insert_returning", ADDR, ADDR, C_INT);
    // UPDATE
    private static final MethodHandle H_UPDATE           = func("cppq_update", ADDR, ADDR);
    private static final MethodHandle H_UPD_SET_STR      = funcVoid("cppq_update_set_str", ADDR, ADDR, ADDR);
    private static final MethodHandle H_UPD_SET_INT      = funcVoid("cppq_update_set_int", ADDR, ADDR, C_INT64);
    private static final MethodHandle H_UPD_SET_DBL      = funcVoid("cppq_update_set_double", ADDR, ADDR, C_DOUBLE);
    private static final MethodHandle H_UPD_SET_NULL     = funcVoid("cppq_update_set_null", ADDR, ADDR);
    private static final MethodHandle H_UPD_SET_JSON     = funcVoid("cppq_update_set_json", ADDR, ADDR, ADDR);
    private static final MethodHandle H_UPD_SET_JSONB    = funcVoid("cppq_update_set_jsonb", ADDR, ADDR, ADDR);
    private static final MethodHandle H_UPD_WHERE_EQ_S   = funcVoid("cppq_update_where_eq_str", ADDR, ADDR, ADDR);
    private static final MethodHandle H_UPD_WHERE_EQ_I   = funcVoid("cppq_update_where_eq_int", ADDR, ADDR, C_INT64);
    // DELETE
    private static final MethodHandle H_DELETE           = func("cppq_delete", ADDR, ADDR);
    private static final MethodHandle H_DEL_WHERE_EQ_S   = funcVoid("cppq_delete_where_eq_str", ADDR, ADDR, ADDR);
    private static final MethodHandle H_DEL_WHERE_EQ_I   = funcVoid("cppq_delete_where_eq_int", ADDR, ADDR, C_INT64);
    // Query introspection
    private static final MethodHandle H_QUERY_SQL        = func("cppq_query_sql", ADDR, ADDR);
    private static final MethodHandle H_QUERY_PCNT       = func("cppq_query_param_count", C_INT, ADDR);
    private static final MethodHandle H_QUERY_PSTR       = func("cppq_query_param_str", ADDR, ADDR, C_INT);
    private static final MethodHandle H_QUERY_PINT       = func("cppq_query_param_int", C_INT64, ADDR, C_INT);
    private static final MethodHandle H_QUERY_PDBL       = func("cppq_query_param_double", C_DOUBLE, ADDR, C_INT);
    private static final MethodHandle H_QUERY_PNULL      = func("cppq_query_param_is_null", C_INT, ADDR, C_INT);
    // Connection
    private static final MethodHandle H_CONNECT          = func("cppq_connect", ADDR, ADDR);
    private static final MethodHandle H_DISCONNECT       = funcVoid("cppq_disconnect", ADDR);
    private static final MethodHandle H_IS_CONNECTED     = func("cppq_is_connected", C_INT, ADDR);
    private static final MethodHandle H_LAST_ERROR       = func("cppq_last_error", ADDR, ADDR);
    private static final MethodHandle H_EXECUTE          = func("cppq_execute", ADDR, ADDR, ADDR);
    // Transaction
    private static final MethodHandle H_BEGIN            = func("cppq_begin", C_INT, ADDR);
    private static final MethodHandle H_COMMIT           = func("cppq_commit", C_INT, ADDR);
    private static final MethodHandle H_ROLLBACK         = func("cppq_rollback", C_INT, ADDR);
    // Result
    private static final MethodHandle H_RES_ROWS         = func("cppq_result_rows", C_INT, ADDR);
    private static final MethodHandle H_RES_COLS         = func("cppq_result_cols", C_INT, ADDR);
    private static final MethodHandle H_RES_GET          = func("cppq_result_get", ADDR, ADDR, C_INT, C_INT);
    private static final MethodHandle H_RES_IS_NULL      = func("cppq_result_is_null", C_INT, ADDR, C_INT, C_INT);
    private static final MethodHandle H_RES_COL_NAME     = func("cppq_result_col_name", ADDR, ADDR, C_INT);
    private static final MethodHandle H_RES_COL_TYPE     = func("cppq_result_col_type", C_INT, ADDR, C_INT);
    private static final MethodHandle H_RES_IS_JSON      = func("cppq_result_is_json", C_INT, ADDR, C_INT);
    // Memory
    private static final MethodHandle H_QUERY_FREE       = funcVoid("cppq_query_free", ADDR);
    private static final MethodHandle H_RESULT_FREE      = funcVoid("cppq_result_free", ADDR);

    // ============================================================
    // Helper: convert Java String to native C string
    // ============================================================
    static MemorySegment cStr(Arena arena, String s) {
        byte[] bytes = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        var seg = arena.allocate(bytes.length + 1);
        seg.copyFrom(MemorySegment.ofArray(bytes));
        seg.set(ValueLayout.JAVA_BYTE, bytes.length, (byte) 0); // null terminator
        return seg;
    }
    static MemorySegment cStrArr(Arena arena, String[] strs) {
        if (strs == null || strs.length == 0) return MemorySegment.NULL;
        var arr = arena.allocate(ADDR.byteSize() * (long) strs.length, ADDR.byteAlignment());
        for (int i = 0; i < strs.length; i++) {
            arr.setAtIndex(ADDR, i, cStr(arena, strs[i]));
        }
        return arr;
    }
    static String readStr(MemorySegment seg) {
        if (seg.equals(MemorySegment.NULL)) return null;
        long addr = seg.address();
        if (addr == 0) return null;
        return seg.reinterpret(Long.MAX_VALUE).getUtf8String(0);
    }

    // ============================================================
    // Query: wraps a cppq_query* handle
    // ============================================================
    public static class Query implements AutoCloseable {
        MemorySegment handle;
        protected final Arena arena;

        Query(MemorySegment handle, Arena arena) {
            this.handle = handle;
            this.arena = arena;
        }

        public String getSql() {
            try {
                var sqlPtr = (MemorySegment) H_QUERY_SQL.invokeExact(handle);
                return readStr(sqlPtr);
            } catch (Throwable e) { throw new RuntimeException(e); }
        }

        public int getParamCount() {
            try {
                return (int) H_QUERY_PCNT.invokeExact(handle);
            } catch (Throwable e) { throw new RuntimeException(e); }
        }

        public String getParamStr(int index) {
            try {
                var ptr = (MemorySegment) H_QUERY_PSTR.invokeExact(handle, index);
                return readStr(ptr);
            } catch (Throwable e) { throw new RuntimeException(e); }
        }

        public long getParamLong(int index) {
            try {
                return (long) H_QUERY_PINT.invokeExact(handle, index);
            } catch (Throwable e) { throw new RuntimeException(e); }
        }

        public boolean isParamNull(int index) {
            try {
                return (int) H_QUERY_PNULL.invokeExact(handle, index) == 1;
            } catch (Throwable e) { throw new RuntimeException(e); }
        }

        public List<Object> getParams() {
            int count = getParamCount();
            var list = new ArrayList<>();
            for (int i = 0; i < count; i++) {
                if (isParamNull(i)) list.add(null);
                else list.add(getParamStr(i));
            }
            return list;
        }

        @Override
        public void close() {
            if (handle != null && !handle.equals(MemorySegment.NULL)) {
                try { H_QUERY_FREE.invokeExact(handle); } catch (Throwable e) { /* ignore */ }
                handle = null;
            }
        }

        @Override
        public String toString() {
            return "Query(sql=" + getSql() + ", params=" + getParams() + ")";
        }
    }

    // ============================================================
    // SelectQuery
    // ============================================================
    public static class SelectQuery extends Query {
        SelectQuery(MemorySegment handle, Arena arena) { super(handle, arena); }

        public SelectQuery from(String table) {
            try { H_SELECT_FROM.invokeExact(handle, cStr(arena, table)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery whereEqStr(String col, String val) {
            try { H_SEL_WHERE_EQ_S.invokeExact(handle, cStr(arena, col), cStr(arena, val)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery whereEqInt(String col, long val) {
            try { H_SEL_WHERE_EQ_I.invokeExact(handle, cStr(arena, col), val); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery whereGtInt(String col, long val) {
            try { H_SEL_WHERE_GT.invokeExact(handle, cStr(arena, col), val); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery whereLike(String col, String pattern) {
            try { H_SEL_WHERE_LIKE.invokeExact(handle, cStr(arena, col), cStr(arena, pattern)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery whereJsonContains(String col, String json) {
            try { H_SEL_WHERE_JCONT.invokeExact(handle, cStr(arena, col), cStr(arena, json)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery whereJsonFieldEq(String col, String field, String val) {
            try { H_SEL_WHERE_JFLD.invokeExact(handle, cStr(arena, col), cStr(arena, field), cStr(arena, val)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery orderBy(String col, boolean asc) {
            try { H_SEL_ORDER.invokeExact(handle, cStr(arena, col), asc ? 1 : 0); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery limit(long n) {
            try { H_SEL_LIMIT.invokeExact(handle, n); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public SelectQuery offset(long n) {
            try { H_SEL_OFFSET.invokeExact(handle, n); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
    }

    // ============================================================
    // InsertQuery
    // ============================================================
    public static class InsertQuery extends Query {
        InsertQuery(MemorySegment handle, Arena arena) { super(handle, arena); }

        public InsertQuery valueStr(String val) {
            try { H_INS_VAL_STR.invokeExact(handle, cStr(arena, val)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public InsertQuery valueInt(long val) {
            try { H_INS_VAL_INT.invokeExact(handle, val); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public InsertQuery valueDouble(double val) {
            try { H_INS_VAL_DBL.invokeExact(handle, val); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public InsertQuery valueNull() {
            try { H_INS_VAL_NULL.invokeExact(handle); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public InsertQuery valueJson(String json) {
            try { H_INS_VAL_JSON.invokeExact(handle, cStr(arena, json)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public InsertQuery valueJsonb(String json) {
            try { H_INS_VAL_JSONB.invokeExact(handle, cStr(arena, json)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public InsertQuery returning(String... cols) {
            try { H_INS_RETURNING.invokeExact(handle, cStrArr(arena, cols), cols.length); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
    }

    // ============================================================
    // UpdateQuery
    // ============================================================
    public static class UpdateQuery extends Query {
        UpdateQuery(MemorySegment handle, Arena arena) { super(handle, arena); }

        public UpdateQuery setStr(String col, String val) {
            try { H_UPD_SET_STR.invokeExact(handle, cStr(arena, col), cStr(arena, val)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public UpdateQuery setInt(String col, long val) {
            try { H_UPD_SET_INT.invokeExact(handle, cStr(arena, col), val); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public UpdateQuery setNull(String col) {
            try { H_UPD_SET_NULL.invokeExact(handle, cStr(arena, col)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public UpdateQuery setJson(String col, String json) {
            try { H_UPD_SET_JSON.invokeExact(handle, cStr(arena, col), cStr(arena, json)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public UpdateQuery setJsonb(String col, String json) {
            try { H_UPD_SET_JSONB.invokeExact(handle, cStr(arena, col), cStr(arena, json)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public UpdateQuery whereEqStr(String col, String val) {
            try { H_UPD_WHERE_EQ_S.invokeExact(handle, cStr(arena, col), cStr(arena, val)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public UpdateQuery whereEqInt(String col, long val) {
            try { H_UPD_WHERE_EQ_I.invokeExact(handle, cStr(arena, col), val); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
    }

    // ============================================================
    // DeleteQuery
    // ============================================================
    public static class DeleteQuery extends Query {
        DeleteQuery(MemorySegment handle, Arena arena) { super(handle, arena); }

        public DeleteQuery whereEqStr(String col, String val) {
            try { H_DEL_WHERE_EQ_S.invokeExact(handle, cStr(arena, col), cStr(arena, val)); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
        public DeleteQuery whereEqInt(String col, long val) {
            try { H_DEL_WHERE_EQ_I.invokeExact(handle, cStr(arena, col), val); } catch (Throwable e) { throw new RuntimeException(e); }
            return this;
        }
    }

    // ============================================================
    // ResultSet
    // ============================================================
    public static class ResultSet implements AutoCloseable {
        private MemorySegment handle;

        ResultSet(MemorySegment handle) { this.handle = handle; }

        public int getRowCount() {
            try { return (int) H_RES_ROWS.invokeExact(handle); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public int getColCount() {
            try { return (int) H_RES_COLS.invokeExact(handle); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public String get(int row, int col) {
            try {
                if ((int) H_RES_IS_NULL.invokeExact(handle, row, col) == 1) return null;
                var ptr = (MemorySegment) H_RES_GET.invokeExact(handle, row, col);
                return readStr(ptr);
            } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public String getColName(int col) {
            try {
                var ptr = (MemorySegment) H_RES_COL_NAME.invokeExact(handle, col);
                return readStr(ptr);
            } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public boolean isJson(int col) {
            try { return (int) H_RES_IS_JSON.invokeExact(handle, col) == 1; } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public List<Map<String, String>> toList() {
            var rows = new ArrayList<Map<String, String>>();
            int cols = getColCount();
            String[] names = new String[cols];
            for (int c = 0; c < cols; c++) names[c] = getColName(c);
            for (int r = 0; r < getRowCount(); r++) {
                var row = new LinkedHashMap<String, String>();
                for (int c = 0; c < cols; c++) row.put(names[c], get(r, c));
                rows.add(row);
            }
            return rows;
        }

        @Override
        public void close() {
            if (handle != null && !handle.equals(MemorySegment.NULL)) {
                try { H_RESULT_FREE.invokeExact(handle); } catch (Throwable e) { /* ignore */ }
                handle = null;
            }
        }
    }

    // ============================================================
    // Connection
    // ============================================================
    public static class Connection implements AutoCloseable {
        private MemorySegment handle;

        Connection(MemorySegment handle) { this.handle = handle; }

        public boolean isConnected() {
            try { return (int) H_IS_CONNECTED.invokeExact(handle) == 1; } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public String getLastError() {
            try {
                var ptr = (MemorySegment) H_LAST_ERROR.invokeExact(handle);
                return readStr(ptr);
            } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public ResultSet execute(Query query) {
            try {
                var resPtr = (MemorySegment) H_EXECUTE.invokeExact(handle, query.handle);
                if (resPtr.equals(MemorySegment.NULL)) {
                    throw new RuntimeException("Execute failed: " + getLastError());
                }
                return new ResultSet(resPtr);
            } catch (RuntimeException e) { throw e; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        public void begin() {
            try {
                if ((int) H_BEGIN.invokeExact(handle) != 0) throw new RuntimeException("BEGIN failed: " + getLastError());
            } catch (RuntimeException e) { throw e; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        public void commit() {
            try {
                if ((int) H_COMMIT.invokeExact(handle) != 0) throw new RuntimeException("COMMIT failed: " + getLastError());
            } catch (RuntimeException e) { throw e; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        public void rollback() {
            try {
                if ((int) H_ROLLBACK.invokeExact(handle) != 0) throw new RuntimeException("ROLLBACK failed: " + getLastError());
            } catch (RuntimeException e) { throw e; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }

        @Override
        public void close() {
            if (handle != null && !handle.equals(MemorySegment.NULL)) {
                try { H_DISCONNECT.invokeExact(handle); } catch (Throwable e) { /* ignore */ }
                handle = null;
            }
        }
    }

    // ============================================================
    // Factory Methods
    // ============================================================

    public static SelectQuery select(String... columns) {
        var arena = Arena.ofConfined();
        try {
            var h = (MemorySegment) H_SELECT.invokeExact(cStrArr(arena, columns), columns.length);
            return new SelectQuery(h, arena);
        } catch (Throwable e) { throw new RuntimeException(e); }
    }

    public static InsertQuery insert(String table, String... columns) {
        var arena = Arena.ofConfined();
        try {
            var h = (MemorySegment) H_INSERT.invokeExact(cStr(arena, table), cStrArr(arena, columns), columns.length);
            return new InsertQuery(h, arena);
        } catch (Throwable e) { throw new RuntimeException(e); }
    }

    public static UpdateQuery update(String table) {
        var arena = Arena.ofConfined();
        try {
            var h = (MemorySegment) H_UPDATE.invokeExact(cStr(arena, table));
            return new UpdateQuery(h, arena);
        } catch (Throwable e) { throw new RuntimeException(e); }
    }

    public static DeleteQuery delete(String table) {
        var arena = Arena.ofConfined();
        try {
            var h = (MemorySegment) H_DELETE.invokeExact(cStr(arena, table));
            return new DeleteQuery(h, arena);
        } catch (Throwable e) { throw new RuntimeException(e); }
    }

    public static Connection connect(String connInfo) {
        var arena = Arena.ofAuto();
        try {
            var h = (MemorySegment) H_CONNECT.invokeExact(cStr(arena, connInfo));
            var conn = new Connection(h);
            if (!conn.isConnected()) {
                String err = conn.getLastError();
                conn.close();
                throw new RuntimeException("Connection failed: " + err);
            }
            return conn;
        } catch (RuntimeException e) { throw e; }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    @Override
    public void close() { /* static-only class */ }
}
