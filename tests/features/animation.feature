Feature: Animation API

  Scenario: forward animation advances through two frames and stops
    Given emu starts with test script "anim_forward/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame1.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame1.png"

  Scenario: forward loop wraps back to first frame
    Given emu starts with test script "anim_loop/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame1.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"

  Scenario: stop() freezes animation on first frame
    Given emu starts with test script "anim_stop/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"

  Scenario: three-frame forward animation cycles red, green, blue then stops
    Given emu starts with test script "anim_three_forward/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "anim_three_red.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "anim_three_green.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "anim_three_blue.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "anim_three_blue.png"

  Scenario: anim() inside _draw raises a Lua error
    Given emu starts with test script "anim_in_draw_error/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "anim:"

  Scenario: aspr() without of() raises a Lua error
    Given emu starts with test script "anim_fr_without_of_error/main.lua" and args "--nowtick=0 --noautotick"
    And I subscribe to SSE events
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "aspr:"

  Scenario: of() called twice raises a Lua error at script load time
    When emu is invoked with args "--nowtick=0 --noautotick --script=scripts/anim_of_twice_error/main.lua"
    Then the exit code is 1

  Scenario: get_state serializes animation current_frame
    Given emu starts with test script "anim_forward/main.lua" and args "--nowtick=0 --noautotick --id=DEADBEEF"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get state
    Then state field "anim" equals:
      """
      {"a": {"path": "{SCRIPTS_DIR}/anim_forward/two_frames.png", "n_frames": 2, "current_frame": 2, "backwards": false, "playing": true, "loop": false}}
      """

  Scenario: set_state restores animation frame
    Given emu starts with test script "anim_forward/main.lua" and args "--nowtick=0 --noautotick --id=DEADBEEF"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 100, "now_unix_sec": 0, "character": null},
        "rw": {}, "scheduler": [],
        "anim": {"a": {"path": "{SCRIPTS_DIR}/anim_forward/two_frames.png", "n_frames": 2, "current_frame": 1, "backwards": false, "playing": true, "loop": false}}
      }}
      """
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"

  Scenario: set_state draws correct frame on first draw with no prior animation
    Given emu starts with test script "anim_forward/main.lua" and args "--nowtick=0 --noautotick --id=DEADBEEF"
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "DEADBEEF", "now_tick": 0, "now_unix_sec": 0, "character": null},
        "rw": {}, "scheduler": [],
        "anim": {"a": {"path": "{SCRIPTS_DIR}/anim_forward/two_frames.png", "n_frames": 2, "current_frame": 2, "backwards": false, "playing": true, "loop": false}}
      }}
      """
    And I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame1.png"
