Feature: _update() and _draw() Lua callbacks

  Scenario: _update() is called once per autotick
    Given emu starts with test script "update_draw_basic/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 200}
      """
    And I get state
    Then rw field "update_count" is 3

  Scenario: _draw() renders rw state set by _update() each tick
    Given emu starts with test script "update_draw_basic/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "red_368x448.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "green_368x448.png"

  Scenario: on_spawn state is visible to _update()
    Given emu starts with test script "update_draw_on_spawn/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    And I get state
    Then rw field "spawn_before_update" is true

  Scenario: _init runs first, then _update at tick 0, then events before _update each autotick
    Given emu starts with test script "update_draw_order/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 200}
      """
    And I get state
    Then rw field "order" equals:
      """
      ["_init", "_update", "on_myevent", "_update", "on_myevent", "_update"]
      """
