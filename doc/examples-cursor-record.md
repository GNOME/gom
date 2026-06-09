Title: Sample: Cursor and Record Wrapper

Use [class@Gom.Cursor] for row-by-row query results. Call
[method@Gom.Cursor.next] before reading a row, and close the cursor when
iteration is complete.

Use [method@Gom.Cursor.snapshot] when values need to outlive the cursor's
current position. The snapshot is a detached [class@Gom.Record].

This fragment assumes `repository` and `query` already exist, and that column
`1` is a string title.

```c
g_autoptr(GomCursor) cursor = NULL;

cursor = dex_await_object (gom_repository_query (repository, query), error);
if (cursor == NULL)
  return FALSE;

while (dex_await_boolean (gom_cursor_next (cursor), error))
  {
    g_autoptr(GomRecord) record = NULL;
    const char *title;

    record = gom_cursor_snapshot (cursor, error);
    if (record == NULL)
      break;

    title = gom_record_get_column_string (record, 1);
    g_print ("title=%s\n", title);
  }

dex_await (gom_cursor_close (cursor), NULL);
```

See also [method@Gom.Record.get_column_by_name] when column names are clearer
than indexes, or when you need the value as a generic `GValue`.
