Feature: _init() Lua callback

  Scenario: _init() can read ro and write to rw
    Given emu starts with test script "init_ro_test/main.lua" and args "--nowtick=42 --noautotick"
    When I get state
    Then rw field "init_tick" is 42

  Scenario: _init() does not run on hot reload
    Given the hot-reload test directory is set up from "init_hot_reload_src"
    And emu starts with the hot-reload test script and args "--nowtick=10 --noautotick"
    And I subscribe to SSE events
    When I get state
    Then rw field "init_tick" is 10
    When "bad_init.lua.template" is copied to "main.lua"
    Then I receive an SSE "_on_reload" event at now_tick 10
    When I get state
    Then rw field "init_tick" is 10
