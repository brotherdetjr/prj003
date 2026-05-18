Feature: Graphics

  Scenario: screen is black on startup when _draw is not defined
    Given emu starts with test script "init_ro_test/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "black_368x448.png"

  Scenario: cls fills the screen when called from _draw
    Given emu starts with test script "cls_in_draw/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "red_368x448.png"

  Scenario: cls raises an error when called outside _draw
    Given emu starts with test script "cls_on_spawn/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 0, "stop_on_event": true}
      """
    Then I receive a "_on_lua_error" SSE event with fn "on_start" and error containing "draw context"
