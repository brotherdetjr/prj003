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
or `"myapp.energy."` — into `buf`. Returns 1 if found, 0 if not.

```c
if (mod_idx < 0) mod_idx = lua_gettop(L) + 1 + mod_idx;
```

`mod_idx` might be a relative index like `-1` (top of stack). This line converts
it to an absolute index so it stays valid after we push more things on top.
`lua_gettop` returns the current stack depth, so `gettop + 1 + (-1) = gettop` —
the current top slot number.

```c
buf[0] = '\0';

lua_pushglobaltable(L);
int g = lua_gettop(L);
```

We push `_G` (the global table) onto the stack and remember its absolute index as
`g`. Everything else we push from here on will be above it.

### Depth-1 scan: is the module directly in `_G`?

```c
lua_pushnil(L);
while (lua_next(L, g)) {
```

`lua_next(L, g)` is how you iterate a Lua table in C. It pops the key at the top
of the stack and pushes the *next* key–value pair. Passing `nil` as the starting
key means "start from the beginning". After a successful call the stack gains two
slots: `[-2]` = key, `[-1]` = value.

```c
    if (lua_type(L, -2) == LUA_TSTRING && lua_rawequal(L, mod_idx, -1)) {
        snprintf(buf, buf_size, "%s.", lua_tostring(L, -2));
        lua_pop(L, 2);
        break;
    }
    lua_pop(L, 1);
}
```

- `lua_type(L, -2) == LUA_TSTRING` — we only care about string keys (global
  variable names are strings).
- `lua_rawequal(L, mod_idx, -1)` — checks **identity**: is the value at `-1` the
  exact same table object as the one at `mod_idx`? (Not deep equality — pointer
  equality.) This is how we recognise "yes, this slot in `_G` *is* our module."
- On a match: write `"keyname."` into buf and pop both key and value
  (`lua_pop(L, 2)`), then `break`.
- No match: pop just the value (`lua_pop(L, 1)`) — `lua_next` needs the key to
  remain on the stack for the next iteration.
- When `lua_next` finds no more entries it returns 0 and pops the key itself,
  leaving the stack at just `_G`.

### Depth-2 scan: is the module at `_G[k1][k2]`?

```c
if (!buf[0]) {
```

Only enter this block if depth-1 found nothing. `buf[0]` is still `'\0'` in that
case.

```c
    lua_pushnil(L);
    while (lua_next(L, g)) {
        if (lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TTABLE ||
            lua_rawequal(L, -1, g) ||
            strcmp(lua_tostring(L, -2), "package") == 0) {
            lua_pop(L, 1);
            continue;
        }
```

Outer loop: iterate `_G` again. Skip any entry that isn't a string-keyed table,
or that is `_G` itself (to avoid infinite recursion), or that is `package` (its
sub-tables would cause false matches via `package.loaded`).

```c
        char k1[64];
        strncpy(k1, lua_tostring(L, -2), sizeof(k1) - 1);
        k1[sizeof(k1) - 1] = '\0';
        int t = lua_gettop(L);
```

We are about to iterate the *inner* table, which pushes more things on the stack —
so `lua_tostring(L, -2)` (the outer key) will no longer be at `-2`. We copy it
into `k1` now. `t` captures the absolute index of the outer table value so the
inner `lua_next` knows which table to iterate.

```c
        lua_pushnil(L);
        while (lua_next(L, t)) {
            if (lua_type(L, -2) == LUA_TSTRING && lua_rawequal(L, mod_idx, -1)) {
                snprintf(buf, buf_size, "%s.%s.", k1, lua_tostring(L, -2));
                lua_pop(L, 2);
                break;
            }
            lua_pop(L, 1);
        }
```

Inner loop: same identity check but now against `_G[k1][k2]`. On a match, write
`"k1.k2."` and pop the inner key+value, then `break` out of the inner loop.

```c
        lua_pop(L, 1); /* outer table */
        if (buf[0]) {
            lua_pop(L, 1); /* k1 */
            break;
        }
    }
}
```

After the inner loop ends (match or exhausted), pop the outer value (the
sub-table). If `buf` was filled by the inner loop, also pop the outer key and
`break` from the outer loop. If not, leave the outer key on the stack for
`lua_next` to consume on the next iteration.

```c
lua_pop(L, 1); /* _G */
return buf[0] != '\0';
```

`_G` is always popped here — every code path above leaves exactly `_G` on the
stack at this point.

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
