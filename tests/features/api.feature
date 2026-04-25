Feature: HTTP API edge cases

  Background:
    Given emu starts with args "--nowtick=42 --noautotick"

  # ---------------------------------------------------------------------------
  # HTTP protocol
  # ---------------------------------------------------------------------------

  Scenario: GET /command is rejected with 405
    When I send a GET request to "/command"
    Then the HTTP status is 405
    And the response has ok false

  Scenario: POST to an unknown path returns 404
    When I send a POST request to "/bogus"
    Then the HTTP status is 404
    And the response has ok false

  Scenario: malformed JSON body returns 400
    When I POST raw body "not json" to "/command"
    Then the HTTP status is 400
    And the response has ok false

  # ---------------------------------------------------------------------------
  # Command dispatch
  # ---------------------------------------------------------------------------

  Scenario: missing cmd field
    When I post command:
      """
      {}
      """
    Then the response has ok false
    And the error is "missing cmd"

  Scenario: unknown cmd value
    When I post command:
      """
      {"cmd": "fly_to_moon"}
      """
    Then the response has ok false
    And the error is "unknown command"

  # ---------------------------------------------------------------------------
  # advance_time
  # ---------------------------------------------------------------------------

  Scenario: advance_time with missing ticks
    When I post command:
      """
      {"cmd": "advance_time"}
      """
    Then the response has ok false

  Scenario: advance_time with negative ticks
    When I post command:
      """
      {"cmd": "advance_time", "ticks": -1}
      """
    Then the response has ok false

  Scenario: advance_time with fractional ticks
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 1.5}
      """
    Then the response has ok false

  Scenario: advance_time ticks=0 stop_on_event=false is a no-op
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 0, "stop_on_event": false}
      """
    Then the response is ok
    And now_tick is 42
    And stopped_on_event is false

  Scenario: advance_time ticks=0 stop_on_event=true with no events pending is a no-op
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 0, "stop_on_event": true}
      """
    Then the response is ok
    And now_tick is 42
    And stopped_on_event is false

  # ---------------------------------------------------------------------------
  # spawn / poof
  # ---------------------------------------------------------------------------

  Scenario: spawning when a character already exists is rejected
    When I spawn a character
    And I post command:
      """
      {"cmd": "spawn"}
      """
    Then the response has ok false
    And the error is "character already exists"

  Scenario: poof when no character is present is rejected
    When I post command:
      """
      {"cmd": "poof"}
      """
    Then the response has ok false
    And the error is "no character"

  # ---------------------------------------------------------------------------
  # set_autotick
  # ---------------------------------------------------------------------------

  Scenario: set_autotick with missing enabled field
    When I post command:
      """
      {"cmd": "set_autotick"}
      """
    Then the response has ok false

  Scenario: set_autotick with non-boolean enabled
    When I post command:
      """
      {"cmd": "set_autotick", "enabled": "yes"}
      """
    Then the response has ok false

  # ---------------------------------------------------------------------------
  # set_wall_clock
  # ---------------------------------------------------------------------------

  Scenario: set_wall_clock with missing now_unix_sec
    When I post command:
      """
      {"cmd": "set_wall_clock"}
      """
    Then the response has ok false

  Scenario: set_wall_clock with a negative value
    When I post command:
      """
      {"cmd": "set_wall_clock", "now_unix_sec": -1}
      """
    Then the response has ok false

  Scenario: set_wall_clock with a non-numeric value
    When I post command:
      """
      {"cmd": "set_wall_clock", "now_unix_sec": "woe is me"}
      """
    Then the response has ok false

  Scenario: set_wall_clock with a fractional value
    When I post command:
      """
      {"cmd": "set_wall_clock", "now_unix_sec": 1.5}
      """
    Then the response has ok false

  # ---------------------------------------------------------------------------
  # set_state
  # ---------------------------------------------------------------------------

  Scenario: set_state restores now_tick and clears character
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 9000, "now_unix_sec": 1775606400, "character": null},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response is ok
    When I get state
    Then now_tick is 9000
    And there is no character

  Scenario: set_state restores a character with rw state
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 500, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {"energy": 200}, "scheduler": []
      }}
      """
    Then the response is ok
    When I get state
    Then now_tick is 500
    And the character id is "CAFEBABE"
    And energy is 200

  Scenario: set_state restores scheduler
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {"energy": 200},
        "scheduler": [{"fire_at_ms": 5000, "event": "on_energy_drain"}]
      }}
      """
    Then the response is ok
    When I get state
    Then the scheduler has an "on_energy_drain" event at tick 5000

  Scenario: set_state restores scheduler and fires events on advance
    Given I subscribe to SSE events
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {"energy": 200},
        "scheduler": [{"fire_at_ms": 5000, "event": "on_energy_drain"}]
      }}
      """
    Then the response is ok
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 4900, "stop_on_event": true}
      """
    Then the response is ok
    And stopped_on_event is true
    And event is "on_energy_drain"
    And I receive an SSE "on_energy_drain" event at now_tick 5000
    When I get state
    Then energy is 199

  Scenario: set_state with missing state field
    When I post command:
      """
      {"cmd": "set_state"}
      """
    Then the response has ok false

  Scenario: set_state with an invalid state object
    When I post command:
      """
      {"cmd": "set_state", "state": {"garbage": true}}
      """
    Then the response has ok false

  Scenario: set_state with missing ro.instance_id is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response has ok false

  Scenario: set_state with missing ro.now_tick is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response has ok false

  Scenario: set_state with missing ro.now_unix_sec is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response has ok false

  Scenario: set_state with missing ro.character is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response has ok false

  Scenario: set_state with missing character.id is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response has ok false

  Scenario: set_state with missing character.birth_unix_sec is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_tick": 0}},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response has ok false

  Scenario: set_state with missing character.birth_tick is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400}},
        "rw": {}, "scheduler": []
      }}
      """
    Then the response has ok false

  Scenario: set_state with scheduler null treats it as empty
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}, "scheduler": null
      }}
      """
    Then the response is ok
    When I get state
    Then the scheduler is an empty array

  Scenario: set_state with scheduler omitted treats it as empty
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}
      }}
      """
    Then the response is ok
    When I get state
    Then the scheduler is an empty array

  Scenario: set_state with rw null treats it as empty
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": null, "scheduler": []
      }}
      """
    Then the response is ok
    When I get state
    Then rw is an empty object

  Scenario: set_state with rw omitted treats it as empty
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "scheduler": []
      }}
      """
    Then the response is ok
    When I get state
    Then rw is an empty object

  Scenario: set_state with scheduler entry missing fire_at_ms is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}, "scheduler": [{"event": "on_energy_drain"}]
      }}
      """
    Then the response has ok false

  Scenario: set_state with scheduler entry missing event is rejected
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 1775606400,
               "character": {"id": "CAFEBABE", "birth_unix_sec": 1775606400, "birth_tick": 0}},
        "rw": {}, "scheduler": [{"fire_at_ms": 5000}]
      }}
      """
    Then the response has ok false

  Scenario: set_state completely replaces existing state rather than merging
    When I spawn a character
    And I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 200, "now_unix_sec": 1775606400,
               "character": {"id": "AABBCCDD", "birth_unix_sec": 1775606400, "birth_tick": 200}},
        "rw": {"energy": 100}, "scheduler": []
      }}
      """
    Then the response is ok
    When I get state
    Then now_tick is 200
    And the character id is "AABBCCDD"
    And energy is 100
    And the scheduler is an empty array
