# Gloxie Protocol Specification

## Overview

Each Gloxie instance runs as a self-contained process (PC) or firmware
image (ESP32). It exposes two independent communication channels:

| Channel       | Purpose                              | PC transport        | ESP32 transport  |
|---------------|--------------------------------------|---------------------|------------------|
| Orchestration | Commands, state queries, debugging   | HTTP over TCP       | HTTP over Wi-Fi  |
| Peer          | Device-to-device game interaction    | stdin / stdout      | Bluetooth (BLE)  |

All messages are JSON. The orchestration channel uses standard HTTP
request/response plus optional Server-Sent Events for push notifications.
The peer channel uses newline-delimited JSON (one JSON object per line).

### Design philosophy: polling over event-sourcing

The orchestration channel is intentionally **not** event-sourced. There is no
pattern of "subscribe once, then receive granular state-delta events." Instead,
callers poll with explicit requests like `get_state` whenever they need the
current snapshot.

Consequences of this choice:

- `advance_time` (both the command and the SSE `tick` event) returns only the
  new virtual timestamp. If the caller needs full state, it issues a separate
  `get_state`.
- SSE events (`tick`, `peer_in`, `peer_out`) are lightweight notifications, not
  state carriers. They signal that something happened; the caller decides whether
  to fetch updated state.
- There is no `hello` event that pushes initial state on SSE connect.

This keeps the protocol simple and makes test orchestration natural: a test
advances the world with `advance_time` commands, then reads state with
`get_state`, with no need to reconcile an event stream against a local state
replica.

---

## 1. Instance startup

### PC

```
emu [OPTIONS]

Options:
  --id=XXXXXXXX              Instance ID (8 hex digits). Seeds the PRNG and
                             is used for peer routing. Generated randomly if
                             omitted, but omitting it makes tests non-repeatable.
  --port=N                   HTTP port for the orchestration channel (default: 7070).
  --nowtick=N                              Initial virtual clock in ms (now_tick).
                                           Defaults to wall-clock time.
  --wallclockutc=YYYY-MM-DDTHH:MM:SS        Initial wall-clock time (now_unix_sec).
                                           Defaults to system clock.
  --file=PATH                Load state from a previously saved JSON file.
  --script=PATH              Lua game script. Defaults to scripts/main.lua
                             relative to the binary.
  --noautotick               Start in manual-tick mode. Default is auto-tick.
```

On startup the instance:
1. Initialises the world (empty — no character yet).
2. Starts the HTTP server on the given port.
3. Begins listening on stdin for incoming peer messages.
4. If auto-tick (default), starts advancing one tick per real second.

All diagnostic output (logs, errors) goes to **stderr**.  
**stdout is reserved exclusively for outgoing peer messages.**

### ESP32

Equivalent configuration is compiled in or loaded from NVS (non-volatile
storage). The instance ID is stored in NVS and generated on first boot.

---

## 2. Orchestration channel — HTTP

Base URL: `http://<host>:<port>/`  
Default port: **7070**

### 2.1 Commands — `POST /command`

**Request** (`Content-Type: application/json`):

```json
{ "cmd": "<command_name>", ...params }
```

**Success response** (`200 OK`, `Content-Type: application/json`):

```json
{ "ok": true, ...data }
```

**Error response** (`200 OK` or `4xx`):

```json
{ "ok": false, "error": "<human-readable message>" }
```

Errors always return a JSON body regardless of HTTP status code.

---

#### `advance_time`

Advance the virtual clock, firing all scheduled events in order.
Returns the new virtual timestamp — use `get_state` for full state.

Request:
```json
{ "cmd": "advance_time", "ticks": 5000 }
```

Required fields:
- `"ticks": N` — non-negative integer. Use `0` as the "unlimited" sentinel:
  advance to the next scheduled event (or no-op if `stop_on_event` is false).

Optional fields:
- `"stop_on_event": true` — stop immediately after the first scheduled event
  fires, even if `ticks` has not elapsed. Default: `false`.

**Behaviour matrix:**

| `ticks`  | `stop_on_event` | Behaviour |
|---|---|---|
| N > 0 | false | Advance full N ticks; fire all events in range. |
| N > 0 | true  | Advance up to N ticks; stop after first event. |
| 0     | true  | Advance to next scheduled event; if none, do not advance. |
| 0     | false | No-op. |

Response:
```json
{ "ok": true, "now_tick": 339000, "stopped_on_event": false }
```

When `stopped_on_event` is `true`, an `"event"` field names what fired:
```json
{ "ok": true, "now_tick": 339000, "stopped_on_event": true, "event": "on_energy_drain" }
```

When `stopped_on_event` is `true`, also pushes an SSE event named after the fired game event, with `{"now_tick": N}` as data.

---

#### `set_wall_clock`

Override the wall-clock time used for zodiac and other real-time features.
In normal operation the wall clock is set to system time at startup; this
command lets tests control it without relying on the system clock.

Request:
```json
{ "cmd": "set_wall_clock", "now_unix_sec": 1744070400 }
```

Response:
```json
{ "ok": true }
```

---

#### `get_wall_clock`

Return the current wall-clock time.

Request:
```json
{ "cmd": "get_wall_clock" }
```

Response:
```json
{ "ok": true, "now_unix_sec": 1744070400 }
```

---

#### `get_state`

Return the full current state.

Request:
```json
{ "cmd": "get_state" }
```

Response: state fields at top level alongside `ok`:
```json
{ "ok": true, "script": "...", "ro": {...}, "rw": {...}, "scheduler": [...] }
```

---

#### `spawn`

Create a new character. Fails if a character already exists (use `poof` first to clear the current one).

Request:
```json
{ "cmd": "spawn" }
```

Optional fields:
- `"character_id": "XXXXXXXX"` — 8 hex digits; random (seeded by instance ID)
  if omitted.

The instance ID and character ID are independent: the instance ID identifies
the device/process; the character ID identifies the creature and survives
save/load/transfer across devices.

Response: state fields at top level alongside `ok` (same shape as `get_state`).

---

#### `poof`

The current character poofs — it leaves the device, clearing the slot so a
new character can be spawned. Fails if no character is present.

Request:
```json
{ "cmd": "poof" }
```

Response:
```json
{ "ok": true }
```

---

#### `get_autotick`

Return the current autotick mode.

Request:
```json
{ "cmd": "get_autotick" }
```

Response:
```json
{ "ok": true, "autotick": true }
```

---

#### `set_autotick`

Switch between automatic and manual tick modes.

Request:
```json
{ "cmd": "set_autotick", "enabled": true }
```

Response:
```json
{ "ok": true, "autotick": true }
```

---

#### `get_screen`

Return a PNG rendering of the current screen.

Request:
```json
{ "cmd": "get_screen" }
```

Response (`Content-Type: image/png`, binary body):

The raw PNG file. Unlike all other commands, this response is not JSON.
The caller must inspect `Content-Type` to distinguish it from error responses,
which remain `application/json` with an `{"ok": false, ...}` body.

---

#### `set_state`

Replace the current world state. Symmetric counterpart to `get_state`.
Intended for save/restore, test setup, and state transfer between instances.

Request:
```json
{ "cmd": "set_state", "state": <State> }
```

Response:
```json
{ "ok": true }
```

---

### 2.2 Push events — `GET /events`

A long-lived Server-Sent Events (SSE) stream. The client opens this endpoint
once and receives events as they occur.

```
GET /events HTTP/1.1
Accept: text/event-stream
```

Each event is formatted per the SSE spec:

```
event: <event_name>\n
data: <JSON>\n
\n
```

#### Game events (e.g. `on_energy_drain`)

Fired when a scheduled game event is processed during an autotick or
`advance_time` call. The SSE event name is the game event name; data carries
the virtual tick at which it fired.

```
event: on_energy_drain
data: {"now_tick": 339042}
```

Use `get_state` afterwards if the full updated state is needed.

#### `peer_out`

Fired when the instance's character emits a peer message (mirrors the stdout
write, for orchestrators that prefer HTTP over stdout parsing).

```
event: peer_out
data: <PeerMessage>
```

#### `peer_in`

Fired when the instance's character receives and processes a peer message.

```
event: peer_in
data: <PeerMessage>
```

---

## 3. Peer channel

Peer messages carry character-to-character game interaction: presence
announcements, greetings, mood effects, etc. An instance with no active
character does not send or accept peer messages.

`from` and `to` are always **character IDs**, not device/instance IDs.
Device routing is a transport concern invisible at the game-logic level.

### 3.1 PC transport (stdin / stdout)

- **Outgoing** (instance → orchestrator): the instance writes one JSON object
  per line to **stdout**.
- **Incoming** (orchestrator → instance): the orchestrator writes one JSON
  object per line to the instance's **stdin**.

The orchestrator routes messages by mapping character IDs to instances (it
builds this map from `get_state` responses). It reads stdout from all managed
instances, inspects the `to` field, and forwards each message to the
appropriate instance's stdin (fan-out for `"*"`).

Peer discovery on PC is orchestrator-mediated: instances do not discover each
other directly.

### 3.2 ESP32 transport (BLE)

Peer messages are exchanged directly over BLE. Discovery uses BLE advertising.
Character IDs are included in the message payload; the device layer handles
transport. The orchestrator is not involved in routing; it may observe peer
events via the SSE `peer_in` / `peer_out` events if connected.

---

### 3.3 Peer message format

All peer messages share a common envelope:

```json
{
  "peer_msg": "<type>",
  "from": "XXXXXXXX",
  "to":   "XXXXXXXX | *"
}
```

`from` and `to` are character IDs. `"*"` in `to` means broadcast to all
reachable characters.

`to: "*"` means broadcast to all reachable peers.

#### `announce`

Periodic presence broadcast. Receivers update their neighbour table.

```json
{
  "peer_msg": "announce",
  "from":     "DEADBEEF",
  "to":       "*",
  "sign":     2,
  "energy":   200
}
```

#### `greet`

Direct greeting between two instances. May trigger mood effects based on sign
compatibility.

```json
{
  "peer_msg": "greet",
  "from": "DEADBEEF",
  "to":   "3F8A21CC"
}
```

Additional peer message types (play, gift, challenge, …) are deferred until
game mechanics are further defined.

---

## 4. Data schemas

### `State`

State fields appear at the top level of every `get_state` / `spawn` response
(alongside `"ok": true`), and are passed as the value of the `"state"` key in
`set_state` requests.

No character spawned yet (or after `poof`):
```json
{
  "ro": {
    "instance_id": "DEADBEEF",
    "now_tick":    0,
    "now_unix_sec": 1744063205,
    "character":   null
  },
  "rw": {},
  "scheduler": []
}
```

With an active character:
```json
{
  "ro": {
    "instance_id": "DEADBEEF",
    "now_tick":    5000,
    "now_unix_sec": 1744063205,
    "character": {
      "id":             "3F8A21CC",
      "birth_unix_sec": 1744063200,
      "birth_tick":     0
    }
  },
  "rw": {"energy": 255},
  "scheduler": [
    {"fire_at_ms": 344000, "event": "on_energy_drain"}
  ]
}
```

`ro.character` is `null` when no character is present.

| Field | Description |
|---|---|
| `script` | Path to the loaded Lua game script. |
| `ro` | Read-only snapshot passed to Lua callbacks as the first argument. |
| `ro.instance_id` | Device/process identity (8 hex digits). |
| `ro.now_tick` | Virtual clock in ms. Advanced by `advance_time`. Drives game logic. |
| `ro.now_unix_sec` | Wall-clock UTC seconds. Set by `set_wall_clock` or system clock. Used for zodiac. |
| `ro.character.birth_unix_sec` | Wall-clock UTC seconds at character birth. Used to derive zodiac sign. |
| `ro.character.birth_tick` | Virtual clock at character birth. Virtual age = `now_tick − birth_tick`. |
| `rw` | Lua-owned read-write state. Contents are defined by the game script. Passed to callbacks as the second argument. |
| `scheduler` | Pending scheduled events: array of `{"fire_at_ms": N, "event": "name"}`. |

### Timestamps

`now_unix_sec` and `birth_unix_sec` are UTC Unix epoch **seconds** (`uint64_t`
on the wire as a JSON integer). `now_tick` and `birth_tick` are virtual clock
values in **milliseconds** — the same numeric unit, but representing virtual
time that may diverge from wall-clock time.

### IDs

All IDs are 8 upper-case hex digits represented as JSON strings
(`"DEADBEEF"`), matching the `uint32_t` value they encode.

---

## 5. Error handling

- Commands with missing required fields return `{"ok": false, "error": "..."}`.
- Commands that are not applicable in the current state (e.g. `spawn` when a
  character exists, or `poof` when none does) return an error rather than
  silently succeeding.
- The HTTP server returns `400 Bad Request` for malformed JSON bodies.
- `get_screen` sent to an instance that has not yet implemented rendering
  returns `{"ok": false, "error": "not implemented"}`.

---

## 6. Versioning

The protocol version is not yet included in messages. Once the spec
stabilises a `"protocol_version"` field will be added to the `State` object
and to the SSE stream's opening `hello` event.
