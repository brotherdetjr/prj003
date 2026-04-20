Feature: Lua scripting behaviour

  Scenario: scheduling an event with a reserved "_" prefix is rejected
    Given emu starts with test script "schedule_reserved.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    And I get state
    Then the scheduler is an empty array
