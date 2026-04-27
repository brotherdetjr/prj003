Feature: Lua schedule() — dispatch, prefix resolution, and argument validation

  # ---------------------------------------------------------------------------
  # Prefix resolution: schedule() uses the real _G path of the calling module
  # ---------------------------------------------------------------------------

  Scenario Outline: prefix follows the global variable path — <description>
    Given emu starts with test script "<script>" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has an "<event>.on_drain" event at tick 5000

    When I advance to the next event
    Then the response is ok
    And stopped_on_event is true
    And event is "<event>.on_drain"

    When I get state
    Then energy is 9
    And the scheduler has an "<event>.on_drain" event at tick 10000

    Examples:
      | description                     | script                                   | event                |
      | same-name require               | require_test/main.lua                    | drain                |
      | same name via init()            | module_init_test/main.lua                | energy               |
      | alias via init()                | module_alias_test/main.lua               | nrg                  |
      | alias via direct schedule       | module_alias_direct_test/main.lua        | nrg                  |
      | nested path via init()          | module_nested_test/main.lua              | myapp.energy         |
      | nested path via direct schedule | module_nested_direct_test/main.lua       | myapp.energy         |
      | deep nested path via init()     | module_deep_nested_test/main.lua         | myapp.systems.energy |

  # ---------------------------------------------------------------------------
  # Diamond require: same module mounted at two paths dispatches independently
  # ---------------------------------------------------------------------------

  Scenario: same module reached via two distinct paths dispatches and reschedules independently
    Given emu starts with test script "diamond_test/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has 2 event(s)
    And the scheduler has an "link1.drain.on_drain" event at tick 10000
    And the scheduler has an "link2.drain.on_drain" event at tick 11000

    When I advance to the next event
    Then the response is ok
    And now_tick is 10000
    And stopped_on_event is true
    And event is "link1.drain.on_drain"

    When I get state
    Then energy is 9
    And the scheduler has 2 event(s)
    And the scheduler has an "link2.drain.on_drain" event at tick 11000
    And the scheduler has an "link1.drain.on_drain" event at tick 15000

    When I advance to the next event
    Then the response is ok
    And now_tick is 11000
    And stopped_on_event is true
    And event is "link2.drain.on_drain"

    When I get state
    Then energy is 8
    And the scheduler has 2 event(s)
    And the scheduler has an "link1.drain.on_drain" event at tick 15000
    And the scheduler has an "link2.drain.on_drain" event at tick 16000

    # link1 recurring
    When I advance to the next event
    Then the response is ok
    And now_tick is 15000
    And stopped_on_event is true
    And event is "link1.drain.on_drain"

    When I get state
    Then energy is 7
    And the scheduler has an "link1.drain.on_drain" event at tick 20000

    # link2 recurring
    When I advance to the next event
    Then the response is ok
    And now_tick is 16000
    And stopped_on_event is true
    And event is "link2.drain.on_drain"

    When I get state
    Then energy is 6
    And the scheduler has an "link2.drain.on_drain" event at tick 21000

  # ---------------------------------------------------------------------------
  # Argument validation
  # ---------------------------------------------------------------------------

  Scenario Outline: <description>
    Given emu starts with test script "<script>" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    When I get state
    Then the scheduler has <count> event(s)

    Examples:
      | description                                    | script                            | count |
      | event name of exactly 63 chars is accepted     | schedule_name_max/main.lua        | 1     |
      | event name of 64 chars is rejected             | schedule_name_too_long/main.lua   | 0     |
      | event name starting with underscore is rejected| schedule_name_underscore/main.lua | 0     |
      | negative delay_ms is rejected                  | schedule_delay_negative/main.lua  | 0     |
      | exhausting the event table is rejected         | schedule_table_full/main.lua      | 64    |
