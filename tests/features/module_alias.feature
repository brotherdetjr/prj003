Feature: Module alias — module loaded under a different variable name

  Scenario: init() schedules relative to the variable name, not the file name
    Given emu starts with test script "module_alias_test/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has an "nrg.on_drain" event at tick 5000

    When I advance to the next event
    Then the response is ok
    And stopped_on_event is true
    And event is "nrg.on_drain"

    When I get state
    Then energy is 9
    And the scheduler has an "nrg.on_drain" event at tick 10000
