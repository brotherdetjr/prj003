Feature: Global freeze — callbacks may not write outside rw

  Scenario: writing a new global inside a callback emits _on_lua_error
    Given emu starts with test script "global_write_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 0, "stop_on_event": true}
      """
    Then I receive a "_on_lua_error" SSE event with fn "on_start" and error containing "new_global"

  Scenario: adding a new field to a module table inside a callback emits _on_lua_error
    Given emu starts with test script "module_field_write_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 0, "stop_on_event": true}
      """
    Then I receive a "_on_lua_error" SSE event with fn "on_start" and error containing "new_field"

  Scenario: writing to rw directly inside _draw emits _on_lua_error and leaves rw intact
    Given emu starts with test script "draw_rw_write_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "rw write blocked in _draw"
    When I get state
    Then state field "rw" equals:
      """
      {"colour": 16711680}
      """

  Scenario: writing to a nested rw table inside _draw emits _on_lua_error and leaves rw intact
    Given emu starts with test script "draw_rw_nested_write_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "rw write blocked in _draw"
    When I get state
    Then state field "rw" equals:
      """
      {"stats": {"energy": 100}}
      """

  Scenario: deleting a key from rw inside _draw emits _on_lua_error and leaves rw intact
    Given emu starts with test script "draw_rw_delete_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "rw write blocked in _draw"
    When I get state
    Then state field "rw" equals:
      """
      {"colour": 16711680}
      """

  Scenario: deleting a key from a nested rw table inside _draw emits _on_lua_error and leaves rw intact
    Given emu starts with test script "draw_rw_nested_delete_test/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "rw write blocked in _draw"
    When I get state
    Then state field "rw" equals:
      """
      {"stats": {"energy": 100}}
      """
