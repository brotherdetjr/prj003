Feature: --stop-on-lua-error — halt advance and disable autotick on Lua error

  Scenario: advance_time halts and reports lua_error when --stop-on-lua-error is set
    Given emu starts with test script "lua_error_in_event/main.lua" and args "--noautotick --stop-on-lua-error"
    And I subscribe to SSE events
    When I spawn a character
    And I advance 5000 ticks
    Then the response is ok
    And lua_error is true
    And stopped_on_event is false
    And I receive a "_on_lua_error" SSE event with fn "on_error_event" and error containing "bad_global"

  Scenario: autotick disables itself on Lua error when --stop-on-lua-error is set
    Given emu starts with test script "lua_error_in_event/main.lua" and args "--stop-on-lua-error"
    And I subscribe to SSE events
    When I spawn a character
    Then I receive a "_on_lua_error" SSE event with fn "on_error_event" and error containing "bad_global"
    When I get autotick
    Then autotick is false

  Scenario: get_stop_on_lua_error reflects --stop-on-lua-error flag
    Given emu starts with test script "lua_error_in_event/main.lua" and args "--noautotick --stop-on-lua-error"
    When I get stop_on_lua_error
    Then stop_on_lua_error is true

  Scenario: set_stop_on_lua_error toggles the setting at runtime
    Given emu starts with test script "lua_error_in_event/main.lua" and args "--noautotick"
    When I get stop_on_lua_error
    Then stop_on_lua_error is false
    When I set stop_on_lua_error to true
    Then stop_on_lua_error is true
    When I set stop_on_lua_error to false
    Then stop_on_lua_error is false
