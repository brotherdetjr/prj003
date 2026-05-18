Feature: Sprites

  Scenario: static PNG drawn at offset
    Given emu starts with test script "spr_static/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "spr_static.png"

  Scenario: alpha blending onto background
    Given emu starts with test script "spr_alpha/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "spr_alpha.png"

  Scenario: fragment draws only the selected region
    Given emu starts with test script "spr_fragment/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "spr_fragment.png"

  Scenario: APNG draws frame 0 by index
    Given emu starts with test script "spr_apng/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "spr_apng_frame0.png"

  Scenario: APNG draws frame 1 by index
    Given emu starts with test script "spr_apng_frame1/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "spr_apng_frame1.png"

  Scenario: spr clips correctly at negative x and y
    Given emu starts with test script "spr_neg_xy/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "spr_neg_xy.png"

  Scenario: aspr clips correctly at negative x and y
    Given emu starts with test script "aspr_neg_xy/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "aspr_neg_xy.png"

  Scenario: spr draws nothing when completely off-screen
    Given emu starts with test script "spr_oob/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "black_368x448.png"

  Scenario: aspr draws nothing when completely off-screen
    Given emu starts with test script "aspr_oob/main.lua" and args "--nowtick=0 --noautotick"
    When I get the screen
    Then the screen matches fixture "black_368x448.png"
