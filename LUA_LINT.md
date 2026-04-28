# Lua Linting Notes

## Runtime enforcement: `freeze_globals`

After the script's top-level code runs, `freeze_globals` installs a `__newindex`
guard on `_G` and recursively on every user-defined table reachable from `_G`.
This blocks:

- Creating a new global: `x = 42` inside a callback → error
- Adding a new field to any module table: `nrg.newField = 42` inside a callback → error

**Known gaps**

1. **Existing-key writes are not caught.** `__newindex` fires only when the key
   does not already exist in the table.  If a script pre-declares a slot at
   top level (`nrg.counter = 0`) and then modifies it inside a callback
   (`nrg.counter = nrg.counter + 1`), no error is raised.  Closing this gap
   would require a full proxy-table approach, which complicates the `lua_next`
   traversal inside `find_global_path`/`scan_table`; deferred for now.

2. **`rawset` bypass.** `rawset(t, k, v)` skips metatables entirely.  A script
   can use it to write to any table without triggering the guard.  Mitigation:
   remove or wrap `rawset` after freeze.

3. **Module-level upvalues.** A closure can capture a mutable table as an
   upvalue (`local state = {}` at module scope) and modify it across calls.
   This is invisible from C and cannot be blocked without rewriting the Lua VM.

## Static analysis: rules to implement

Two rules that are detectable as pure AST pattern matches:

### Rule 1 — `require` result must be assigned to a global

```lua
local app = require("app")   -- warn/error
app = require("app")         -- ok
```

The module prefix detection in `push_caller_prefix` (via `package.loaded` +
`find_global_path`) only works when the module table is reachable from `_G`.
Assigning to a local makes the module invisible to the scheduler.

Implementation: find every `LocalStatement` whose init expression is a call
to `require`; flag it.

### Rule 2 — Write to a global table field inside a callback

```lua
function on_event(rw, ro)
    nrg.counter = 1   -- warn: nrg is a global, not a parameter
    rw.counter = 1    -- ok: rw is a parameter
end
```

Implementation: for every `AssignmentStatement` inside a function body, check
whether the LHS root name resolves to a global (i.e., is not a parameter or
local of any enclosing scope).  `rw` and `ro` are always parameters so they
are naturally excluded.  Not 100%: cannot catch writes to globals that were
shadowed by a local of the same name.

## Out-of-the-box linters

**luacheck** (`luarocks install luacheck`) is the standard tool.  It catches
undefined globals, unused variables, shadowing, etc.  Neither rule above is
available out of the box; luacheck is not easily extended with custom AST rules.

**selene** (Rust, more modern) supports custom lint rules written in Lua and a
standard-library definition file; more extensible, but still requires custom
work for these rules.

A small standalone checker (~50–80 lines) using any Lua AST parser (e.g.
`luaparse` via Node, or `lua-parser` via LuaRocks) would cover both rules
without pulling in a full linter framework.
