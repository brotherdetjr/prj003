Feature: Graphics

  Scenario: screen is black on startup
    Given emu starts with args "--noautotick"
    When I get the screen
    Then the screen matches fixture "black_368x448.png"

  Scenario: cls fills the screen when called from _draw
    Given emu starts with test script "cls_in_draw/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "red_368x448.png"

  Scenario: cls raises an error when called outside _draw
    Given emu starts with test script "cls_on_spawn/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I spawn a character
    Then I receive a "_on_lua_error" SSE event with fn "on_spawn" and error containing "draw context"
