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
- Does not pass, but can escape under certain conditions; a new character can be born

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

### Aura visualization
- Each sign has an associated color/glow
- Visible on screen; pulses during BLE encounters

## Architecture

```
core/             ← pure C, zero platform dependencies
  character.h/c   ← stats, energy drain, state machine
  world.h/c       ← world container, tick driver, character spawning
  zodiac.h/c      ← sign cycle, compatibility matrix (TODO)

platform/
  common/
    app.h         ← app_t (world + mongoose mgr + autotick); shared by all platforms
  esp32/
    main.ino      ← Arduino setup()/loop(), RTC + BLE
  pc/
    main.c        ← argument parsing, entry point, main loop
    server.c/h    ← HTTP command dispatch, SSE push events
    peer.c/h      ← stdin/stdout peer channel
    state.c/h     ← world ↔ JSON serialisation
    Makefile

vendor/
  mongoose/       ← embedded HTTP server (single file, MIT)
  cjson/          ← JSON parser/writer (single file, MIT)
```

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

### Start

```sh
./emu [OPTIONS]

  --id=XXXXXXXX                   instance ID (8 hex digits); seeds PRNG
  --port=N                        HTTP port (default: 7070)
  --timeutc=YYYY-MM-DDTHH:MM:SS   initial virtual time (UTC)
  --file=PATH                     load world state from JSON file
  --noautotick                    start in manual-tick mode
```

Diagnostic output goes to **stderr**. **stdout** is reserved for outgoing
peer messages (newline-delimited JSON).

### Smoke test

Start the instance in a terminal (fixed ID and time for reproducibility):

```sh
./emu --id=DEADBEEF --timeutc=2026-04-08T00:00:00 --noautotick
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
        "now_ts": 1744070400,
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
        "now_ts": 1744070400,
        "autotick": false,
        "character": {
            "id": "14FE67E1",
            "birth_ts": 1744070400,
            "energy": 255,
            "drain_acc": 0
        }
    }
}
```

**Advance one tick (returns no state):**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"tick"}' | python3 -m json.tool
```
```json
{"ok": true}
```

**Check state after tick (`now_ts` and `drain_acc` incremented):**
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
        "now_ts": 1744070401,
        "autotick": false,
        "character": {
            "id": "14FE67E1",
            "birth_ts": 1744070400,
            "energy": 255,
            "drain_acc": 1
        }
    }
}
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
event: tick
data: {"now_ts":1744070402}

event: tick
data: {"now_ts":1744070403}
```

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

**Character escapes:**
```sh
curl -s -X POST http://localhost:7070/command \
  -H 'Content-Type: application/json' \
  -d '{"cmd":"escape"}' | python3 -m json.tool
```
```json
{"ok": true}
```

After `escape`, `get_state` shows `"character": null` and a new `spawn` is accepted.

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

## Development order

1. ~~Define core data model~~
2. ~~PC HTTP server: command protocol, SSE push, peer channel~~
3. Python + Behave BDD integration tests
4. Behaviour loop: feeding, sleep, aging
5. Zodiac system and compatibility
6. BLE encounters on ESP32
7. Graphics and `get_screen`
