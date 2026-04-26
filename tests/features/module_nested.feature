Feature: Module nested — module mounted under a two-level global path

  Scenario: init() schedules relative to the full nested path
    Given emu starts with test script "module_nested_test/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has an "myapp.energy.on_drain" event at tick 5000

    When I advance to the next event
    Then the response is ok
    And stopped_on_event is true
    And event is "myapp.energy.on_drain"

    When I get state
    Then energy is 9
    And the scheduler has an "myapp.energy.on_drain" event at tick 10000
