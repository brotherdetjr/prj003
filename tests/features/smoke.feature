Feature: Smoke — happy path from README

  Background:
    Given emu starts with args "--id=DEADBEEF --nowtick=42 --wallclockutc=2026-04-08T00:00:00 --noautotick"

  Scenario: Initial state has no character
    When I get state
    Then the response is ok
    And now_tick is 42
    And now_unix_sec is 1775606400
    And there is no character

  Scenario: Wall clock can be set and read back independently
    When I post command:
      """
      {"cmd": "set_wall_clock", "now_unix_sec": 1775606401}
      """
    Then the response is ok
    When I post command:
      """
      {"cmd": "get_wall_clock"}
      """
    Then now_unix_sec is 1775606401

  Scenario: Character lifecycle — spawn, advance, energy drain, poof
    When I spawn a character
    Then the response is ok
    And the character id is "14FE67E1"
    And birth_unix_sec is 1775606400
    And birth_tick is 42
    And energy is 255
    And the scheduler has an "on_energy_drain" event at tick 339042

    When I post command:
      """
      {"cmd": "advance_time", "ticks": 1000}
      """
    Then the response is ok
    And now_tick is 1042
    And stopped_on_event is false

    When I post command:
      """
      {"cmd": "advance_time", "ticks": 0, "stop_on_event": true}
      """
    Then the response is ok
    And now_tick is 339042
    And stopped_on_event is true
    And event is "on_energy_drain"

    When I get state
    Then now_tick is 339042
    And energy is 254
    And the scheduler has an "on_energy_drain" event at tick 678042

    When I post command:
      """
      {"cmd": "poof"}
      """
    Then the response is ok

    When I get state
    Then there is no character
    And the scheduler is an empty array
