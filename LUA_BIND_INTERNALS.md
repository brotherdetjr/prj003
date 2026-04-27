# lua_bind internals: `find_global_path` and `push_caller_prefix`

## The Lua C stack — the one concept you need first

The Lua C API is entirely stack-based. Every value you want to work with must be
pushed onto a virtual stack, and every result Lua gives back lands on that same
stack. Indices work like this:

```
bottom → [1] [2] [3] ... [-3] [-2] [-1] ← top
```

Positive indices count from the bottom; negative from the top (`-1` is always the
topmost value). Every API call that "reads" something pops from the stack; every
call that "produces" something pushes onto it. If you call a function that pushes
something, you are responsible for popping it when you're done — otherwise you
leak stack space.

---

## `find_global_path`

**Purpose:** given a Lua table that is already on the stack (at `mod_idx`), find
where it is reachable in the global environment and write that path — e.g. `"nrg."`
or `"myapp.energy."` or `"myapp.systems.energy."` — into `buf`. Returns 1 if
found, 0 if not. Works at any nesting depth.

The work is split between a thin public function and a recursive DFS helper.

### `find_global_path` — setup and teardown

```c
if (mod_idx < 0) mod_idx = lua_gettop(L) + 1 + mod_idx;
```

`mod_idx` might be a relative index like `-1` (top of stack). This line converts
it to an absolute index so it stays valid after we push more things on top.
`lua_gettop` returns the current stack depth, so `gettop + 1 + (-1) = gettop` —
the current top slot number.

```c
lua_newtable(L);
int visited = lua_gettop(L);

lua_pushglobaltable(L);
int g = lua_gettop(L);
```

We create a fresh Lua table (`visited`) that we use as a set to track which tables
have already been entered during the DFS — this breaks cycles like `a.b = a`. Then
we push `_G` and remember both absolute indices.

```c
lua_getglobal(L, "package");
lua_pushboolean(L, 1);
lua_rawset(L, visited); /* pre-mark package so scan_table skips it */
```

`lua_rawset(L, visited)` pops the top two values (key = `package` table, value =
`true`) and stores them in `visited`. Pre-marking `package` prevents the DFS from
descending into `package.loaded` and finding false matches there.

```c
scan_table(L, g, mod_idx, visited, buf, buf_size, 0);

lua_pop(L, 2); /* _G, visited */
return buf[0] != '\0';
```

Launch the DFS from `_G` with an empty path so far (`path_len = 0`). Afterwards
pop `_G` and the `visited` table — `scan_table` always leaves the stack balanced.

### `scan_table` — the recursive DFS

`scan_table(L, tbl, mod_idx, visited, buf, buf_size, path_len)` searches `tbl` for
the table at `mod_idx`, building the dotted path by writing directly into
`buf[path_len..]`. On success it returns 1 with `buf` holding the complete path;
on failure it restores `buf[path_len] = '\0'` and returns 0.

```c
lua_pushvalue(L, tbl);
lua_pushboolean(L, 1);
lua_rawset(L, visited); /* visited[tbl] = true */
```

Mark the current table visited before iterating it, so that any back-edge to it
encountered during recursion will be skipped.

```c
lua_pushnil(L);
while (lua_next(L, tbl)) {
```

Standard Lua table iteration. `lua_next` pops the key at the top and pushes the
next key–value pair, or returns 0 (pushing nothing) when exhausted. Starting with
`nil` means "from the beginning". During each iteration `[-2]` = key, `[-1]` =
value.

```c
    if (lua_type(L, -2) != LUA_TSTRING) {
        lua_pop(L, 1);
        continue;
    }
    const char *key = lua_tostring(L, -2);
    size_t key_len = strlen(key);
    if (path_len + key_len + 2 > buf_size) { /* key + '.' + '\0' */
        lua_pop(L, 1);
        continue;
    }
    memcpy(buf + path_len, key, key_len);
    buf[path_len + key_len] = '.';
    buf[path_len + key_len + 1] = '\0';
```

Skip non-string keys (only string keys are valid global-path components). Then
append `"key."` directly into `buf` — no temporary string needed. The length check
ensures we never write past `buf_size`.

```c
    if (lua_rawequal(L, -1, mod_idx)) {
        lua_pop(L, 2);
        return 1;
    }
```

`lua_rawequal` checks **identity** — pointer equality, not deep equality. If the
value at `-1` is literally the same table object as `mod_idx`, `buf` already holds
the correct complete path. Pop the key and value and return success.

```c
    if (lua_type(L, -1) == LUA_TTABLE) {
        lua_pushvalue(L, -1);
        lua_rawget(L, visited);
        int seen = lua_toboolean(L, -1);
        lua_pop(L, 1);
        if (!seen) {
            int sub = lua_gettop(L);
            if (scan_table(L, sub, mod_idx, visited,
                           buf, buf_size, path_len + key_len + 1)) {
                lua_pop(L, 2);
                return 1;
            }
        }
    }
```

If the value is a table we haven't visited yet, recurse into it. `lua_pushvalue`
copies the table reference so `lua_rawget` can consume it as a key lookup without
disturbing the iteration state. `sub = lua_gettop(L)` captures the absolute index
of the value (the sub-table) before the recursion pushes anything else. If the
recursive call finds the module, propagate the success upward.

```c
    buf[path_len] = '\0'; /* backtrack path before next iteration */
    lua_pop(L, 1);
}
return 0;
```

This branch is reached when neither the identity check nor the recursion succeeded.
Restore `buf` to what it was before this key was appended (backtrack), then pop the
value — leaving the key on the stack for `lua_next` to consume on the next
iteration. If the loop exhausts the table without a match, return 0.

---

## `push_caller_prefix`

**Purpose:** figure out which Lua module is *calling* `schedule()` right now,
determine where that module lives in `_G`, and push the resulting prefix string
(e.g. `"myapp.energy."`) onto the Lua stack so `l_schedule` can prepend it to the
event name.

### Step 1: find the caller's source file

```c
lua_Debug ar;
char src[1024] = "";
for (int level = 1; lua_getstack(L, level, &ar); level++) {
    lua_getinfo(L, "S", &ar);
    if (ar.what[0] == 'L' && ar.source[0] == '@') {
        strncpy(src, ar.source + 1, sizeof(src) - 1);
        break;
    }
}
if (!src[0]) { lua_pushstring(L, ""); return; }
```

`lua_getstack` and `lua_getinfo` are Lua's debug API. `lua_getstack(L, level, &ar)`
fills `ar` with the activation record for call-stack level `level` (1 = immediate
caller, 2 = caller's caller, …). `lua_getinfo(L, "S", &ar)` then fills `ar.what`
(the type of function: `'L'` = Lua source, `'C'` = C function) and `ar.source`
(the source name, prefixed with `'@'` when it is a filename). We walk up until we
find the innermost Lua source frame and record its filename in `src`. If we find
nothing (e.g. called entirely from C), push `""` and return.

### Step 2: extract the script directory from `package.path`

```c
lua_getglobal(L, "package");
lua_getfield(L, -1, "path");
const char *pp = lua_tostring(L, -1);
char dir[1024] = "";
if (pp) {
    const char *q = strstr(pp, "/?.lua");
    if (q && (size_t)(q - pp) < sizeof(dir)) {
        size_t n = (size_t)(q - pp);
        memcpy(dir, pp, n);
        dir[n] = '\0';
    }
}
lua_pop(L, 1); /* path */
if (!dir[0]) { lua_pop(L, 1); lua_pushstring(L, ""); return; }
```

`lua_getglobal` pushes the value of a global variable. `lua_getfield(L, -1, "path")`
pushes `package["path"]` onto the stack (leaving `package` below it). We set
`package.path` to a single entry like `"/scripts/?.lua"` during init, so we can
extract the directory by finding `"/?.lua"` and taking everything before it. Then
pop `path` (keep `package` for the next step).

### Step 3: find the matching module table in `package.loaded`

`package.loaded` is a table Lua maintains automatically: every `require("modname")`
stores the returned module table there under the module's name.

```c
lua_getfield(L, -1, "loaded");
lua_remove(L, -2); /* drop package */
int loaded = lua_gettop(L);
```

`lua_getfield(L, -1, "loaded")` pushes `package.loaded`. `lua_remove(L, -2)`
removes the `package` table from one slot below the top, leaving only `loaded`. We
record its index.

```c
int found = 0;
lua_pushnil(L);
while (!found && lua_next(L, loaded)) {
```

`!found &&` short-circuits: once `found` is set to 1, `lua_next` is never called
again, so the iterator does not advance past our chosen entry.

```c
    if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TTABLE) {
        const char *mn = lua_tostring(L, -2);
        char rp[256];
        strncpy(rp, mn, sizeof(rp) - 1);
        rp[sizeof(rp) - 1] = '\0';
        for (char *p = rp; *p; p++)
            if (*p == '.') *p = '/';
        char fp[1024];
        int n = snprintf(fp, sizeof(fp), "%s/%s.lua", dir, rp);
        if (n > 0 && (size_t)n < sizeof(fp) && strcmp(fp, src) == 0) {
            lua_remove(L, -2); /* drop key, leave module table on top */
            found = 1;
            continue;
        }
    }
    lua_pop(L, 1);
}
```

For each string-keyed table entry in `package.loaded`: convert the module name
(`"myapp.energy"` → `"myapp/energy"`) and reconstruct the expected file path. If
it matches the caller's source file, we have found the right module table.
`lua_remove(L, -2)` removes the key but leaves the value (the module table) on
top. `continue` goes back to the loop condition, where `!found` is now false, so
`lua_next` is not called — the stack is left with `loaded` and the module table on
top.

If no match: `lua_pop(L, 1)` drops the value, leaving the key for `lua_next` on
the next iteration.

### Step 4: look up the module table's global path

```c
if (!found) {
    lua_pop(L, 1); /* loaded */
    lua_pushstring(L, "");
    return;
}

char prefix[128] = "";
find_global_path(L, -1, prefix, sizeof(prefix));
lua_pop(L, 2); /* module_table, loaded */
lua_pushstring(L, prefix);
```

If nothing matched, pop `loaded` and push `""`. Otherwise, the module table is at
`-1`. We pass it to `find_global_path` which scans `_G` for the same object and
fills `prefix`. Then pop both the module table and `loaded`, and push the result
string — which is what `l_schedule` reads off the stack.
