Feature: Animation API

  Scenario: forward animation advances through two frames and stops
    Given emu starts with test script "anim_forward/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
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
    When I get the screen
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

  Scenario: pause() freezes animation on first frame
    Given emu starts with test script "anim_pause/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"

  Scenario: three-frame forward animation cycles red, green, blue then stops
    Given emu starts with test script "anim_three_forward/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
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
    Given emu starts with test script "anim_in_draw_error/main.lua" and args "--nowtick=0 --noautotick --wait-for-sse-client"
    And I subscribe to SSE events
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "anim:"

  Scenario: aspr() without of() raises a Lua error
    Given emu starts with test script "anim_aspr_without_of_error/main.lua" and args "--nowtick=0 --noautotick --wait-for-sse-client"
    And I subscribe to SSE events
    Then I receive a "_on_lua_error" SSE event with fn "_draw" and error containing "aspr:"

  Scenario: of() called twice raises a Lua error at script load time
    Given emu starts with test script "anim_of_twice_error/main.lua" and args "--nowtick=0 --noautotick --wait-for-sse-client"
    And I subscribe to SSE events
    Then I receive a "_on_lua_error" SSE event with fn "_init" and error containing "anim.of:"

  Scenario: get_state serializes animation next_frame
    Given emu starts with test script "anim_forward/main.lua" and args "--nowtick=0 --noautotick --wallclockutc=1970-01-01T00:00:00 --id=DEADBEEF"
    When I get state
    Then state equals:
      """
      {"ro": {"instance_id": "DEADBEEF", "now_tick": 0, "now_unix_sec": 0, "character": null}, "rw": {}, "scheduler": [], "anim": {"a": {"path": "{SCRIPTS_DIR}/anim_forward/two_frames.png", "n_frames": 2, "next_frame": 2, "backwards": false, "playing": true, "loop": false}}}
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
        "anim": {"a": {"path": "{SCRIPTS_DIR}/anim_forward/two_frames.png", "n_frames": 2, "next_frame": 1, "backwards": false, "playing": true, "loop": false}}
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
        "anim": {"a": {"path": "{SCRIPTS_DIR}/anim_forward/two_frames.png", "n_frames": 2, "next_frame": 2, "backwards": false, "playing": true, "loop": false}}
      }}
      """
    And I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame1.png"

  Scenario: animation instance is deleted after a draw in which it was not referenced
    Given emu starts with test script "anim_autodeletion/main.lua" and args "--nowtick=0 --noautotick --id=00000000"
    When I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "00000000", "now_tick": 0, "now_unix_sec": 0, "character": null},
        "rw": {"show": true}, "scheduler": [],
        "anim": {"a": {"path": "{SCRIPTS_DIR}/anim_autodeletion/two_frames.png", "n_frames": 2, "next_frame": 1, "backwards": false, "playing": true, "loop": false}}
      }}
      """
    And I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I post command:
      """
      {"cmd": "set_state", "state": {
        "ro": {"instance_id": "00000000", "now_tick": 100, "now_unix_sec": 0, "character": null},
        "rw": {"show": false}, "scheduler": [],
        "anim": {"a": {"path": "{SCRIPTS_DIR}/anim_autodeletion/two_frames.png", "n_frames": 2, "next_frame": 2, "backwards": false, "playing": true, "loop": false}}
      }}
      """
    And I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get state
    Then state equals:
      """
      {"ro": {"instance_id": "00000000", "now_tick": 200, "now_unix_sec": 0, "character": null}, "rw": {"show": false}, "scheduler": [], "anim": {}}
      """
