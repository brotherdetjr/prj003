Feature: Diamond require — same module reached via two distinct paths

  Scenario: both require paths dispatch and reschedule independently
    Given emu starts with test script "diamond_test/main.lua" and args "--nowtick=0 --noautotick"
    When I spawn a character
    Then the response is ok
    And energy is 10
    And the scheduler has 2 event(s)
    And the scheduler has an "link1.drain.on_drain" event at tick 10000
    And the scheduler has an "link2.drain.on_drain" event at tick 11000

    When I advance to the next event
    Then the response is ok
    And now_tick is 10000
    And stopped_on_event is true
    And event is "link1.drain.on_drain"

    When I get state
    Then energy is 9
    And the scheduler has 2 event(s)
    And the scheduler has an "link2.drain.on_drain" event at tick 11000
    And the scheduler has an "link1.drain.on_drain" event at tick 15000

    When I advance to the next event
    Then the response is ok
    And now_tick is 11000
    And stopped_on_event is true
    And event is "link2.drain.on_drain"

    When I get state
    Then energy is 8
    And the scheduler has 2 event(s)
    And the scheduler has an "link1.drain.on_drain" event at tick 15000
    And the scheduler has an "link2.drain.on_drain" event at tick 16000

    # link1 recurring
    When I advance to the next event
    Then the response is ok
    And now_tick is 15000
    And stopped_on_event is true
    And event is "link1.drain.on_drain"

    When I get state
    Then energy is 7
    And the scheduler has an "link1.drain.on_drain" event at tick 20000

    # link2 recurring
    When I advance to the next event
    Then the response is ok
    And now_tick is 16000
    And stopped_on_event is true
    And event is "link2.drain.on_drain"

    When I get state
    Then energy is 6
    And the scheduler has an "link2.drain.on_drain" event at tick 21000
