Feature: schedule argument validation

  Scenario: event name of exactly 63 chars is accepted
    Given emu starts with test script "schedule_name_max/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    When I get state
    Then the scheduler has 1 event(s)

  Scenario: event name of 64 chars is rejected with a Lua error
    Given emu starts with test script "schedule_name_too_long/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    When I get state
    Then the scheduler is an empty array

  Scenario: event name starting with underscore is rejected with a Lua error
    Given emu starts with test script "schedule_name_underscore/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    When I get state
    Then the scheduler is an empty array

  Scenario: negative delay_ms is rejected with a Lua error
    Given emu starts with test script "schedule_delay_negative/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    When I get state
    Then the scheduler is an empty array

  Scenario: exhausting the event table is rejected with a Lua error
    Given emu starts with test script "schedule_table_full/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    When I get state
    Then the scheduler has 64 event(s)
