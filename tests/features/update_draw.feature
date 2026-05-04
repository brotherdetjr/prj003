Feature: _update() and _draw() Lua callbacks

  Scenario: _update() is called once per autotick
    Given emu starts with test script "update_draw_basic/main.lua" and args "--nowtick=0 --noautotick"
    When I advance 200 ticks
    And I get state
    Then rw field "update_count" is 2

  Scenario: _draw() is called once per autotick and renders to screen
    Given emu starts with test script "update_draw_basic/main.lua" and args "--nowtick=0 --noautotick"
    When I advance 200 ticks
    And I get state
    And I get the screen
    Then rw field "draw_count" is 2
    And the screen matches fixture "green_368x448.png"

  Scenario: on_spawn state is visible to _update()
    Given emu starts with test script "update_draw_on_spawn/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    And I advance 100 ticks
    And I get state
    Then rw field "spawn_before_update" is true
