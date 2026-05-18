# Gloxie

## Concept

Gloxie is a pocket creature toy with no internet, no AI. The creature lives on
the device, ages, needs care, and meets other creatures via Bluetooth.

## Hardware

- **Target Device Candidate:** [MaTouch ESP32-S3 AMOLED with Touch 1.78” CHSC6417](https://www.makerfabs.com/matouch-esp32-s3-amoled-with-touch-1-8-ft3168.html)
- **RAM:** 512 KB internal + 8 MB PSRAM
- **Flash:** 16 MB
- **Controls:** touchscreen
- **Radio:** Bluetooth (BLE) for device-to-device discovery and interaction
- **Screen:** 1.8”, 368*448 px

## Core mechanics - draft (!!!) ideas

### Character lifecycle
- Born with a generated ID and a birth timestamp (UTC epoch)
- Ages over real time; care quality shapes which form it evolves into
- Does not die of old age, but  under certain conditions; a new character can be born
- Due to marketing reasons, we don't use word "die", but "poof"

### Stats
- **Energy** — depletes over time; restored by feeding/sleeping
- (more stats to be added: hunger, happiness, mood, ...)

### Zodiac system (working title)
- 6 signs, each lasting 3 days → 18-day repeating cycle
- Sign is derived from birth timestamp, not current time
- Each sign has base modifiers (mood tendency, luck, social energy)
- 6×6 compatibility matrix drives encounter outcomes and passive mood effects

### Bluetooth interactions
- Devices advertise character sign and basic stats via BLE and Wi-Fi
- Proximity ([RSSI](https://en.wikipedia.org/wiki/Received_signal_strength_indicator)) used to gauge "nearness" — compatible signs lift mood passively
- Encounter memory: device remembers last N characters met (by ID)
- Repeated meetings feel different from first contact

### Day/night cycle
- Character sleeps during certain hours (RTC or elapsed ms)
- Reduced interaction window creates urgency

### Mini-game
- One simple reflex game tied to button timing
- Outcome modified by current day's sign "luck" stat
- Could be "rock paper scissors"

### Touchscreen latency
The render loop targets 100 ms per frame (10 FPS). For the care loop (feed,
play, dismiss) this is imperceptible — interactions are discrete taps with no
precision timing required, and 100 ms is within the range users find
responsive for button-style input. The original Tamagotchi was slower.

The one exception is a pure reflex mini-game: 100 ms of input latency eats
meaningfully into the player's reaction budget. **Design mini-game mechanics
to be forgiving of this** — rock-paper-scissors is fine; a narrow tap-window
game is not.

### Aura visualization
- Each sign has an associated color/glow
- Visible on screen; pulses during BLE encounters

## Architecture

```
common/             ← shared code (all platforms)
  app.h/c           ← app_t struct; app_init/spawn/poof/advance
  lua_bind.h/c      ← Lua VM init, schedule()/spawn() globals, event dispatch
  lua_gfx.h/c       ← Lua graphics globals (cls, spr, …)
  lua_anim.h/c      ← animation instance registry; Lua anim()/aspr() globals
  gfx.h/c           ← software renderer + PNG encoder
  apng.h/c          ← PNG/APNG decoder: returns flat RGBA frames
  spr.h/c           ← sprite registry and blitter (uses apng.h/c)
  server.h/c        ← HTTP command dispatch, SSE game-event push
  state.h/c         ← app ↔ JSON serialisation
  character.h/c     ← character struct, initialisation
  scheduler.h/c     ← generic min-heap priority queue (opaque tag)
  test_scheduler.c  ← scheduler unit tests (make)

platform/
  esp32/
    main.ino        ← Arduino setup()/loop(), RTC + BLE
  pc/
    main.c          ← argument parsing, entry point, main loop
    peer.c/h        ← stdin/stdout peer channel
    display.c/h     ← SDL2 window; blits framebuffer to screen
    Makefile

scripts/
  main.lua          ← game logic: spawn, energy drain
                       (may require additional .lua files in the same directory)

tests/
  features/
    smoke.feature             ← happy-path scenarios from README smoke test
    args.feature              ← CLI argument parsing, defaults, invalid inputs
    api.feature               ← HTTP API edge cases (bad inputs, state errors)
    schedule.feature          ← schedule() dispatch, prefix resolution, argument validation
    hot_reload.feature        ← live reload when a loaded Lua file changes; _on_reload SSE event
    lua_freeze.feature        ← global-write freeze; _on_lua_error SSE event
    stop_on_lua_error.feature ← --stop-on-lua-error flag and set/get_stop_on_lua_error commands
    graphics.feature          ← cls() and framebuffer rendering scenarios
    sprites.feature           ← spr() drawing: static PNG, APNG, alpha, fragments
    update_draw.feature       ← _update() and _draw() Lua callback scenarios
    environment.py  ← Behave hooks (emu lifecycle, temp-file cleanup)
    steps/
      emu_steps.py  ← shared Given/When/Then step definitions
      args_steps.py ← steps specific to CLI argument scenarios
      api_steps.py  ← steps specific to HTTP API scenarios
      sse_steps.py  ← SSE subscription and event assertion steps
      utils.py      ← shared helpers (EMU path, post, raw_request, start_emu)

vendor/
  mongoose/         ← embedded HTTP server (single file, MIT)
  cjson/            ← JSON parser/writer (single file, MIT)
  lua/              ← Lua 5.4 interpreter (MIT)
```

### Time model

Two independent clocks:

| Field | Type | Description |
|---|---|---|
| `app.now_tick` | virtual ms (can drift from wall clock) | Advances via `advance_time`. Drives all game logic and the scheduler. |
| `app.now_unix_sec` | Unix epoch seconds | Wall clock. Set from RTC (ESP32) or system clock / `set_wall_clock` (PC). Updated every real second in autotick mode. Used only for zodiac. |

The scheduler is a min-heap of `(fire_at_ms, tag)` events. `app_advance` pops
and dispatches events in chronological order up to a target tick. Game logic
registers recurring events rather than polling every tick.

`common/` provides the pure-C primitives (scheduler, world, character) that the
platform and scripts build on. The PC build is the primary development target;
behaviour is verified there before flashing to hardware.

### Lua scripting

Game logic lives in Lua scripts (`scripts/`). The Lua VM is embedded via
`lua_bind.c`; scripts are loaded at startup and expose event callbacks that
receive two arguments: `rw` (read-write scripted state) and `ro` (read-only
snapshot: `instance_id`, `now_tick`, `now_unix_sec`, `character`). Callbacks
that don't need both may simply declare fewer parameters.

`spawn()` and `schedule(delay_ms, name)` are available as Lua globals. `spawn()` creates the character; it raises a Lua error if a character already exists. Callback names are
**module-relative**: the dispatch layer automatically prepends the current
module's prefix, so a callback inside `energy.on_drain` uses just `"on_drain"`
and the engine stores `"energy.on_drain"`.

Scripts may be split across multiple files using `require`. The search path is
set to the directory containing the main script, so `require("energy")` loads
`energy.lua` from the same directory. Transitive requires work as expected.
The module should be assigned to a global (or a field of one) so the dispatch
layer can resolve its path by scanning `_G`:

```lua
energy = require("energy")         -- direct global
-- or
myapp = {}; myapp.energy = require("energy")  -- nested under a table
-- or
nrg = require("energy")            -- alias; prefix becomes "nrg."
```

Top-level callbacks in the main script have no prefix and must pass the full
dotted path when bootstrapping from `_init`:

```lua
-- main.lua
energy = require("energy")

function _init(rw)
    spawn()
    energy.init(rw)   -- energy.init() calls schedule(); prefix resolved via _G scan
end
```

```lua
-- energy.lua
local M = {}

function M.init(rw)
    schedule(5000, "on_drain")   -- dispatched as "energy.on_drain" (prefix from _G scan)
end

function M.on_drain(rw)
    schedule(5000, "on_drain")   -- prefix prepended automatically by dispatcher
end

return M
```

The same module may be required under multiple names (diamond dependency) — each
path dispatches and reschedules independently and correctly.

Event names are limited to 63 characters (after prefix expansion) and must not
start with `_` (reserved for system events); `schedule` raises a Lua error if
either constraint is violated.

After the script's top-level code runs, `_G` and all user-defined tables
reachable from it are frozen: any attempt to create a new global or add a new
field to a module table from inside a callback raises a Lua error.  The only
persistent state a callback may write is `rw`; local variables are unrestricted.
See `LUA_LINT.md` for known gaps and planned static analysis rules.

#### Game loop callbacks

`_init([rw [, ro]])` is called once when the script is loaded, after top-level code runs and
globals are frozen. Use it for one-time setup (e.g. registering animation instances). If `_init`
raises a Lua error the process exits — errors here are fatal. Hot reload does not trigger `_init`;
the previous `rw` state is preserved across reloads instead.

At startup, immediately after `_init`, the engine calls `_update` then `_draw` once at the initial tick. After that, each tick (every `AUTOTICK` ms of virtual time), the engine calls these Lua globals in order:

1. All scheduled `on_*` callbacks whose `fire_at_ms` falls within the current tick window.
2. `_update(rw [, ro])` — game logic; always called.
3. `_draw(rw [, ro])` — rendering; called after `_update`.

All three functions are optional. If not defined they are silently skipped. They receive the same
`rw` and `ro` arguments as `on_*` callbacks. `_update` may write to `rw`; in `_draw` the `rw`
argument is a recursive read-only proxy — any write attempt raises a Lua error.

```lua
function _update(rw, ro)
    rw.frame = (rw.frame or 0) % 4 + 1
end

function _draw(rw)
    cls(0x000000)
    spr("player.png", rw.frame, 100, 100)
end
```

#### Naming conventions

| Prefix | Meaning | Example |
|---|---|---|
| `on_` | Schedulable event callback | `on_energy_drain` |
| `_on_` | System event emitted by the engine; not schedulable by scripts | `_on_reload` |
| `_update` / `_draw` | Game loop callbacks; called every tick by the engine | — |

#### System events

| Event | Fired when | SSE data |
|---|---|---|
| `_on_reload` | Lua VM successfully reloaded after a source file change | `{"now_tick": N}` |
| `_on_lua_error` | A Lua callback throws an error (e.g. global-write violation) | `{"fn": "name", "error": "message"}` |

#### Graphics globals

The following graphics functions are available as Lua globals inside `_draw()` (and any function called from it). Calling them outside a draw context raises a Lua error.

| Function | Description |
|---|---|
| `cls(color)` | Fill the entire screen with `color` (24-bit `0xRRGGBB` integer) |
| `spr(path [, frame [, x [, y [, fx [, fy [, fw [, fh]]]]]]])` | Draw a PNG or APNG sprite from `path` (relative to the script file). `frame` is a one-based frame index (default 1; clamped for static PNGs). `x, y` set the screen position. `fx, fy, fw, fh` select a sub-region of the canvas (defaults: origin, full size). |
| `aspr(id [, x [, y [, fx [, fy [, fw [, fh]]]]]]])` | Draw the current frame of animation instance `id`. `x, y` set the screen position (default 0). `fx, fy, fw, fh` select a sub-region of the canvas (defaults: origin, full size). Sets the instance's "used in this draw" flag, keeping it alive. Raises a Lua error if `id` is not registered or has no frame count set. |

#### Animation globals

Animation instances track which frame of a sprite sequence should be shown each draw. They are created and configured outside `_draw()` (e.g. in `_init` or `on_*` handlers) and drawn via `aspr()` inside `_draw()`. Unused instances are garbage-collected automatically after any `_draw()` call in which they were not referenced.

```lua
-- create or reference an instance and configure it (calls are chainable)
anim("walk").of("walk.png").loop(true)

-- inside _draw(), draw the current frame
aspr("walk", x, y)
```

`anim(id)` returns a method table. Calling `anim(id)` when the instance already exists returns the same table without reinitialising it. Calling it inside `_draw()` raises a Lua error.

| Method | Description |
|---|---|
| `.of(path)` | Set the frame count from the sprite at `path` (loaded if not already cached). Must be called exactly once per instance; a second call raises a Lua error. |
| `.backwards(bool)` | Play in reverse when `true`. Default: `false`. |
| `.loop(bool)` | Loop when the last (or first, if backwards) frame is reached. Default: `false`. |
| `.pause()` | Pause playback; frame is not advanced on subsequent draw calls. |
| `.play()` | Resume playback. |

**Frame advancement** — after each `_draw()` call the engine advances every live instance one step:

- Forward, not at last frame → `current_frame++`
- Forward, at last frame, loop → `current_frame = 1`
- Forward, at last frame, no loop → stop playing
- Backwards, not at frame 1 → `current_frame--`
- Backwards, at frame 1, loop → `current_frame = n_frames`
- Backwards, at frame 1, no loop → stop playing

## PC instance

### Setup

After cloning, install the pre-commit hook (runs the full build pipeline before every commit):

```sh
ln -sf ../../scripts/pre-commit .git/hooks/pre-commit
```

Requires GCC (with ASan support), `clang-format`, Python 3 with `behave` and
`requests`, SDL2 (`libsdl2-dev`), apngasm, and [GitHub CLI](https://cli.github.com/) on Linux.

```sh
# Linux (Debian/Ubuntu)
sudo apt install gh python3 python3-pip libsdl2-dev apngasm
pip install behave requests
gh auth login
```

### Build and test

```sh
cd platform/pc
make
```

Runs format check, compiles `emu-asan`, runs unit tests, runs the full Behave
integration suite under AddressSanitizer, and fails if any ASan error reports
appear in `/tmp/emu-asan.<pid>`.

To build the production binary:

```sh
make emu     # produces emu (stripped, optimised)
```

To apply formatting:

```sh
make format         # apply formatting in-place
```

### Start

```sh
./emu [OPTIONS]

  --id=XXXXXXXX                             instance ID (8 hex digits); seeds PRNG
  --port=N                                  HTTP port (default: 7070)
  --nowtick=N                               initial virtual clock in ms (now_tick)
  --wallclockutc=YYYY-MM-DDTHH:MM:SS        initial wall-clock time (now_unix_sec)
  --file=PATH                               load world state from JSON file
  --script=PATH                             Lua game script (default: scripts/main.lua)
  --noautotick                              start in manual-tick mode
  --stop-on-lua-error                       halt advance and disable autotick on Lua error
  --headless                                suppress the SDL2 display window
  --help                                    show this help and exit
```

Diagnostic output goes to **stderr**. **stdout** is reserved for outgoing
peer messages (newline-delimited JSON).

### Smoke test

Start the instance in a terminal. `--nowtick` sets the virtual clock (game
logic); `--wallclockutc` sets the wall clock (zodiac). The two are independent:
`now_tick` advances via `advance_time`; `now_unix_sec` only changes via
`set_wall_clock`. The script's `_init` runs at startup and calls `spawn()` to
create the character.

```sh
./emu --id=DEADBEEF --nowtick=42 --wallclockutc=2026-04-08T00:00:00 --noautotick
```

In a second terminal, run these commands one by one.

**Initial state — character spawned by `_init`:**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"get_state"}' | python3 -m json.tool
```
```json
{
    "ok": true,
    "ro": {
        "instance_id": "DEADBEEF",
        "now_tick": 42,
        "now_unix_sec": 1775606400,
        "character": {
            "id": "14FE67E1",
            "birth_unix_sec": 1775606400,
            "birth_tick": 42
        }
    },
    "rw": {"energy": 255},
    "scheduler": [
        {"fire_at_ms": 339042, "event": "on_energy_drain"}
    ]
}
```

**Advance 1000 virtual ticks (1 simulated second; `now_tick` moves, `now_unix_sec` does not):**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"advance_time","ticks":1000}' | python3 -m json.tool
```
```json
{"ok": true, "now_tick": 1042, "stopped_on_event": false}
```

**Fast-forward to the next event (energy drain at `birth_tick` + 339,000 ms = 339,042):**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"advance_time","ticks":0,"stop_on_event":true}' | python3 -m json.tool
```
```json
{"ok": true, "now_tick": 339042, "stopped_on_event": true, "event": "on_energy_drain"}
```

**Check state (`now_tick` advanced to 339,042; `now_unix_sec` still 2026-04-08; energy 254):**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"get_state"}' | python3 -m json.tool
```
```json
{
    "ok": true,
    "ro": {
        "instance_id": "DEADBEEF",
        "now_tick": 339042,
        "now_unix_sec": 1775606400,
        "character": {
            "id": "14FE67E1",
            "birth_unix_sec": 1775606400,
            "birth_tick": 42
        }
    },
    "rw": {"energy": 254},
    "scheduler": [
        {"fire_at_ms": 678042, "event": "on_energy_drain"}
    ]
}
```

**Advance wall clock independently (e.g. to test a different zodiac sign):**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"set_wall_clock","now_unix_sec":1775606401}' | python3 -m json.tool
```
```json
{"ok": true}
```

**Enable auto-tick:**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"set_autotick","enabled":true}' | python3 -m json.tool
```
```json
{"ok": true, "autotick": true}
```

**Subscribe to push events (SSE — open in a separate terminal):**
```sh
curl -N http://localhost:7070/events
```
```
event: on_energy_drain
data: {"now_tick":678042}

event: on_energy_drain
data: {"now_tick":1017042}
```

Game events (`on_energy_drain`, …) are pushed as they fire. `peer_in` and
`peer_out` appear when peer interactions occur. In autotick mode the virtual
clock advances 1,000 ticks per real second, so `on_energy_drain` fires roughly
every 339 real seconds.

**Save state, restore it into a fresh instance:**
```sh
# get current state
STATE=$(curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"get_state"}' | python3 -c "import sys,json; d=json.load(sys.stdin); del d['ok']; print(json.dumps(d))")

# push it back (round-trip check)
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  --data "{\"cmd\":\"set_state\",\"state\":$STATE}" | python3 -m json.tool
```
```json
{"ok": true}
```

**Character poofs:**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"poof"}' | python3 -m json.tool
```
```json
{"ok": true}
```

After `poof`, `get_state` shows `"character": null`. A new character can be spawned by calling `spawn()` from the Lua script.

### Hot-reload

The emulator watches exactly the Lua files that were loaded at startup — the
main script plus every file pulled in transitively via `require`. When any of
them changes, the Lua VM is torn down, rebuilt from the main script, and the
previous `rw` state, scheduler, and animation instances are restored — so scheduled events keep
firing and game variables are preserved across reloads. The watched-file list
is refreshed after each successful reload, so adding or removing a `require`
takes effect immediately.

## ESP32 compatibility

### Architecture stance

The HTTP server layer is designed to work on ESP32 as well as PC. This enables
WiFi-based debugging on real hardware without a separate debug protocol.

### What is reusable on ESP32 without changes

- `common/` — everything: `app_t`, Lua binding, HTTP server, state serialisation, character, scheduler

Mongoose (`vendor/mongoose/`) has explicit ESP32/Arduino support and compiles
on that target without modification.

### What changes on ESP32

**`main.ino` sketch** drives the event loop via `mg_mgr_poll()` instead of
`delay()`. This is non-blocking and naturally accommodates BLE polling, button
reads, and rendering alongside the HTTP server:

```c
void setup() {
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) delay(100);

    app_init(&s_app, rtc_now(), 0);

    mg_mgr_init(&app.mgr);
    mg_http_listen(&app.mgr, "http://0.0.0.0:80", mg_event_handler, &app);
    mg_timer_add(&app.mgr, WORLD_TICK_S * 1000, MG_TIMER_REPEAT, tick_timer_fn, &app);
}

void loop() {
    mg_mgr_poll(&app.mgr, 10);  /* drives HTTP + tick timer */
    /* poll BLE, buttons, render */
}
```

**Peer channel** — on PC, peers communicate over stdin/stdout (`peer.c`). On
ESP32, BLE will play that role. The peer message envelope format (defined in
`PROTOCOL.md`) is shared; only the transport implementation differs.

### What is ESP32-only

- WiFi initialisation in `setup()`
- RTC read for initial `now_ts`
- `esp_random()` for character ID generation
- BLE peer transport (future `platform/esp32/peer_ble.c`)
