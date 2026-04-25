Feature: Lua require — callbacks in required modules

  Scenario: dotted callback names dispatch into required module tables
    Given emu starts with test script "require_test/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has an "drain.on_drain" event at tick 5000

    When I advance to the next event
    Then the response is ok
    And stopped_on_event is true
    And event is "drain.on_drain"

    When I get state
    Then energy is 9
    And the scheduler has an "drain.on_drain" event at tick 10000
