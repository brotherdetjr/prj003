# Tamagotchi-like toy — MaTouch ESP32-S3 (ST7701S 3.16")

## Concept

A pocket creature toy with no internet, no AI. The creature lives on the device,
ages, needs care, and meets other creatures via Bluetooth.

## Hardware

- **Device:** MaTouch ESP32-S3 Parallel TFT 3.16" (ST7701S)
- **Controls:** up to 3 buttons, optional orientation sensor (accelerometer)
- **Radio:** Bluetooth (BLE) for device-to-device discovery and interaction

## Core mechanics

### Character lifecycle
- Born with a generated ID and a birth timestamp (UTC epoch)
- Ages over real time; care quality shapes which form it evolves into
- Eventually passes; a new character can be born

### Stats
- **Energy** — depletes over time; restored by feeding/sleeping
- (more stats to be added: hunger, happiness, mood, ...)

### Zodiac system (working title)
- 6 signs, each lasting 3 days → 18-day repeating cycle
- Sign is derived from birth timestamp, not current time
- Each sign has base modifiers (mood tendency, luck, social energy)
- 6×6 compatibility matrix drives encounter outcomes and passive mood effects

### Bluetooth interactions
- Devices advertise character sign and basic stats via BLE
- Proximity (RSSI) used to gauge "nearness" — compatible signs lift mood passively
- Encounter memory: device remembers last N characters met (by ID)
- Repeated meetings feel different from first contact

### Day/night cycle
- Character sleeps during certain hours (RTC or elapsed ms)
- Reduced interaction window creates urgency

### Mini-game
- One simple reflex game tied to button timing
- Outcome modified by current day's sign "luck" stat

### Aura visualization
- Each sign has an associated color/glow
- Visible on screen; pulses during BLE encounters

## Architecture

```
core/             ← pure C, zero platform dependencies
  character.h/c   ← stats, energy drain, state machine
  world.h/c       ← world container, tick driver, character spawning
  zodiac.h/c      ← sign cycle, compatibility matrix (TODO)
  render_api.h    ← abstract: draw_sprite, draw_rect, play_tone (TODO)

platform/
  esp32/
    main.ino      ← Arduino setup()/loop(), RTC + BLE
  desktop/
    main.c        ← argument parsing, entry point
    emu.c         ← terminal emulator (screen, console, file I/O)
    emu.h
    Makefile
```

Business logic lives entirely in `core/`. Platform layers implement the
abstract interfaces. This makes the desktop emulator a first-class target —
iterate fast on PC, then flash to hardware.

## Desktop emulator

### Build

```
cd platform/desktop
make
```

Requires GCC and a POSIX terminal (Linux/macOS). No external libraries.

### Usage

```
./emu                                      # empty device, wall-clock time
./emu --timeutc=2026-04-07T23:40:00        # empty device, fixed start time
./emu --noautotick                         # manual tick mode
./emu --file=mystate.json                  # load saved state
```

**Auto-tick mode (default):** the virtual clock advances by one second every
real second. The screen refreshes after each tick.

**Manual mode (`--noautotick`):** the clock only moves when the user presses
Enter or Space. Useful for step-by-step debugging.

### Main screen

```
=== TAMAGOTCHI EMULATOR ===

Virtual time  : 2026-04-07T23:40:05 UTC
File          : mystate.json [modified]

---- CHARACTER ----
ID            : 3F8A21CC
Born          : 2026-04-07T23:40:00 UTC
Energy        :  255 / 255

[auto-tick]  ESC = console
```

Press **ESC** at any time to pause and enter the dev console.

### Dev console commands

| Command         | Description                              |
|-----------------|------------------------------------------|
| `new`           | Create a new character at current time   |
| `tick`          | Advance one tick manually                |
| `save [file]`   | Save state to file (JSON)                |
| `resume`        | Return to main screen                    |
| `exit`          | Quit (prompts to save if unsaved changes)|
| `help`          | List commands                            |

### Save file format

State is saved as human-readable JSON, e.g.:

```json
{
  "now_ts": 1744063205,
  "has_character": true,
  "character": {
    "id": "3F8A21CC",
    "birth_ts": 1744063200,
    "energy": 255,
    "drain_acc": 5
  }
}
```

The file can be inspected or hand-edited between sessions.

## Development order

1. ~~Define core data model~~
2. ~~Desktop emulator: terminal UI, file save/load~~
3. Behavior loop: feeding, sleep, aging
4. Zodiac system and compatibility
5. BLE encounters on ESP32
6. Graphics and animations
