Feature: Global freeze — callbacks may not write outside rw

  Scenario: writing a new global inside a callback emits _on_lua_error
    Given emu starts with test script "global_write_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I spawn a character
    Then I receive a "_on_lua_error" SSE event with fn "on_spawn" and error containing "new_global"

  Scenario: adding a new field to a module table inside a callback emits _on_lua_error
    Given emu starts with test script "module_field_write_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I spawn a character
    Then I receive a "_on_lua_error" SSE event with fn "on_spawn" and error containing "new_field"
