# Gloxie

## Concept

Gloxie is a pocket creature toy with no internet, no AI. The creature lives on
the device, ages, needs care, and meets other creatures via Bluetooth.

## Hardware

- **Target Device Candidate:** MaTouch ESP32-S3 Parallel TFT 3.16" (ST7701S)
- **Controls:** up to 3 buttons (touchscreen?), optional orientation sensor (accelerometer)
- **Radio:** Bluetooth (BLE) for device-to-device discovery and interaction

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
core/               ← pure C, zero platform dependencies
  scheduler.h/c     ← generic min-heap priority queue (opaque tag)
  world.h/c         ← world container, event dispatch, character spawning
  character.h/c     ← character struct, initialisation
  zodiac.h/c        ← sign cycle, compatibility matrix (TODO)
  test_scheduler.c  ← scheduler unit tests (make test)

platform/
  common/
    app.h           ← app_t (world + mongoose mgr + autotick flag + AUTOTICK constant)
  esp32/
    main.ino        ← Arduino setup()/loop(), RTC + BLE
  pc/
    main.c          ← argument parsing, entry point, main loop
    server.c/h      ← HTTP command dispatch, SSE game-event push
    peer.c/h        ← stdin/stdout peer channel
    state.c/h       ← world ↔ JSON serialisation
    Makefile

tests/
  features/
    smoke.feature   ← happy-path scenarios from README smoke test
    args.feature    ← CLI argument parsing, defaults, invalid inputs
    api.feature     ← HTTP API edge cases (bad inputs, state errors)
    environment.py  ← Behave hooks (emu lifecycle, temp-file cleanup)
    steps/
      steps.py      ← shared Given/When/Then step definitions
      args_steps.py ← steps specific to CLI argument scenarios
      api_steps.py  ← steps specific to HTTP API scenarios
      utils.py      ← shared helpers (EMU path, post, raw_request, start_emu)

vendor/
  mongoose/         ← embedded HTTP server (single file, MIT)
  cjson/            ← JSON parser/writer (single file, MIT)
```

### Time model

Two independent clocks:

| Field | Type | Description |
|---|---|---|
| `world.now_tick` | virtual ticks, similar to millisecond during the actual application execution, though can drift away from the wall clock time | Advances via `advance_time`. Drives all game logic and the scheduler. |
| `world.now_unix_sec` | Unix Epoch seconds | Wall clock. Set from RTC (ESP32) or system clock / `set_wall_clock` (PC). Updated every real second in autotick mode. Used only for zodiac. |

The scheduler is a min-heap of `(fire_at_ms, tag)` events. `world_advance` pops
and dispatches events in chronological order up to a target tick. Game logic
(energy drain, future: hunger, sleep, …) registers recurring events rather than
polling every tick.

Business logic lives entirely in `core/`. Platform layers talk to it through
the public API in `world.h` and `character.h`. The PC build is the primary
development target; behaviour is verified there before flashing to hardware.

## PC instance

### Build

```sh
cd platform/pc
make
```

Requires GCC on Linux or macOS. No system libraries beyond libc.

### Tests

Unit tests (scheduler):

```sh
cd platform/pc
make test
```

Integration tests (Behave/Cucumber) — requires Python 3 with `behave` and
`requests`:

```sh
pip install behave requests
cd tests
python3 -m behave
```

Build the `emu` binary before running integration tests.

### Start

```sh
./emu [OPTIONS]

  --id=XXXXXXXX                             instance ID (8 hex digits); seeds PRNG
  --port=N                                  HTTP port (default: 7070)
  --nowtick=N                               initial virtual clock in ms (now_tick)
  --wallclockutc=YYYY-MM-DDTHH:MM:SS        initial wall-clock time (now_unix_sec)
  --file=PATH                               load world state from JSON file
  --noautotick                              start in manual-tick mode
  --help                                    show this help and exit
```

Diagnostic output goes to **stderr**. **stdout** is reserved for outgoing
peer messages (newline-delimited JSON).

### Smoke test

Start the instance in a terminal. `--nowtick` sets the virtual clock (game
logic); `--wallclockutc` sets the wall clock (zodiac). The two are independent:
`now_tick` advances via `advance_time`; `now_unix_sec` only changes via
`set_wall_clock`.

```sh
./emu --id=DEADBEEF --nowtick=42 --wallclockutc=2026-04-08T00:00:00 --noautotick
```

In a second terminal, run these commands one by one.

**Empty state — no character yet:**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"get_state"}' | python3 -m json.tool
```
```json
{
    "ok": true,
    "state": {
        "instance_id": "DEADBEEF",
        "now_tick": 42,
        "now_unix_sec": 1775606400,
        "autotick": false,
        "character": null
    }
}
```

**Spawn a character:**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"spawn"}' | python3 -m json.tool
```
```json
{
    "ok": true,
    "state": {
        "instance_id": "DEADBEEF",
        "now_tick": 42,
        "now_unix_sec": 1775606400,
        "autotick": false,
        "character": {
            "id": "14FE67E1",
            "birth_unix_sec": 1775606400,
            "birth_tick": 42,
            "energy": 255
        }
    }
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
{"ok": true, "now_tick": 339042, "stopped_on_event": true, "event": "energy_drain"}
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
    "state": {
        "instance_id": "DEADBEEF",
        "now_tick": 339042,
        "now_unix_sec": 1775606400,
        "autotick": false,
        "character": {
            "id": "14FE67E1",
            "birth_unix_sec": 1775606400,
            "birth_tick": 42,
            "energy": 254
        }
    }
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
event: energy_drain
data: {"now_tick":678042}

event: energy_drain
data: {"now_tick":1017042}
```

Game events (`energy_drain`, …) are pushed as they fire. `peer_in` and
`peer_out` appear when peer interactions occur. In autotick mode the virtual
clock advances 1,000 ticks per real second, so `energy_drain` fires roughly
every 339 real seconds.

**Save state, restore it into a fresh instance:**
```sh
# get current state
STATE=$(curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"get_state"}' | python3 -c "import sys,json; print(json.dumps(json.load(sys.stdin)['state']))")

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

After `poof`, `get_state` shows `"character": null` and a new `spawn` is accepted.

## ESP32 compatibility

### Architecture stance

The HTTP server layer is designed to work on ESP32 as well as PC. This enables
WiFi-based debugging on real hardware without a separate debug protocol.

### What is reusable on ESP32 without changes

- `core/` — pure C, zero platform dependencies
- `platform/common/app.h` — `app_t` struct (world + Mongoose manager + autotick)
- `platform/pc/server.c/h` — HTTP command dispatch and SSE push
- `platform/pc/state.c/h` — world ↔ JSON serialisation

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

    world_init(&s_world, rtc_now());
    world_spawn_character(&s_world, esp_random());

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
