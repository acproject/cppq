"""
cppq - Python binding for cppq Postgres DSL library.

Provides a Pythonic API over the C ABI (ctypes) to build
parameterized SQL queries and execute them against PostgreSQL.

Usage:
    from cppq import select, insert, update, delete, connect

    # Build query without database connection
    q = select("id", "name").from_("users").where_eq_str("phone", "13800001234").limit(10)
    print(q.sql)     # "SELECT id, name FROM users WHERE phone=$1 LIMIT $2"
    print(q.params)  # ["13800001234", 10]

    # Connect and execute
    conn = connect("postgresql://localhost/mydb")
    result = conn.execute(q)
    for row in result:
        print(row)
"""

import ctypes
import ctypes.util
import os
import sys
from pathlib import Path
from typing import Any, Optional


def _find_lib():
    """Locate the cppq_ffi shared library."""
    # Try common build output locations
    search_dirs = [
        Path(__file__).parent,                                      # Same dir as this .py
        Path(__file__).parent.parent.parent / "build" / "lib",      # project/build/lib/
        Path(__file__).parent.parent.parent / "build",               # project/build/
        Path(__file__).parent.parent.parent / "build" / "src",
    ]

    # Platform-specific lib name
    if sys.platform == "darwin":
        lib_name = "libcppq_ffi.dylib"
    elif sys.platform == "win32":
        lib_name = "cppq_ffi.dll"
    else:
        lib_name = "libcppq_ffi.so"

    for d in search_dirs:
        path = d / lib_name
        if path.exists():
            return str(path)

    # Try system paths
    system_lib = ctypes.util.find_library("cppq_ffi")
    if system_lib:
        return system_lib

    raise OSError(
        f"Cannot find {lib_name}. "
        "Build the project with -DCPPQ_BUILD_FFI=ON first."
    )


_lib = ctypes.CDLL(_find_lib())

# ============================================================
# C type mappings
# ============================================================
_c_str_p = ctypes.c_char_p
_c_int = ctypes.c_int
_c_int64 = ctypes.c_int64
_c_double = ctypes.c_double
_c_str_array = ctypes.POINTER(_c_str_p)

# Opaque handle types
class _Query(ctypes.Structure):
    pass

class _Conn(ctypes.Structure):
    pass

class _Result(ctypes.Structure):
    pass

_QueryPtr = ctypes.POINTER(_Query)
_ConnPtr = ctypes.POINTER(_Conn)
_ResultPtr = ctypes.POINTER(_Result)

# ============================================================
# Function signatures
# ============================================================

# -- SELECT --
_lib.cppq_select.argtypes = [_c_str_array, _c_int]
_lib.cppq_select.restype = _QueryPtr

_lib.cppq_select_from.argtypes = [_QueryPtr, _c_str_p]
_lib.cppq_select_from.restype = None

_lib.cppq_select_where_eq_str.argtypes = [_QueryPtr, _c_str_p, _c_str_p]
_lib.cppq_select_where_eq_str.restype = None

_lib.cppq_select_where_eq_int.argtypes = [_QueryPtr, _c_str_p, _c_int64]
_lib.cppq_select_where_eq_int.restype = None

_lib.cppq_select_where_gt_int.argtypes = [_QueryPtr, _c_str_p, _c_int64]
_lib.cppq_select_where_gt_int.restype = None

_lib.cppq_select_where_lt_int.argtypes = [_QueryPtr, _c_str_p, _c_int64]
_lib.cppq_select_where_lt_int.restype = None

_lib.cppq_select_where_like.argtypes = [_QueryPtr, _c_str_p, _c_str_p]
_lib.cppq_select_where_like.restype = None

_lib.cppq_select_order_by.argtypes = [_QueryPtr, _c_str_p, _c_int]
_lib.cppq_select_order_by.restype = None

_lib.cppq_select_limit.argtypes = [_QueryPtr, _c_int64]
_lib.cppq_select_limit.restype = None

_lib.cppq_select_offset.argtypes = [_QueryPtr, _c_int64]
_lib.cppq_select_offset.restype = None

# -- INSERT --
_lib.cppq_insert.argtypes = [_c_str_p, _c_str_array, _c_int]
_lib.cppq_insert.restype = _QueryPtr

_lib.cppq_insert_value_str.argtypes = [_QueryPtr, _c_str_p]
_lib.cppq_insert_value_str.restype = None

_lib.cppq_insert_value_int.argtypes = [_QueryPtr, _c_int64]
_lib.cppq_insert_value_int.restype = None

_lib.cppq_insert_value_double.argtypes = [_QueryPtr, _c_double]
_lib.cppq_insert_value_double.restype = None

_lib.cppq_insert_value_null.argtypes = [_QueryPtr]
_lib.cppq_insert_value_null.restype = None

_lib.cppq_insert_returning.argtypes = [_QueryPtr, _c_str_array, _c_int]
_lib.cppq_insert_returning.restype = None

# -- UPDATE --
_lib.cppq_update.argtypes = [_c_str_p]
_lib.cppq_update.restype = _QueryPtr

_lib.cppq_update_set_str.argtypes = [_QueryPtr, _c_str_p, _c_str_p]
_lib.cppq_update_set_str.restype = None

_lib.cppq_update_set_int.argtypes = [_QueryPtr, _c_str_p, _c_int64]
_lib.cppq_update_set_int.restype = None

_lib.cppq_update_set_double.argtypes = [_QueryPtr, _c_str_p, _c_double]
_lib.cppq_update_set_double.restype = None

_lib.cppq_update_set_null.argtypes = [_QueryPtr, _c_str_p]
_lib.cppq_update_set_null.restype = None

_lib.cppq_update_where_eq_str.argtypes = [_QueryPtr, _c_str_p, _c_str_p]
_lib.cppq_update_where_eq_str.restype = None

_lib.cppq_update_where_eq_int.argtypes = [_QueryPtr, _c_str_p, _c_int64]
_lib.cppq_update_where_eq_int.restype = None

# -- DELETE --
_lib.cppq_delete.argtypes = [_c_str_p]
_lib.cppq_delete.restype = _QueryPtr

_lib.cppq_delete_where_eq_str.argtypes = [_QueryPtr, _c_str_p, _c_str_p]
_lib.cppq_delete_where_eq_str.restype = None

_lib.cppq_delete_where_eq_int.argtypes = [_QueryPtr, _c_str_p, _c_int64]
_lib.cppq_delete_where_eq_int.restype = None

# -- Query Introspection --
_lib.cppq_query_sql.argtypes = [_QueryPtr]
_lib.cppq_query_sql.restype = _c_str_p

_lib.cppq_query_param_count.argtypes = [_QueryPtr]
_lib.cppq_query_param_count.restype = _c_int

_lib.cppq_query_param_str.argtypes = [_QueryPtr, _c_int]
_lib.cppq_query_param_str.restype = _c_str_p

_lib.cppq_query_param_int.argtypes = [_QueryPtr, _c_int]
_lib.cppq_query_param_int.restype = _c_int64

_lib.cppq_query_param_double.argtypes = [_QueryPtr, _c_int]
_lib.cppq_query_param_double.restype = _c_double

_lib.cppq_query_param_is_null.argtypes = [_QueryPtr, _c_int]
_lib.cppq_query_param_is_null.restype = _c_int

# -- Connection --
_lib.cppq_connect.argtypes = [_c_str_p]
_lib.cppq_connect.restype = _ConnPtr

_lib.cppq_disconnect.argtypes = [_ConnPtr]
_lib.cppq_disconnect.restype = None

_lib.cppq_is_connected.argtypes = [_ConnPtr]
_lib.cppq_is_connected.restype = _c_int

_lib.cppq_last_error.argtypes = [_ConnPtr]
_lib.cppq_last_error.restype = _c_str_p

_lib.cppq_execute.argtypes = [_ConnPtr, _QueryPtr]
_lib.cppq_execute.restype = _ResultPtr

_lib.cppq_begin.argtypes = [_ConnPtr]
_lib.cppq_begin.restype = _c_int

_lib.cppq_commit.argtypes = [_ConnPtr]
_lib.cppq_commit.restype = _c_int

_lib.cppq_rollback.argtypes = [_ConnPtr]
_lib.cppq_rollback.restype = _c_int

# -- Result --
_lib.cppq_result_rows.argtypes = [_ResultPtr]
_lib.cppq_result_rows.restype = _c_int

_lib.cppq_result_cols.argtypes = [_ResultPtr]
_lib.cppq_result_cols.restype = _c_int

_lib.cppq_result_get.argtypes = [_ResultPtr, _c_int, _c_int]
_lib.cppq_result_get.restype = _c_str_p

_lib.cppq_result_is_null.argtypes = [_ResultPtr, _c_int, _c_int]
_lib.cppq_result_is_null.restype = _c_int

_lib.cppq_result_col_name.argtypes = [_ResultPtr, _c_int]
_lib.cppq_result_col_name.restype = _c_str_p

# -- Memory --
_lib.cppq_query_free.argtypes = [_QueryPtr]
_lib.cppq_query_free.restype = None

_lib.cppq_result_free.argtypes = [_ResultPtr]
_lib.cppq_result_free.restype = None


def _to_c_str_array(strs: list[str]) -> _c_str_array:
    """Convert a Python list of strings to a C array of char*."""
    arr = (_c_str_p * len(strs))()
    for i, s in enumerate(strs):
        arr[i] = s.encode("utf-8")
    return ctypes.cast(arr, _c_str_array)


def _encode(s: str) -> bytes:
    return s.encode("utf-8")


# ============================================================
# Pythonic Wrapper Classes
# ============================================================

class Query:
    """Wraps a cppq_query handle. Provides .sql and .params properties."""

    def __init__(self, handle: _QueryPtr):
        self._handle = handle

    def __del__(self):
        if hasattr(self, "_handle") and self._handle:
            _lib.cppq_query_free(self._handle)
            self._handle = None

    @property
    def sql(self) -> str:
        raw = _lib.cppq_query_sql(self._handle)
        return raw.decode("utf-8") if raw else ""

    @property
    def params(self) -> list[Any]:
        count = _lib.cppq_query_param_count(self._handle)
        result = []
        for i in range(count):
            if _lib.cppq_query_param_is_null(self._handle, i):
                result.append(None)
            else:
                # Try to get as string first (universal)
                s = _lib.cppq_query_param_str(self._handle, i)
                val = s.decode("utf-8") if s else None
                # Attempt numeric conversion
                if val is not None:
                    try:
                        if "." in val:
                            result.append(float(val))
                        else:
                            result.append(int(val))
                    except ValueError:
                        result.append(val)
                else:
                    result.append(val)
        return result

    def __repr__(self) -> str:
        return f"Query(sql={self.sql!r}, params={self.params!r})"


class SelectQuery(Query):
    """SELECT query builder with fluent API."""

    def from_(self, table: str) -> "SelectQuery":
        _lib.cppq_select_from(self._handle, _encode(table))
        return self

    def where_eq_str(self, col: str, val: str) -> "SelectQuery":
        _lib.cppq_select_where_eq_str(self._handle, _encode(col), _encode(val))
        return self

    def where_eq_int(self, col: str, val: int) -> "SelectQuery":
        _lib.cppq_select_where_eq_int(self._handle, _encode(col), val)
        return self

    def where_gt_int(self, col: str, val: int) -> "SelectQuery":
        _lib.cppq_select_where_gt_int(self._handle, _encode(col), val)
        return self

    def where_lt_int(self, col: str, val: int) -> "SelectQuery":
        _lib.cppq_select_where_lt_int(self._handle, _encode(col), val)
        return self

    def where_like(self, col: str, pattern: str) -> "SelectQuery":
        _lib.cppq_select_where_like(self._handle, _encode(col), _encode(pattern))
        return self

    def order_by(self, col: str, asc: bool = True) -> "SelectQuery":
        _lib.cppq_select_order_by(self._handle, _encode(col), 1 if asc else 0)
        return self

    def limit(self, n: int) -> "SelectQuery":
        _lib.cppq_select_limit(self._handle, n)
        return self

    def offset(self, n: int) -> "SelectQuery":
        _lib.cppq_select_offset(self._handle, n)
        return self


class InsertQuery(Query):
    """INSERT query builder with fluent API."""

    def value_str(self, val: str) -> "InsertQuery":
        _lib.cppq_insert_value_str(self._handle, _encode(val))
        return self

    def value_int(self, val: int) -> "InsertQuery":
        _lib.cppq_insert_value_int(self._handle, val)
        return self

    def value_double(self, val: float) -> "InsertQuery":
        _lib.cppq_insert_value_double(self._handle, val)
        return self

    def value_null(self) -> "InsertQuery":
        _lib.cppq_insert_value_null(self._handle)
        return self

    def returning(self, *cols: str) -> "InsertQuery":
        arr = _to_c_str_array(list(cols))
        _lib.cppq_insert_returning(self._handle, arr, len(cols))
        return self


class UpdateQuery(Query):
    """UPDATE query builder with fluent API."""

    def set_str(self, col: str, val: str) -> "UpdateQuery":
        _lib.cppq_update_set_str(self._handle, _encode(col), _encode(val))
        return self

    def set_int(self, col: str, val: int) -> "UpdateQuery":
        _lib.cppq_update_set_int(self._handle, _encode(col), val)
        return self

    def set_double(self, col: str, val: float) -> "UpdateQuery":
        _lib.cppq_update_set_double(self._handle, _encode(col), val)
        return self

    def set_null(self, col: str) -> "UpdateQuery":
        _lib.cppq_update_set_null(self._handle, _encode(col))
        return self

    def where_eq_str(self, col: str, val: str) -> "UpdateQuery":
        _lib.cppq_update_where_eq_str(self._handle, _encode(col), _encode(val))
        return self

    def where_eq_int(self, col: str, val: int) -> "UpdateQuery":
        _lib.cppq_update_where_eq_int(self._handle, _encode(col), val)
        return self


class DeleteQuery(Query):
    """DELETE query builder with fluent API."""

    def where_eq_str(self, col: str, val: str) -> "DeleteQuery":
        _lib.cppq_delete_where_eq_str(self._handle, _encode(col), _encode(val))
        return self

    def where_eq_int(self, col: str, val: int) -> "DeleteQuery":
        _lib.cppq_delete_where_eq_int(self._handle, _encode(col), val)
        return self


class ResultSet:
    """Wraps a cppq_result handle. Iterable over rows as dicts."""

    def __init__(self, handle: _ResultPtr):
        self._handle = handle

    def __del__(self):
        if hasattr(self, "_handle") and self._handle:
            _lib.cppq_result_free(self._handle)
            self._handle = None

    @property
    def row_count(self) -> int:
        return _lib.cppq_result_rows(self._handle)

    @property
    def col_count(self) -> int:
        return _lib.cppq_result_cols(self._handle)

    def get(self, row: int, col: int) -> Optional[str]:
        if _lib.cppq_result_is_null(self._handle, row, col):
            return None
        raw = _lib.cppq_result_get(self._handle, row, col)
        return raw.decode("utf-8") if raw else None

    def col_name(self, col: int) -> str:
        raw = _lib.cppq_result_col_name(self._handle, col)
        return raw.decode("utf-8") if raw else ""

    def __iter__(self):
        """Iterate rows as dicts."""
        col_names = [self.col_name(c) for c in range(self.col_count)]
        for r in range(self.row_count):
            row_dict = {}
            for c, name in enumerate(col_names):
                row_dict[name] = self.get(r, c)
            yield row_dict

    def __len__(self) -> int:
        return self.row_count

    def __repr__(self) -> str:
        return f"ResultSet(rows={self.row_count}, cols={self.col_count})"


class Connection:
    """Database connection wrapper."""

    def __init__(self, handle: _ConnPtr):
        self._handle = handle

    def __del__(self):
        self.close()

    @property
    def is_connected(self) -> bool:
        return bool(_lib.cppq_is_connected(self._handle))

    @property
    def last_error(self) -> str:
        raw = _lib.cppq_last_error(self._handle)
        return raw.decode("utf-8") if raw else ""

    def execute(self, query: Query) -> ResultSet:
        result_ptr = _lib.cppq_execute(self._handle, query._handle)
        if not result_ptr:
            raise RuntimeError(f"Query execution failed: {self.last_error}")
        return ResultSet(result_ptr)

    def begin(self):
        if _lib.cppq_begin(self._handle) != 0:
            raise RuntimeError(f"BEGIN failed: {self.last_error}")

    def commit(self):
        if _lib.cppq_commit(self._handle) != 0:
            raise RuntimeError(f"COMMIT failed: {self.last_error}")

    def rollback(self):
        if _lib.cppq_rollback(self._handle) != 0:
            raise RuntimeError(f"ROLLBACK failed: {self.last_error}")

    def close(self):
        if hasattr(self, "_handle") and self._handle:
            _lib.cppq_disconnect(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None:
            self.rollback()
        else:
            self.commit()
        self.close()
        return False


# ============================================================
# Public API: Factory functions
# ============================================================

def select(*columns: str) -> SelectQuery:
    """Create a SELECT query builder.

    Example:
        q = select("id", "name").from_("users").where_eq_str("phone", "138").limit(10)
    """
    cols = list(columns) if columns else []
    arr = _to_c_str_array(cols) if cols else None
    handle = _lib.cppq_select(arr, len(cols))
    return SelectQuery(handle)


def insert(table: str, *columns: str) -> InsertQuery:
    """Create an INSERT query builder.

    Example:
        q = insert("users", "name", "age").value_str("Alice").value_int(25)
    """
    cols = list(columns)
    arr = _to_c_str_array(cols) if cols else None
    handle = _lib.cppq_insert(_encode(table), arr, len(cols))
    return InsertQuery(handle)


def update(table: str) -> UpdateQuery:
    """Create an UPDATE query builder.

    Example:
        q = update("users").set_str("name", "Bob").where_eq_int("id", 42)
    """
    handle = _lib.cppq_update(_encode(table))
    return UpdateQuery(handle)


def delete(table: str) -> DeleteQuery:
    """Create a DELETE query builder.

    Example:
        q = delete("users").where_eq_int("id", 42)
    """
    handle = _lib.cppq_delete(_encode(table))
    return DeleteQuery(handle)


def connect(conn_info: str) -> Connection:
    """Connect to a PostgreSQL database.

    Args:
        conn_info: libpq connection string, e.g. "host=localhost dbname=mydb"
                   or "postgresql://localhost/mydb"

    Returns:
        Connection object.

    Raises:
        RuntimeError: If connection fails.
    """
    handle = _lib.cppq_connect(_encode(conn_info))
    conn = Connection(handle)
    if not conn.is_connected:
        err = conn.last_error
        conn.close()
        raise RuntimeError(f"Connection failed: {err}")
    return conn
