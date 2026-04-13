Feature: HTTP API edge cases

  Background:
    Given emu starts with args "--id=DEADBEEF --nowtick=42 --noautotick"

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
