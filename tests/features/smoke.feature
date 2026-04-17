Feature: Smoke — happy path from README

  Background:
    Given a running emu with id "DEADBEEF" nowtick 42 wallclock "2026-04-08T00:00:00" in manual-tick mode

  Scenario: Initial state has no character
    When I get state
    Then the response is ok
    And now_tick is 42
    And now_unix_sec is 1775606400
    And there is no character
    And the script path ends with "scripts/energy.lua"

  Scenario: Wall clock can be set and read back independently
    When I set wall clock to 1775606401
    Then the response is ok
    When I get wall clock
    Then now_unix_sec is 1775606401

  Scenario: Character lifecycle — spawn, advance, energy drain, poof
    When I spawn a character
    Then the response is ok
    And the character id is "14FE67E1"
    And birth_unix_sec is 1775606400
    And birth_tick is 42
    And scripted energy is 255
    And the scheduler has an "energy_drain" event at tick 339042

    When I advance 1000 ticks
    Then the response is ok
    And now_tick is 1042
    And stopped_on_event is false

    When I advance to the next event
    Then the response is ok
    And now_tick is 339042
    And stopped_on_event is true
    And event is "energy_drain"

    When I get state
    Then now_tick is 339042
    And scripted energy is 254
    And the scheduler has an "energy_drain" event at tick 678042

    When I poof the character
    Then the response is ok

    When I get state
    Then there is no character
    And the scheduler is empty
