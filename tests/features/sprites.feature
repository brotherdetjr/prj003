Feature: Sprites

  Scenario: static PNG drawn at offset
    Given emu starts with test script "spr_static/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_static.png"

  Scenario: alpha blending onto background
    Given emu starts with test script "spr_alpha/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_alpha.png"

  Scenario: fragment draws only the selected region
    Given emu starts with test script "spr_fragment/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_fragment.png"

  Scenario: APNG draws frame 0 by index
    Given emu starts with test script "spr_apng/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"

  Scenario: APNG draws frame 1 by index
    Given emu starts with test script "spr_apng_frame1/main.lua" and args "--nowtick=0 --noautotick"
    When I post command:
      """
      {"cmd": "advance_time", "ticks": 100}
      """
    And I get the screen
    Then the screen matches fixture "spr_apng_frame1.png"
