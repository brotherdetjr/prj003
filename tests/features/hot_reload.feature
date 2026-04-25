Feature: Hot-reload — changes to Lua files in the script directory take effect without restart

  Scenario: replacing a required module updates the drain interval
    Given the hot-reload test directory is set up from "hot_reload_src"
    And "energy1.lua.template" is copied to "energy.lua"
    And emu starts with the hot-reload test script and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has an "energy.on_drain" event at tick 5000

    When I advance to the next event
    Then now_tick is 5000
    And event is "energy.on_drain"

    # energy2.lua.template has DRAIN_INTERVAL = 9000; the pre-scheduled event at
    # tick 10000 fires using the new code and reschedules at 10000 + 9000 = 19000
    When "energy2.lua.template" is copied to "energy.lua"
    Then I receive an SSE "_on_reload" event at now_tick 5000

    When I advance to the next event
    Then now_tick is 10000
    And event is "energy.on_drain"

    When I advance to the next event
    Then now_tick is 19000
    And event is "energy.on_drain"

  Scenario: touching an untracked Lua file does not trigger reload
    Given the hot-reload test directory is set up from "hot_reload_src"
    And "energy1.lua.template" is copied to "energy.lua"
    And emu starts with the hot-reload test script and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I spawn a character
    Then the response is ok
    When "decoy.lua.template" is copied to "decoy.lua"
    Then I do not receive an SSE "_on_reload" event
